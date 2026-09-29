#pragma once
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

// Real-time vocal pitch correction ("auto-tune" style).
//
//  * Detection: YIN pitch tracker (de Cheveigne & Kawahara 2002) on a ~24 kHz
//    decimated copy of the voice, updated every 4 ms.
//  * Target: nearest note of the chosen key + scale (with a little hysteresis
//    so it doesn't flip between two notes when you sing between them).
//  * Retune speed: how quickly the correction moves to the target.
//    0 ms = instant snap (the hard "robot" effect); 20-50 ms = natural
//    modern pop/rap tuning; 100+ ms = only sustained notes get pulled in and
//    vibrato passes through.
//  * Shifting: pitch-synchronous overlap-add (TD-PSOLA). Two-period grains are
//    re-spaced at the corrected period, which keeps the voice's formants
//    (no chipmunk effect) and works per channel with shared pitch marks.
//
// Latency is fixed (2.5 x the longest period + a little), so the host's delay
// compensation stays correct whether tuning is on or off.
class PitchCorrector
{
public:
    struct Params
    {
        bool  enabled = false;
        float amount = 1.0f;     // 0..1 of the correction applied
        float speedMs = 25.0f;   // retune speed
        int   key = 0;           // 0 = C ... 11 = B
        int   scale = 0;         // see scaleNames()
    };

    static juce::StringArray keyNames()
    {
        return { "C", "C#/Db", "D", "D#/Eb", "E", "F", "F#/Gb", "G", "G#/Ab", "A", "A#/Bb", "B" };
    }

    static juce::StringArray scaleNames()
    {
        return { "Chromatic (any key)", "Major", "Minor", "Harmonic Minor", "Major Pentatonic", "Minor Pentatonic",
                 "Bhairav (S r G m P d N)", "Kafi / Dorian", "Bhairavi / Phrygian", "Yaman / Lydian" };
    }

    // semitone steps of each scale, relative to the key
    static std::array<bool, 12> scaleMask (int scale, int key)
    {
        static const std::vector<std::vector<int>> steps {
            { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 },
            { 0, 2, 4, 5, 7, 9, 11 },       // major (Bilawal)
            { 0, 2, 3, 5, 7, 8, 10 },       // natural minor (Asavari)
            { 0, 2, 3, 5, 7, 8, 11 },       // harmonic minor
            { 0, 2, 4, 7, 9 },              // major pentatonic (Bhupali)
            { 0, 3, 5, 7, 10 },             // minor pentatonic (Malkauns-like family)
            { 0, 1, 4, 5, 7, 8, 11 },       // Bhairav thaat (double harmonic)
            { 0, 2, 3, 5, 7, 9, 10 },       // Kafi thaat (Dorian)
            { 0, 1, 3, 5, 7, 8, 10 },       // Bhairavi thaat (Phrygian)
            { 0, 2, 4, 6, 7, 9, 11 },       // Kalyan / Yaman thaat (Lydian)
        };
        std::array<bool, 12> m {};
        const auto& s = steps[(size_t) juce::jlimit (0, (int) steps.size() - 1, scale)];
        for (int st : s) m[(size_t) ((st + key) % 12)] = true;
        return m;
    }

    void prepare (double sr, int numChannels)
    {
        sampleRate = sr;
        chans = juce::jlimit (1, 2, numChannels);

        decim = juce::jmax (1, (int) std::lround (sr / 24000.0));
        dsr = sr / decim;
        pMax = (int) std::ceil (sr / minHz);
        pMin = (int) std::floor (sr / maxHz);
        latency = (int) std::ceil (2.5 * pMax) + 16;
        pDefault = juce::jmin (pMax, (int) std::lround (0.006 * sr));
        hop = juce::jmax (32, (int) std::lround (0.004 * sr));

        size = juce::nextPowerOfTwo (latency + 4 * pMax + 4096);
        mask = size - 1;
        for (auto& b : in)  b.assign ((size_t) size, 0.0f);
        for (auto& b : out) b.assign ((size_t) size, 0.0f);
        wsum.assign ((size_t) size, 0.0f);

        yinW = (int) std::lround (0.020 * dsr);
        yinTauMax = (int) std::ceil (dsr / minHz);
        yinTauMin = juce::jmax (2, (int) std::floor (dsr / maxHz));
        dsize = juce::nextPowerOfTwo (yinW + yinTauMax + 64);
        dmask = dsize - 1;
        dec.assign ((size_t) dsize, 0.0f);
        yinBuf.assign ((size_t) (yinW + yinTauMax + 1), 0.0f);
        diff.assign ((size_t) (yinTauMax + 2), 0.0f);

        // anti-alias before decimating the detector copy (the audio itself is never decimated)
        auto lp = juce::dsp::IIR::Coefficients<float>::makeLowPass (sr, (float) juce::jmin (0.45 * dsr, 9000.0));
        aa1.coefficients = lp; aa2.coefficients = lp;
        reset();
    }

    void reset()
    {
        for (auto& b : in)  std::fill (b.begin(), b.end(), 0.0f);
        for (auto& b : out) std::fill (b.begin(), b.end(), 0.0f);
        std::fill (wsum.begin(), wsum.end(), 0.0f);
        std::fill (dec.begin(), dec.end(), 0.0f);
        aa1.reset(); aa2.reset();
        n = 0; dw = 0; decCount = 0; hopCount = 0;
        s = 0.0; a = 0; period = pDefault;
        corr = 0.0f; voiced = false; targetNote = -1;
        history.fill (0.0f); histCount = 0;
        wet = 0.0f;
        meterDetected.store (-1.0f); meterTarget.store (-1.0f); meterCents.store (0.0f);
    }

    int getLatencySamples() const { return latency; }
    void setParams (const Params& p) { params = p; }

    void process (juce::AudioBuffer<float>& b)
    {
        const int num = b.getNumSamples();
        const int c = juce::jmin (chans, b.getNumChannels());
        const float wetStep = 1.0f / (0.03f * (float) sampleRate);
        const bool on = params.enabled && params.amount > 0.0005f;

        for (int i = 0; i < num; ++i)
        {
            float mono = 0.0f;
            for (int ch = 0; ch < c; ++ch)
            {
                const float x = b.getSample (ch, i);
                in[(size_t) ch][(size_t) (n & mask)] = x;
                mono += x;
            }
            mono /= (float) c;

            const float lp = aa2.processSample (aa1.processSample (mono));
            if (++decCount >= decim) { decCount = 0; dec[(size_t) (dw & dmask)] = lp; ++dw; }

            ++n; // samples [0, n) are now available

            if (++hopCount >= hop) { hopCount = 0; analyse(); }

            // place every grain whose input is fully available
            while (s + 1.5 * pMax + 2.0 <= (double) n)
                placeGrain (c);

            // emit sample (n - 1 - latency): dry path is an exact delay, wet path is the PSOLA output
            const int64_t m = n - 1 - latency;
            wet = on ? juce::jmin (1.0f, wet + wetStep) : juce::jmax (0.0f, wet - wetStep);
            const size_t mi = (size_t) (m & mask);
            const float norm = 1.0f / juce::jmax (0.5f, wsum[mi]);
            for (int ch = 0; ch < c; ++ch)
            {
                const float dry = m >= 0 ? in[(size_t) ch][mi] : 0.0f;
                const float shifted = out[(size_t) ch][mi] * norm;
                out[(size_t) ch][mi] = 0.0f;
                b.setSample (ch, i, wet <= 0.0f ? dry : dry + wet * (shifted - dry));
            }
            wsum[mi] = 0.0f;
        }
    }

    // ---- for the interface ----
    float getDetectedMidi() const { return meterDetected.load (std::memory_order_relaxed); } // -1 = no pitch
    float getTargetMidi() const   { return meterTarget.load (std::memory_order_relaxed); }
    float getCorrectionCents() const { return meterCents.load (std::memory_order_relaxed); }

private:
    void analyse()
    {
        const int len = yinW + yinTauMax;
        if (dw < len) return;
        double energy = 0.0;
        for (int j = 0; j < len; ++j)
        {
            const float v = dec[(size_t) ((dw - len + j) & dmask)];
            yinBuf[(size_t) j] = v;
            if (j < yinW) energy += (double) v * v;
        }
        const float rmsDb = 10.0f * (float) std::log10 (energy / yinW + 1e-20);

        float f0 = 0.0f;
        if (rmsDb > -62.0f)
        {
            // difference function + cumulative mean normalisation
            diff[0] = 1.0f;
            double running = 0.0;
            for (int tau = 1; tau <= yinTauMax; ++tau)
            {
                double d = 0.0;
                const float* x = yinBuf.data();
                for (int j = 0; j < yinW; ++j) { const float e = x[j] - x[j + tau]; d += (double) e * e; }
                running += d;
                diff[(size_t) tau] = running > 0.0 ? (float) (d * tau / running) : 1.0f;
            }
            int best = -1;
            for (int tau = yinTauMin; tau <= yinTauMax; ++tau)
            {
                if (diff[(size_t) tau] < 0.15f)
                {
                    while (tau + 1 <= yinTauMax && diff[(size_t) (tau + 1)] < diff[(size_t) tau]) ++tau;
                    best = tau;
                    break;
                }
            }
            if (best < 0) // no clear dip: take the global minimum if it's reasonably periodic
            {
                int mi = yinTauMin;
                for (int tau = yinTauMin; tau <= yinTauMax; ++tau) if (diff[(size_t) tau] < diff[(size_t) mi]) mi = tau;
                if (diff[(size_t) mi] < 0.3f) best = mi;
            }
            if (best > yinTauMin && best < yinTauMax)
            {
                const float y0 = diff[(size_t) (best - 1)], y1 = diff[(size_t) best], y2 = diff[(size_t) (best + 1)];
                const float den = y0 - 2.0f * y1 + y2;
                const float shift = std::abs (den) > 1e-9f ? 0.5f * (y0 - y2) / den : 0.0f;
                f0 = (float) (dsr / (best + juce::jlimit (-0.5f, 0.5f, shift)));
            }
        }

        // 3-point median against octave slips
        if (f0 > 0.0f)
        {
            history[(size_t) (histCount++ % 3)] = f0;
            if (histCount >= 3)
            {
                auto h = history;
                std::sort (h.begin(), h.end());
                // only step in when this estimate jumped away from its neighbours (an octave slip);
                // otherwise use it as-is so tracking has no extra lag
                if (std::abs (std::log2 (f0 / h[1])) > 0.35f) f0 = h[1];
            }
        }
        else histCount = 0;

        const float hopSec = (float) hop / (float) sampleRate;
        if (f0 > 0.0f)
        {
            voiced = true;
            const float midi = 69.0f + 12.0f * std::log2 (f0 / 440.0f);
            const auto allowed = scaleMask (params.scale, params.key);

            // keep the current note unless we've clearly moved to another one
            bool keep = targetNote >= 0 && allowed[(size_t) (targetNote % 12)] && std::abs (midi - (float) targetNote) < 0.62f;
            if (! keep)
            {
                int bestNote = -1; float bestDist = 1e9f;
                const int base = (int) std::floor (midi);
                for (int cand = base - 6; cand <= base + 7; ++cand)
                {
                    if (cand < 0 || ! allowed[(size_t) (cand % 12)]) continue;
                    const float d = std::abs (midi - (float) cand);
                    if (d < bestDist) { bestDist = d; bestNote = cand; }
                }
                targetNote = bestNote;
            }
            const float want = targetNote >= 0 ? ((float) targetNote - midi) * 100.0f * juce::jlimit (0.0f, 1.0f, params.amount) : 0.0f;
            const float speed = juce::jmax (0.0f, params.speedMs) * 0.001f;
            const float alpha = speed <= 0.0005f ? 1.0f : 1.0f - std::exp (-hopSec / speed);
            corr += alpha * (want - corr);
            period = juce::jlimit (pMin, pMax, (int) std::lround (sampleRate / f0));
            meterDetected.store (midi); meterTarget.store ((float) targetNote); meterCents.store (want);
        }
        else
        {
            voiced = false;
            targetNote = -1;
            corr += (1.0f - std::exp (-hopSec / 0.015f)) * (0.0f - corr);
            period = pDefault;
            meterDetected.store (-1.0f); meterTarget.store (-1.0f); meterCents.store (0.0f);
        }
        corr = juce::jlimit (-700.0f, 700.0f, corr);
    }

    void placeGrain (int c)
    {
        const int P = period;
        const double ratio = std::pow (2.0, (double) corr / 1200.0);
        const int64_t centre = (int64_t) std::llround (s);

        // analysis mark: the period-spaced mark nearest the synthesis position
        while ((double) a + 0.5 * P < s) a += P;
        while ((double) a - 0.5 * P > s) a -= P;

        const float twoPiOver = juce::MathConstants<float>::twoPi / (float) (2 * P);
        for (int k = -P; k < P; ++k)
        {
            const float w = 0.5f - 0.5f * std::cos (twoPiOver * (float) (k + P));
            const size_t oi = (size_t) ((centre + k) & mask);
            const size_t ii = (size_t) ((a + k) & mask);
            for (int ch = 0; ch < c; ++ch)
                out[(size_t) ch][oi] += in[(size_t) ch][ii] * w;
            wsum[oi] += w;
        }
        s += (double) P / ratio;
    }

    static constexpr double minHz = 75.0, maxHz = 1050.0;

    Params params;
    double sampleRate = 44100.0, dsr = 22050.0;
    int chans = 2, decim = 2, pMax = 640, pMin = 42, latency = 1616, pDefault = 288, hop = 256;
    int size = 8192, mask = 8191, dsize = 1024, dmask = 1023;
    int yinW = 480, yinTauMax = 320, yinTauMin = 22;

    std::array<std::vector<float>, 2> in, out;
    std::vector<float> wsum, dec, yinBuf, diff;
    juce::dsp::IIR::Filter<float> aa1, aa2;

    int64_t n = 0, dw = 0, a = 0;
    double s = 0.0;
    int decCount = 0, hopCount = 0, period = 288, targetNote = -1, histCount = 0;
    float corr = 0.0f, wet = 0.0f;
    bool voiced = false;
    std::array<float, 3> history {};

    std::atomic<float> meterDetected { -1.0f }, meterTarget { -1.0f }, meterCents { 0.0f };
};
