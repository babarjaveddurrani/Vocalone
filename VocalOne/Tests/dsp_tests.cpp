// Headless tests for the VocalOne DSP chain (no DAW / GUI needed).
#include <juce_audio_basics/juce_audio_basics.h>
#include "../Source/Params.h"
#include <cstdio>
#include <cmath>

static int failures = 0;
static void check (bool ok, const char* what)
{
    std::printf ("   %s  %s\n", ok ? "ok  " : "FAIL", what);
    if (! ok) ++failures;
}

static constexpr double SR = 48000.0;
static const double twoPi = juce::MathConstants<double>::twoPi;

static float rmsDb (const juce::AudioBuffer<float>& b, int ch, int a, int e)
{
    double s = 0; for (int i = a; i < e; ++i) s += (double) b.getSample (ch, i) * b.getSample (ch, i);
    return (float) (10.0 * std::log10 (s / juce::jmax (1, e - a) + 1e-20));
}
static float peakAbs (const juce::AudioBuffer<float>& b)
{
    float m = 0; for (int ch = 0; ch < b.getNumChannels(); ++ch) m = juce::jmax (m, b.getMagnitude (ch, 0, b.getNumSamples()));
    return m;
}
static bool finite (const juce::AudioBuffer<float>& b)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i) if (! std::isfinite (b.getSample (ch, i))) return false;
    return true;
}
template <typename F> static void inBlocks (juce::AudioBuffer<float>& b, F&& f, int block = 512)
{
    for (int s = 0; s < b.getNumSamples(); s += block)
    {
        const int n = juce::jmin (block, b.getNumSamples() - s);
        juce::AudioBuffer<float> chunk (b.getArrayOfWritePointers(), b.getNumChannels(), s, n);
        f (chunk);
    }
}
static void fillSine (juce::AudioBuffer<float>& b, double hz, float amp, int a = 0, int e = -1)
{
    if (e < 0) e = b.getNumSamples();
    for (int i = a; i < e; ++i)
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            b.setSample (ch, i, b.getSample (ch, i) + amp * (float) std::sin (twoPi * hz * i / SR));
}

// Synthetic "vocal": harmonic vowel bursts with a sibilant hiss at the start of each,
// sitting on room noise. level scales the whole thing (to test loud vs quiet recordings).
static juce::AudioBuffer<float> fakeVocal (double secs, float level, bool withNoise = true)
{
    const int n = (int) (secs * SR);
    juce::AudioBuffer<float> b (2, n);
    b.clear();
    juce::Random rng (99);
    juce::dsp::IIR::Filter<float> hp;
    hp.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (SR, 5000.0f);
    const int phrase = (int) (0.8 * SR);
    for (int i = 0; i < n; ++i)
    {
        const int k = i / phrase, pos = i % phrase;
        float v = 0.0f;
        if (k % 2 == 0)
        {
            const double f0 = 196.0 * (1.0 + 0.02 * std::sin (twoPi * 5.0 * i / SR)); // vibrato
            for (int h = 1; h <= 12; ++h) v += (0.3f / (float) h) * (float) std::sin (twoPi * f0 * h * i / SR);
            if (pos < (int) (0.08 * SR)) v += 0.6f * hp.processSample (rng.nextFloat() * 2.0f - 1.0f); // "s"
        }
        float noise = withNoise ? 0.004f * (rng.nextFloat() * 2.0f - 1.0f) : 0.0f;
        b.setSample (0, i, level * (v + noise));
        b.setSample (1, i, level * (v + noise));
    }
    return b;
}


// ---------------- pitch helpers ----------------
// harmonic "voice" following a pitch curve (Hz as a function of time)
template <typename F> static juce::AudioBuffer<float> sungTone (double secs, F&& hzAt, float amp = 0.3f)
{
    const int n = (int) (secs * SR);
    juce::AudioBuffer<float> b (2, n);
    double ph = 0.0;
    for (int i = 0; i < n; ++i)
    {
        ph += hzAt (i / SR) / SR;
        float v = 0.0f;
        for (int h = 1; h <= 14; ++h) v += (float) (std::sin (twoPi * ph * h) / (h * 0.9 + 0.3));
        v *= amp * 0.35f;
        b.setSample (0, i, v); b.setSample (1, i, v);
    }
    return b;
}
// offline YIN (full rate) -> pitch in Hz at the centre of [start, start + 2048)
static float measureHz (const juce::AudioBuffer<float>& b, int start)
{
    const int W = 1200, tauMax = 700;
    if (start + W + tauMax >= b.getNumSamples()) return 0.0f;
    std::vector<double> d ((size_t) tauMax + 1, 0.0);
    const float* x = b.getReadPointer (0) + start;
    double run = 0.0; int best = -1;
    std::vector<double> dn ((size_t) tauMax + 1, 1.0);
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        double s = 0; for (int j = 0; j < W; ++j) { const double e = x[j] - x[j + tau]; s += e * e; }
        run += s; dn[(size_t) tau] = s * tau / (run + 1e-20);
    }
    for (int tau = 40; tau < tauMax; ++tau)
        if (dn[(size_t) tau] < 0.12) { while (tau + 1 < tauMax && dn[(size_t) tau + 1] < dn[(size_t) tau]) ++tau; best = tau; break; }
    if (best < 0) return 0.0f;
    const double y0 = dn[(size_t) best - 1], y1 = dn[(size_t) best], y2 = dn[(size_t) best + 1];
    const double den = y0 - 2 * y1 + y2;
    const double sh = std::abs (den) > 1e-12 ? 0.5 * (y0 - y2) / den : 0.0;
    return (float) (SR / (best + sh));
}
static float centsBetween (float f, float ref) { return 1200.0f * std::log2 (f / ref); }
static float midiHz (int midi) { return 440.0f * std::pow (2.0f, (midi - 69) / 12.0f); }
template <typename F> static void inBlocksT (juce::AudioBuffer<float>& b, F&& f) { inBlocks (b, f, 480); }

int main()
{
    // ---------------- Resonance ----------------
    std::printf ("Resonance suppressor\n");
    {
        ResonanceSuppressor rs; rs.prepare (SR, 2);
        ResonanceSuppressor::Params p; p.depth = 0.9f; p.sensitivity = 0.7f; p.maxCutDb = 18.0f; rs.setParams (p);
        juce::AudioBuffer<float> b (2, (int) (SR * 3)); b.clear();
        juce::Random rng (42);
        for (int i = 0; i < b.getNumSamples(); ++i) for (int ch = 0; ch < 2; ++ch) b.setSample (ch, i, 0.05f * (rng.nextFloat() * 2 - 1));
        fillSine (b, 3000.0, 0.5f);
        inBlocks (b, [&] (auto& c) { rs.process (c); });
        double re = 0, im = 0; const int a = b.getNumSamples() / 2, n = b.getNumSamples() - a;
        for (int i = 0; i < n; ++i) { re += b.getSample (0, a + i) * std::cos (twoPi * 3000 * i / SR); im -= b.getSample (0, a + i) * std::sin (twoPi * 3000 * i / SR); }
        const double db = 20 * std::log10 (std::sqrt (re * re + im * im) / n * 2 + 1e-12);
        std::printf ("   3 kHz ringing peak: -6 dB in -> %.1f dB out\n", db);
        check (db < -12.0, "harsh resonance pulled down");
    }

    // ---------------- Noise ----------------
    std::printf ("Noise reduction\n");
    {
        auto run = [] (bool adaptive, float& gapDb)
        {
            NoiseReducer nr; nr.prepare (SR, 2);
            auto b = fakeVocal (8.0, 1.0f);
            // first 2 s: noise only (overwrite vocal)
            juce::Random rng (5);
            for (int i = 0; i < (int) (2 * SR); ++i) for (int ch = 0; ch < 2; ++ch) b.setSample (ch, i, 0.004f * (rng.nextFloat() * 2 - 1));
            auto in = b;
            int pos = 0;
            inBlocks (b, [&] (auto& c) {
                NoiseReducer::Params p; p.reductionDb = 18; p.adaptive = adaptive; p.learn = ! adaptive && pos < (int) (1.5 * SR);
                nr.setParams (p); nr.process (c); pos += c.getNumSamples(); });
            // gap between phrases 3 (0.8s*3=2.4s..3.2s) -> measure 2.6..3.0 s
            const int lat = NoiseReducer::getLatencySamples();
            gapDb = rmsDb (b, 0, (int) (2.6 * SR) + lat, (int) (3.0 * SR) + lat) - rmsDb (in, 0, (int) (2.6 * SR), (int) (3.0 * SR));
        };
        float learned, adaptive;
        run (false, learned); run (true, adaptive);
        std::printf ("   room noise between lines: learned %.1f dB, auto %.1f dB\n", learned, adaptive);
        check (learned < -10.0f, "learned noise print removes room noise");
        check (adaptive < -8.0f, "auto mode removes room noise");
    }

    // ---------------- Auto level ----------------
    std::printf ("Auto level\n");
    for (float inDb : { -24.0f, -8.0f })
    {
        AutoLeveler lv; lv.prepare (SR);
        juce::AudioBuffer<float> b (2, (int) (SR * 5)); b.clear();
        fillSine (b, 300.0, juce::Decibels::decibelsToGain (inDb + 3.01f)); // sine RMS = amp - 3 dB
        inBlocks (b, [&] (auto& c) { lv.process (c); });
        const float out = rmsDb (b, 0, (int) (4 * SR), (int) (5 * SR));
        std::printf ("   %.0f dB in -> %.1f dB out (target -18)\n", inDb, out);
        check (std::abs (out + 18.0f) < 1.5f, "brought to target level");
    }

    // ---------------- Compressor ----------------
    std::printf ("Compressor\n");
    {
        auto run = [] (float amount)
        {
            Compressor c; c.prepare (SR); Compressor::Params p; p.amount = amount; c.setParams (p);
            juce::AudioBuffer<float> b (2, (int) (SR * 6)); b.clear();
            const int seg = (int) (0.5 * SR);
            for (int s = 0; s < 12; ++s) fillSine (b, 250.0, juce::Decibels::decibelsToGain (s % 2 ? -26.0f : -8.0f), s * seg, (s + 1) * seg);
            inBlocks (b, [&] (auto& x) { c.process (x); });
            return rmsDb (b, 0, 10 * seg + seg / 2, 11 * seg) - rmsDb (b, 0, 11 * seg + seg / 2, 12 * seg);
        };
        const float r0 = run (0.0f), r7 = run (0.7f);
        std::printf ("   loud-vs-quiet gap: %.1f dB at 0, %.1f dB at 0.7\n", r0, r7);
        check (std::abs (r0 - 18.0f) < 0.3f, "amount 0 leaves dynamics alone");
        check (r7 < r0 - 6.0f, "compression evens out loud and quiet words");
    }

    // ---------------- De-esser ----------------
    std::printf ("De-esser (level independent)\n");
    {
        auto run = [] (float level, float& sibCut, float& vowelChange)
        {
            DeEsser de; de.prepare (SR, 2); DeEsser::Params p; p.amount = 0.6f; de.setParams (p);
            auto b = fakeVocal (3.2, level, false);
            auto in = b;
            inBlocks (b, [&] (auto& c) { de.process (c); });
            juce::dsp::IIR::Filter<float> f1, f2;
            f1.coefficients = f2.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (SR, 6000.0f);
            auto bandDb = [] (juce::AudioBuffer<float>& x, juce::dsp::IIR::Filter<float>& f, int a, int e)
            { double s = 0; for (int i = 0; i < e; ++i) { float y = f.processSample (x.getSample (0, i)); if (i >= a) s += y * y; } return 10 * std::log10 (s / (e - a) + 1e-20); };
            const int s0 = (int) (1.6 * SR) + 400, s1 = (int) (1.6 * SR + 0.08 * SR);
            sibCut = (float) (bandDb (b, f1, s0, s1) - bandDb (in, f2, s0, s1));
            vowelChange = rmsDb (b, 0, (int) (1.9 * SR), (int) (2.3 * SR)) - rmsDb (in, 0, (int) (1.9 * SR), (int) (2.3 * SR));
        };
        float sLoud, vLoud, sQuiet, vQuiet;
        run (1.0f, sLoud, vLoud); run (0.05f, sQuiet, vQuiet);
        std::printf ("   loud take : 's' band %.1f dB, vowels %.2f dB\n", sLoud, vLoud);
        std::printf ("   quiet take: 's' band %.1f dB, vowels %.2f dB\n", sQuiet, vQuiet);
        check (sLoud < -4.0f, "sibilance reduced");
        check (std::abs (vLoud) < 0.5f, "vowels untouched");
        check (std::abs (sLoud - sQuiet) < 2.0f, "works the same on a quiet recording");
    }

    // ---------------- Tone EQ ----------------
    std::printf ("Tone EQ\n");
    {
        struct Case { float hz; ToneEQ::Params p; float expect; };
        ToneEQ::Params pres; pres.presenceDb = 6; pres.airDb = 0; pres.clarity = 0; pres.warmthDb = 0;
        ToneEQ::Params mud;  mud.presenceDb = 0;  mud.airDb = 0;  mud.clarity = 1; mud.warmthDb = 0;
        ToneEQ::Params air;  air.presenceDb = 0;  air.airDb = 8;  air.clarity = 0; air.warmthDb = 0;
        ToneEQ::Params warm; warm.presenceDb = 0; warm.airDb = 0; warm.clarity = 0; warm.warmthDb = 5;
        for (auto& cs : { Case { 3500, pres, 6 }, Case { 320, mud, -8 }, Case { 16000, air, 8 }, Case { 60, warm, 5 } })
        {
            ToneEQ eq; eq.prepare (SR, 2); eq.setParams (cs.p);
            juce::AudioBuffer<float> b (2, (int) SR); b.clear(); fillSine (b, cs.hz, 0.1f);
            const float in = rmsDb (b, 0, (int) (0.5 * SR), (int) SR);
            eq.process (b);
            const float d = rmsDb (b, 0, (int) (0.5 * SR), (int) SR) - in;
            std::printf ("   %6.0f Hz: %+.1f dB (want %+.0f)\n", cs.hz, d, cs.expect);
            check (std::abs (d - cs.expect) < 1.0f, "band gain correct");
        }
    }

    // ---------------- Saturation ----------------
    std::printf ("Saturation\n");
    {
        Saturation sat; sat.prepare (SR, 512, 2); sat.setAmount (0.8f);
        juce::AudioBuffer<float> b (2, (int) SR); b.clear(); fillSine (b, 1000.0, 0.5f);
        inBlocks (b, [&] (auto& c) { sat.process (c); });
        double h1r = 0, h1i = 0, h2r = 0, h2i = 0, h3r = 0, h3i = 0;
        for (int i = (int) (0.5 * SR); i < (int) SR; ++i)
        {
            const double x = b.getSample (0, i);
            h1r += x * std::cos (twoPi * 1000 * i / SR); h1i += x * std::sin (twoPi * 1000 * i / SR);
            h2r += x * std::cos (twoPi * 2000 * i / SR); h2i += x * std::sin (twoPi * 2000 * i / SR);
            h3r += x * std::cos (twoPi * 3000 * i / SR); h3i += x * std::sin (twoPi * 3000 * i / SR);
        }
        const double h1 = std::hypot (h1r, h1i), h2 = std::hypot (h2r, h2i), h3 = std::hypot (h3r, h3i);
        std::printf ("   harmonics: 2nd %.1f dB, 3rd %.1f dB below fundamental; latency %d\n", 20 * std::log10 (h1 / h2), 20 * std::log10 (h1 / h3), sat.getLatencySamples());
        check (finite (b), "no invalid samples");
        check (h2 / h1 > 0.003 && h3 / h1 > 0.01, "adds warm even + odd harmonics");
    }

    // ---------------- Limiter ----------------
    std::printf ("Limiter\n");
    {
        Limiter lim; lim.prepare (SR, 2); Limiter::Params p; p.ceilingDb = -1.0f; lim.setParams (p);
        juce::AudioBuffer<float> b (2, (int) (SR * 2)); juce::Random rng (3);
        for (int i = 0; i < b.getNumSamples(); ++i) for (int ch = 0; ch < 2; ++ch) b.setSample (ch, i, 4.0f * (rng.nextFloat() * 2 - 1) * (i % 9000 < 300 ? 1.0f : 0.1f));
        inBlocks (b, [&] (auto& c) { lim.process (c); });
        const float pk = juce::Decibels::gainToDecibels (peakAbs (b));
        std::printf ("   +12 dB spikes in -> peak %.2f dB out (ceiling -1)\n", pk);
        check (pk <= -0.99f, "never passes the ceiling");
    }

    // ---------------- Echo timing ----------------
    std::printf ("Echo\n");
    {
        EchoDelay e; e.prepare (SR, 512);
        EchoDelay::Params p; p.division = 2; p.bpm = 120; p.feedback = 0; p.pingPong = false; e.setParams (p);
        std::vector<float> in ((size_t) SR, 0.0f), l ((size_t) SR), r ((size_t) SR);
        // let the delay-time smoother settle first
        std::vector<float> warm ((size_t) SR, 0.0f), wl ((size_t) SR), wr ((size_t) SR);
        e.process (warm.data(), wl.data(), wr.data(), (int) SR);
        in[0] = 1.0f;
        e.process (in.data(), l.data(), r.data(), (int) SR);
        int arg = 0; for (int i = 0; i < (int) SR; ++i) if (std::abs (l[(size_t) i]) > std::abs (l[(size_t) arg])) arg = i;
        std::printf ("   1/8 at 120 BPM: echo at %.1f ms (want 250)\n", arg / SR * 1000);
        check (std::abs (arg - (int) (0.25 * SR)) < 60, "locked to song tempo");
    }


    // ---------------- Pitch correction ----------------
    std::printf ("Pitch correction\n");
    {
        auto run = [] (juce::AudioBuffer<float> b, PitchCorrector::Params p)
        {
            PitchCorrector pc; pc.prepare (SR, 2); pc.setParams (p);
            inBlocksT (b, [&] (auto& c) { pc.process (c); });
            return std::make_pair (b, pc.getLatencySamples());
        };
        const float sharpHz = 220.0f * std::pow (2.0f, 35.0f / 1200.0f); // A3, 35 cents sharp
        auto tone = sungTone (2.0, [&] (double) { return (double) sharpHz; });

        // off = exact delay
        {
            PitchCorrector::Params p; p.enabled = false;
            auto [o, lat] = run (tone, p);
            double err = 0; for (int i = lat; i < o.getNumSamples(); ++i) err = std::max (err, (double) std::abs (o.getSample (0, i) - tone.getSample (0, i - lat)));
            std::printf ("   off: max difference from input %.2g (latency %d samples = %.1f ms)\n", err, lat, lat / SR * 1000);
            check (err < 1e-6, "switched off = untouched");
        }
        // natural speed pulls a sharp note onto pitch
        {
            PitchCorrector::Params p; p.enabled = true; p.speedMs = 25; p.amount = 1;
            auto [o, lat] = run (tone, p);
            const float before = centsBetween (measureHz (tone, 60000), 220.0f);
            const float after  = centsBetween (measureHz (o, 60000 + lat), 220.0f);
            const float lvl = rmsDb (o, 0, 50000, 90000) - rmsDb (tone, 0, 50000, 90000);
            std::printf ("   A3 sung %+.1f cents sharp -> %+.1f cents after tuning (level change %+.2f dB)\n", before, after, lvl);
            check (std::abs (after) < 4.0f, "pulled onto the note");
            check (std::abs (lvl) < 1.0f, "volume unchanged");
        }
        // key/scale: 228 Hz (between A and A#) -> chromatic goes to A#, C major must go to A
        {
            auto t = sungTone (1.5, [] (double) { return 228.0; });
            PitchCorrector::Params p; p.enabled = true; p.speedMs = 0;
            auto [o1, l1] = run (t, p);
            p.scale = 1; p.key = 0;
            auto [o2, l2] = run (t, p);
            const float chrom = centsBetween (measureHz (o1, 40000 + l1), midiHz (58));
            const float maj   = centsBetween (measureHz (o2, 40000 + l2), midiHz (57));
            std::printf ("   228 Hz: chromatic -> A#3 %+.1f c, C major -> A3 %+.1f c\n", chrom, maj);
            check (std::abs (chrom) < 5.0f && std::abs (maj) < 5.0f, "snaps to the right note for the chosen key/scale");
        }
        // vibrato: robot speed flattens it, slow speed keeps it
        {
            auto vib = sungTone (3.0, [] (double t) { return 220.0 * std::pow (2.0, 30.0 / 1200.0 * std::sin (juce::MathConstants<double>::twoPi * 5.5 * t)); });
            auto depth = [&] (const juce::AudioBuffer<float>& o, int lat)
            {
                float lo = 1e9f, hi = -1e9f;
                for (int st = 60000; st < 120000; st += 400) { const float f = measureHz (o, st + lat); if (f > 0) { const float c = centsBetween (f, 220.0f); lo = std::min (lo, c); hi = std::max (hi, c); } }
                return 0.5f * (hi - lo);
            };
            PitchCorrector::Params p; p.enabled = true; p.speedMs = 0;
            auto [oR, lR] = run (vib, p);
            p.speedMs = 150;
            auto [oN, lN] = run (vib, p);
            const float dIn = depth (vib, 0), dR = depth (oR, lR), dN = depth (oN, lN);
            std::printf ("   vibrato depth: sung +/-%.0f c, robot speed +/-%.0f c, slow speed +/-%.0f c\n", dIn, dR, dN);
            check (dR < 8.0f, "robot setting locks the note (hard-tune effect)");
            check (dN > 15.0f, "slow setting keeps natural vibrato");
        }
        // hard tune on a slide: output should sit on semitones
        {
            auto glide = sungTone (2.5, [] (double t) { return 196.0 * std::pow (2.0, (t / 2.5) * 7.0 / 12.0); });
            PitchCorrector::Params p; p.enabled = true; p.speedMs = 0;
            auto [o, lat] = run (glide, p);
            int on = 0, total = 0;
            for (int st = 8000; st < 110000; st += 600)
            {
                const float f = measureHz (o, st + lat); if (f <= 0) continue;
                const float midi = 69.0f + 12.0f * std::log2 (f / 440.0f);
                ++total; if (std::abs (midi - std::round (midi)) < 0.15f) ++on;
            }
            std::printf ("   7-semitone slide, robot speed: %d%% of the time exactly on a note\n", 100 * on / std::max (1, total));
            check (on > total * 80 / 100, "hard-tune steps between notes");
        }
        // breath / 's' noise passes through at the same level
        {
            juce::AudioBuffer<float> nz (2, (int) SR); juce::Random rng (11);
            for (int i = 0; i < nz.getNumSamples(); ++i) { const float v = 0.05f * (rng.nextFloat() * 2 - 1); nz.setSample (0, i, v); nz.setSample (1, i, v); }
            PitchCorrector::Params p; p.enabled = true; p.speedMs = 0;
            auto [o, lat] = run (nz, p);
            const float d = rmsDb (o, 0, lat + 4000, (int) SR) - rmsDb (nz, 0, 4000, (int) SR - lat);
            std::printf ("   unpitched noise level change %+.2f dB\n", d);
            check (std::abs (d) < 1.5f, "breaths and consonants pass through");
        }
    }

    // ---------------- Full chain ----------------
    std::printf ("Full chain\n");
    {
        // latency: with everything neutral, an impulse must come out exactly where we tell FL it will
        VocalChain chain; chain.prepare (SR, 512, 2);
        VocalChain::Settings s;
        s.cleanOn = false; s.toneOn = false; s.dynOn = false; s.deessOn = false; s.colorOn = false; s.spaceOn = false;
        chain.setSettings (s);
        juce::AudioBuffer<float> b (2, 16384); b.clear(); b.setSample (0, 100, 0.5f); b.setSample (1, 100, 0.5f);
        inBlocks (b, [&] (auto& c) { chain.process (c); });
        int arg = 0; for (int i = 0; i < b.getNumSamples(); ++i) if (std::abs (b.getSample (0, i)) > std::abs (b.getSample (0, arg))) arg = i;
        std::printf ("   reported latency %d, measured %d samples\n", chain.getLatencySamples(), arg - 100);
        check (std::abs (arg - 100 - chain.getLatencySamples()) <= 2, "FL Studio delay compensation will line up");
    }
    {
        // every preset only refers to real parameters, with values inside their ranges
        bool tablesOk = true;
        for (auto& pr : presets::all())
            for (auto& [id, v] : pr.values)
            {
                bool found = false;
                for (auto& d : params::all())
                    if (std::string (d.id) == id) { found = true; if (v < d.min - 1e-4f || v > d.max + 1e-4f) { std::printf ("   %s: %s=%g out of range\n", pr.name, id, v); tablesOk = false; } }
                if (! found) { std::printf ("   %s: unknown parameter %s\n", pr.name, id); tablesOk = false; }
            }
        std::printf ("   %d presets in %s\n", (int) presets::all().size(), "the library");
        check (tablesOk, "preset tables are valid");

        // every preset, on a loud and a quiet take, with odd block sizes like FL sends
        for (auto& pr : presets::all())
        {
            const auto vals = presets::valuesFor (pr);
            auto getter = [&] (const char* id) { return vals.at (id); };
            float outDb[2];
            bool ok = true;
            int idx = 0;
            for (float level : { 2.0f, 0.1f })
            {
                VocalChain chain; chain.prepare (SR, 512, 2);
                chain.setSettings (params::makeSettings (getter, 128.0));
                auto b = fakeVocal (10.0, level);
                inBlocks (b, [&] (auto& c) { chain.process (c); }, 1234); // bigger than prepared block
                ok = ok && finite (b) && juce::Decibels::gainToDecibels (peakAbs (b)) <= -0.99f;
                outDb[idx++] = rmsDb (b, 0, (int) (6 * SR), (int) (10 * SR));
            }
            std::printf ("   %-34s loud take %.1f dB, quiet take %.1f dB (takes 26 dB apart)\n", pr.name, outDb[0], outDb[1]);
            check (ok, "stable and under the -1 dB ceiling");
            check (std::abs (outDb[0] - outDb[1]) < 6.0f, "comes out at a similar level either way");
        }
    }

    std::printf (failures == 0 ? "\nALL TESTS PASSED\n" : "\n%d CHECK(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
