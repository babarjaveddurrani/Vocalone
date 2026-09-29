#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

// Steady room-noise reduction (AC hum/hiss, fans, fridge, computer noise, room tone).
//
// STFT spectral gate (2048 Hann, 75% overlap). A per-bin noise profile is either
//   * LEARNED: while "Learn" is on, the plugin averages the spectrum of what it hears
//     (play 1-3 s of the room with nobody singing), or
//   * ADAPTIVE: tracked continuously with minimum statistics (the quietest level each
//     bin reaches over the last ~2 s is taken as the noise floor), so it follows noise
//     that drifts and needs no setup.
// Each bin gets a Wiener-style gain from its signal-to-noise ratio, floored by the
// Reduction amount, then smoothed across time (attack/release) and frequency to
// keep "musical noise" (warbly artifacts) down.
class NoiseReducer
{
public:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize  = 1 << fftOrder;
    static constexpr int hopSize  = fftSize / 4;
    static constexpr int numBins  = fftSize / 2 + 1;
    static constexpr int maxChannels = 2;

    struct Params
    {
        float reductionDb = 12.0f;   // how far noise-only bins are pulled down (0 = off)
        float thresholdDb = 6.0f;    // how far above the noise profile a bin must be to pass untouched
        float releaseMs   = 80.0f;   // gain recovery speed (longer = smoother, fewer artifacts)
        bool  learn       = false;   // capture profile from incoming audio
        bool  adaptive    = false;   // track profile automatically instead of using learned one
    };

    NoiseReducer() : fft (fftOrder)
    {
        for (int i = 0; i < fftSize; ++i)
            window[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * (float) i / (float) fftSize);
        for (auto& p : learnedProfile) p.store (0.0f);
        for (auto& d : displayGr) d.store (0.0f);
        for (auto& d : displayNoise) d.store (-200.0f);
        reset();
    }

    static constexpr int getLatencySamples() { return fftSize; }

    void prepare (double sr, int channels)
    {
        sampleRate = sr;
        numChannels = juce::jlimit (1, maxChannels, channels);
        reset();
    }

    void reset()
    {
        for (auto& c : ch)
        {
            c.inRing.assign ((size_t) fftSize, 0.0f);
            c.outRing.assign ((size_t) fftSize, 0.0f);
            c.gainState.fill (1.0f);
            c.smoothP.fill (0.0f);
        }
        smoothPow.fill (0.0f);
        adaptiveNoise.fill (0.0f);
        subMin.fill (1.0e30f);
        for (auto& w : winMins) w.fill (1.0e30f);
        subCount = 0; winIdx = 0; framesSeen = 0;
        learnAccum.fill (0.0);
        learnFrames = 0;
        wasLearning = false;
        writePos = 0; hopCount = 0;
    }

    void setParams (const Params& p) { params = p; }

    // ---- profile persistence (called from message thread) ----
    bool hasLearnedProfile() const { return profileValid.load(); }
    std::vector<float> getLearnedProfile() const
    {
        std::vector<float> v ((size_t) numBins);
        for (int b = 0; b < numBins; ++b) v[(size_t) b] = learnedProfile[(size_t) b].load();
        return v;
    }
    void setLearnedProfile (const std::vector<float>& v)
    {
        if ((int) v.size() != numBins) return;
        for (int b = 0; b < numBins; ++b) learnedProfile[(size_t) b].store (v[(size_t) b]);
        profileValid.store (true);
    }
    void clearLearnedProfile() { profileValid.store (false); }

    // In-place; output delayed by getLatencySamples().
    void process (juce::AudioBuffer<float>& buffer)
    {
        const int n = buffer.getNumSamples();
        const int chans = juce::jmin (buffer.getNumChannels(), numChannels);

        for (int i = 0; i < n; ++i)
        {
            for (int c = 0; c < chans; ++c)
            {
                auto& s = ch[(size_t) c];
                float* d = buffer.getWritePointer (c);
                s.inRing[(size_t) writePos] = d[i];
                d[i] = s.outRing[(size_t) writePos];
                s.outRing[(size_t) writePos] = 0.0f;
            }
            writePos = (writePos + 1) % fftSize;
            if (++hopCount >= hopSize)
            {
                hopCount = 0;
                processFrame (chans);
            }
        }
    }

    // for the GUI
    float getDisplayReductionDb (int bin) const { return displayGr[(size_t) bin].load (std::memory_order_relaxed); }
    float getDisplayNoiseDb (int bin) const    { return displayNoise[(size_t) bin].load (std::memory_order_relaxed); }
    bool  isLearning() const { return params.learn; }
    float getSampleRate() const { return (float) sampleRate; }

private:
    struct Chan
    {
        std::vector<float> inRing, outRing;
        std::array<float, 2 * fftSize> data {};
        std::array<float, numBins> gainState {};
        std::array<float, numBins> smoothP {};
    };

    void processFrame (int chans)
    {
        // analysis
        for (int c = 0; c < chans; ++c)
        {
            auto& s = ch[(size_t) c];
            for (int i = 0; i < fftSize; ++i)
                s.data[(size_t) i] = s.inRing[(size_t) ((writePos + i) % fftSize)] * window[(size_t) i];
            std::fill (s.data.begin() + fftSize, s.data.end(), 0.0f);
            fft.performRealOnlyForwardTransform (s.data.data(), true);
        }

        // channel-averaged power drives the noise estimate
        for (int b = 0; b < numBins; ++b)
        {
            float p = 0.0f;
            for (int c = 0; c < chans; ++c)
                p += power (ch[(size_t) c], b);
            framePow[(size_t) b] = p / (float) chans;
        }

        updateLearn();
        updateAdaptive();

        const bool useAdaptive = params.adaptive || ! profileValid.load();
        const bool haveEstimate = useAdaptive ? framesSeen > winFrames() : true;

        const float frameRate = (float) sampleRate / (float) hopSize;
        const float attack  = std::exp (-1.0f / (0.005f * frameRate));   // ~5 ms: open fast so words aren't clipped
        const float release = std::exp (-1.0f / (juce::jmax (10.0f, params.releaseMs) * 0.001f * frameRate));
        const float floorGain = juce::Decibels::decibelsToGain (-juce::jmax (0.0f, params.reductionDb));
        const float thresh = juce::Decibels::decibelsToGain (params.thresholdDb, -100.0f); // power-domain factor via ^2 below
        const float threshPow = thresh * thresh;
        const bool active = params.reductionDb > 0.01f && haveEstimate && ! params.learn;

        for (int b = 0; b < numBins; ++b)
        {
            const float noise = useAdaptive ? adaptiveNoise[(size_t) b] : learnedProfile[(size_t) b].load (std::memory_order_relaxed);
            noiseNow[(size_t) b] = noise;
        }

        for (int c = 0; c < chans; ++c)
        {
            auto& s = ch[(size_t) c];
            for (int b = 0; b < numBins; ++b)
            {
                float target = 1.0f;
                if (active)
                {
                    // lightly time-smoothed power: random noise spikes in single frames
                    // shouldn't pop the gate open
                    float& sp = s.smoothP[(size_t) b];
                    const float raw = power (s, b);
                    sp = juce::jmax (raw * 0.5f, 0.6f * sp + 0.4f * raw);
                    const float p = sp + 1.0e-20f;
                    const float nThr = noiseNow[(size_t) b] * threshPow;
                    // power-subtraction amplitude gain: ~1 when signal is well above noise*threshold,
                    // falls to 0 as the bin approaches the noise floor
                    const float g = std::sqrt (juce::jmax (0.0f, p - nThr) / p);
                    target = juce::jmax (floorGain, g);
                }
                float& g = s.gainState[(size_t) b];
                g = (target > g) ? attack * (g - target) + target
                                 : release * (g - target) + target;
            }

            // frequency smoothing (5-tap) to suppress isolated bins flickering on/off
            for (int b = 0; b < numBins; ++b)
            {
                auto at = [&] (int k) { return s.gainState[(size_t) juce::jlimit (0, numBins - 1, k)]; };
                smoothedGain[(size_t) b] = 0.1f * at (b - 2) + 0.2f * at (b - 1) + 0.4f * at (b) + 0.2f * at (b + 1) + 0.1f * at (b + 2);
                smoothedGain[(size_t) b] = juce::jmax (floorGain, smoothedGain[(size_t) b]);
            }

            if (c == 0)
                for (int b = 0; b < numBins; ++b)
                {
                    displayGr[(size_t) b].store (20.0f * std::log10 (smoothedGain[(size_t) b] + 1.0e-9f), std::memory_order_relaxed);
                    displayNoise[(size_t) b].store (10.0f * std::log10 (noiseNow[(size_t) b] + 1.0e-20f), std::memory_order_relaxed);
                }

            for (int b = 0; b < numBins; ++b)
            {
                s.data[(size_t) (2 * b)]     *= smoothedGain[(size_t) b];
                s.data[(size_t) (2 * b + 1)] *= smoothedGain[(size_t) b];
            }
            fft.performRealOnlyInverseTransform (s.data.data());
            constexpr float olaScale = 1.0f / 1.5f;
            for (int i = 0; i < fftSize; ++i)
                s.outRing[(size_t) ((writePos + i) % fftSize)] += s.data[(size_t) i] * window[(size_t) i] * olaScale;
        }
    }

    void updateLearn()
    {
        if (params.learn)
        {
            if (! wasLearning) { learnAccum.fill (0.0); learnFrames = 0; }
            for (int b = 0; b < numBins; ++b) learnAccum[(size_t) b] += (double) framePow[(size_t) b];
            ++learnFrames;
            wasLearning = true;
        }
        else if (wasLearning)
        {
            wasLearning = false;
            if (learnFrames >= 8) // need ~90 ms of material at minimum
            {
                for (int b = 0; b < numBins; ++b)
                    learnedProfile[(size_t) b].store ((float) (learnAccum[(size_t) b] / (double) learnFrames));
                profileValid.store (true);
            }
        }
    }

    int winFrames() const { return subLen * numSub; }

    // Minimum statistics (Martin 2001, simplified): smooth the periodogram, keep the
    // minimum over numSub sub-windows of subLen frames (~2 s total), correct the
    // minimum's downward bias.
    void updateAdaptive()
    {
        constexpr float smooth = 0.85f;
        constexpr float biasCorrection = 2.0f; // min of a smoothed periodogram sits ~3 dB under the mean
        for (int b = 0; b < numBins; ++b)
        {
            float& sp = smoothPow[(size_t) b];
            sp = (framesSeen == 0) ? framePow[(size_t) b] : smooth * sp + (1.0f - smooth) * framePow[(size_t) b];
            subMin[(size_t) b] = juce::jmin (subMin[(size_t) b], sp);
        }
        ++framesSeen;

        if (++subCount >= subLen)
        {
            subCount = 0;
            winMins[(size_t) winIdx] = subMin;
            winIdx = (winIdx + 1) % numSub;
            subMin.fill (1.0e30f);
        }

        for (int b = 0; b < numBins; ++b)
        {
            float m = subMin[(size_t) b];
            for (int k = 0; k < numSub; ++k) m = juce::jmin (m, winMins[(size_t) k][(size_t) b]);
            adaptiveNoise[(size_t) b] = (m >= 1.0e29f ? 0.0f : m) * biasCorrection;
        }
    }

    static float power (const Chan& s, int b)
    {
        const float re = s.data[(size_t) (2 * b)], im = s.data[(size_t) (2 * b + 1)];
        return re * re + im * im;
    }

    static constexpr int subLen = 24, numSub = 8;

    juce::dsp::FFT fft;
    std::array<float, fftSize> window {};
    std::array<Chan, maxChannels> ch;
    std::array<float, numBins> framePow {}, smoothPow {}, adaptiveNoise {}, subMin {}, noiseNow {}, smoothedGain {};
    std::array<std::array<float, numBins>, numSub> winMins {};
    std::array<double, numBins> learnAccum {};
    std::array<std::atomic<float>, numBins> learnedProfile, displayGr, displayNoise;
    std::atomic<bool> profileValid { false };
    Params params;
    double sampleRate = 44100.0;
    int numChannels = 2, writePos = 0, hopCount = 0;
    int subCount = 0, winIdx = 0, framesSeen = 0, learnFrames = 0;
    bool wasLearning = false;
};
