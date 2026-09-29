#pragma once
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <vector>

// ---------------------------------------------------------------------------
// AutoLeveler: slowly rides the vocal toward a target loudness (like a
// mixer riding a fader), so every recording hits the compressor, de-esser
// and saturation at the same level. This is what makes the presets work
// whether you recorded quiet or hot. It freezes during silence so it
// doesn't pump up room noise between lines.
// ---------------------------------------------------------------------------
class AutoLeveler
{
public:
    struct Params
    {
        bool  enabled = true;
        float targetDb = -18.0f;  // RMS target
        float maxBoostDb = 18.0f;
        float maxCutDb = 15.0f;
        float gateDb = -50.0f;    // below this = silence, hold the gain
    };

    void prepare (double sr) { sampleRate = sr; reset(); }
    void reset() { env = 0.0f; gainDb = 0.0f; }
    void setParams (const Params& p) { params = p; }

    void process (juce::AudioBuffer<float>& b)
    {
        const int n = b.getNumSamples(), c = b.getNumChannels();
        const float sr = (float) sampleRate;
        const float envCoef = std::exp (-1.0f / (0.080f * sr));
        const float upCoef   = std::exp (-1.0f / (0.800f * sr)); // raise slowly
        const float downCoef = std::exp (-1.0f / (0.250f * sr)); // pull down a bit faster
        for (int i = 0; i < n; ++i)
        {
            float m = 0.0f;
            for (int ch = 0; ch < c; ++ch) { const float x = b.getSample (ch, i); m = juce::jmax (m, x * x); }
            env = envCoef * env + (1.0f - envCoef) * m;
            const float envDb = 10.0f * std::log10 (env + 1.0e-12f);

            float target = gainDb;                          // hold during silence
            if (! params.enabled) target = 0.0f;
            else if (envDb > params.gateDb)
                target = juce::jlimit (-params.maxCutDb, params.maxBoostDb, params.targetDb - envDb);

            const float k = target > gainDb ? upCoef : downCoef;
            gainDb = k * gainDb + (1.0f - k) * target;
            const float g = juce::Decibels::decibelsToGain (gainDb);
            for (int ch = 0; ch < c; ++ch) b.getWritePointer (ch)[i] *= g;
        }
        meterGain.store (gainDb, std::memory_order_relaxed);
    }

    float getGainDb() const { return meterGain.load (std::memory_order_relaxed); }

private:
    Params params;
    double sampleRate = 44100.0;
    float env = 0.0f, gainDb = 0.0f;
    std::atomic<float> meterGain { 0.0f };
};

// ---------------------------------------------------------------------------
// Compressor: feed-forward, soft knee, stereo-linked. One "amount" drives
// threshold and ratio together. Make-up gain is automatic: it tracks the
// average reduction while the vocal is present and gives it back, so
// turning the knob changes how controlled/upfront the vocal is, not how loud.
// ---------------------------------------------------------------------------
class Compressor
{
public:
    struct Params
    {
        float amount = 0.4f;     // 0..1
        float attackMs = 10.0f;
        float releaseMs = 120.0f;
    };

    void prepare (double sr) { sampleRate = sr; reset(); }
    void reset() { grDb = 0.0f; avgGr = 0.0f; }
    void setParams (const Params& p) { params = p; }

    void process (juce::AudioBuffer<float>& b)
    {
        const int n = b.getNumSamples(), c = b.getNumChannels();
        const float sr = (float) sampleRate;
        const float a = std::exp (-1.0f / (juce::jmax (0.1f, params.attackMs) * 0.001f * sr));
        const float r = std::exp (-1.0f / (juce::jmax (5.0f, params.releaseMs) * 0.001f * sr));
        const float avgCoef = std::exp (-1.0f / (1.5f * sr));
        const float amt = juce::jlimit (0.0f, 1.0f, params.amount);
        const float threshold = -8.0f - 22.0f * amt;   // input is leveled to ~-18 dB RMS
        const float ratio = 1.0f + 5.0f * amt;
        const float slope = 1.0f - 1.0f / ratio;
        constexpr float knee = 8.0f;

        for (int i = 0; i < n; ++i)
        {
            float peak = 0.0f;
            for (int ch = 0; ch < c; ++ch) peak = juce::jmax (peak, std::abs (b.getSample (ch, i)));
            const float xDb = juce::Decibels::gainToDecibels (peak, -120.0f);

            float target = 0.0f;
            const float over = xDb - threshold;
            if (amt > 0.001f)
            {
                if (over > knee * 0.5f)       target = -slope * over;
                else if (over > -knee * 0.5f) target = -slope * (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee);
            }
            grDb = (target < grDb) ? a * (grDb - target) + target : r * (grDb - target) + target;

            if (xDb > -45.0f) avgGr = avgCoef * avgGr + (1.0f - avgCoef) * grDb; // learn make-up while singing
            const float makeup = juce::jmin (18.0f, -avgGr);
            const float g = juce::Decibels::decibelsToGain (grDb + makeup);
            for (int ch = 0; ch < c; ++ch) b.getWritePointer (ch)[i] *= g;
        }
        meterGr.store (grDb, std::memory_order_relaxed);
    }

    float getReductionDb() const { return meterGr.load (std::memory_order_relaxed); }

private:
    Params params;
    double sampleRate = 44100.0;
    float grDb = 0.0f, avgGr = 0.0f;
    std::atomic<float> meterGr { 0.0f };
};

// ---------------------------------------------------------------------------
// Lookahead brickwall limiter. Guarantees the output never passes the
// ceiling: required gain -> instant-attack/slow-release envelope -> minimum
// over the lookahead window -> moving average over the same window, with
// the audio delayed to match. Latency = lookahead - 1 samples.
// ---------------------------------------------------------------------------
class Limiter
{
public:
    struct Params { bool enabled = true; float ceilingDb = -1.0f; float releaseMs = 80.0f; };

    void prepare (double sr, int numChannels)
    {
        sampleRate = sr;
        L = juce::jmax (2, (int) std::lround (0.0015 * sr));
        chans = juce::jlimit (1, 2, numChannels);
        for (auto& d : delay) d.assign ((size_t) L, 0.0f);
        envRing.assign ((size_t) L, 1.0f);
        minRing.assign ((size_t) L, 1.0f);
        reset();
    }

    void reset()
    {
        for (auto& d : delay) std::fill (d.begin(), d.end(), 0.0f);
        std::fill (envRing.begin(), envRing.end(), 1.0f);
        std::fill (minRing.begin(), minRing.end(), 1.0f);
        env = 1.0f; sum = (double) L; pos = 0; count = 0;
    }

    int getLatencySamples() const { return L - 1; }
    void setParams (const Params& p) { params = p; }

    void process (juce::AudioBuffer<float>& b)
    {
        const int n = b.getNumSamples();
        const int c = juce::jmin (chans, b.getNumChannels());
        const float ceiling = params.enabled ? juce::Decibels::decibelsToGain (params.ceilingDb) : 1.0e9f;
        const float rel = 1.0f - std::exp (-1.0f / (juce::jmax (5.0f, params.releaseMs) * 0.001f * (float) sampleRate));
        float minGain = 1.0f;

        for (int i = 0; i < n; ++i)
        {
            float peak = 0.0f;
            for (int ch = 0; ch < c; ++ch) peak = juce::jmax (peak, std::abs (b.getSample (ch, i)));
            const float req = peak > ceiling ? ceiling / peak : 1.0f;

            env = juce::jmin (req, env + (1.0f - env) * rel);
            envRing[(size_t) pos] = env;
            float m = 1.0f;
            for (float e : envRing) m = juce::jmin (m, e);

            sum += (double) m - (double) minRing[(size_t) pos];
            minRing[(size_t) pos] = m;
            if (++count >= 4096) { count = 0; sum = 0.0; for (float v : minRing) sum += v; } // stop float drift
            const float g = (float) (sum / (double) L);
            minGain = juce::jmin (minGain, g);

            // read delayed sample (L-1 samples ago), write current
            const int readPos = (pos + 1) % L;
            for (int ch = 0; ch < c; ++ch)
            {
                auto& d = delay[(size_t) ch];
                d[(size_t) pos] = b.getSample (ch, i);
                const float y = d[(size_t) readPos] * g;
                b.setSample (ch, i, params.enabled ? juce::jlimit (-ceiling, ceiling, y) : y);
            }
            pos = (pos + 1) % L;
        }
        meterGr.store (juce::Decibels::gainToDecibels (minGain), std::memory_order_relaxed);
    }

    float getReductionDb() const { return meterGr.load (std::memory_order_relaxed); }

private:
    Params params;
    double sampleRate = 44100.0;
    int L = 72, chans = 2, pos = 0, count = 0;
    std::array<std::vector<float>, 2> delay;
    std::vector<float> envRing, minRing;
    float env = 1.0f;
    double sum = 0.0;
    std::atomic<float> meterGr { 0.0f };
};
