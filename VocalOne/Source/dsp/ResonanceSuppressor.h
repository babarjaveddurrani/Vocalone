#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

// Dynamic spectral resonance suppressor.
// STFT (Hann, 75% overlap). For every bin we compare its level (dB) with a
// smoothed spectral envelope. Bins that stick out above the envelope are
// resonances/harshness and get pulled down. Reduction is smoothed in time
// (attack/release) and across frequency to avoid musical noise.
// Stereo channels share one reduction curve so the image doesn't shift.
class ResonanceSuppressor
{
public:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize  = 1 << fftOrder;   // 2048
    static constexpr int hopSize  = fftSize / 4;
    static constexpr int numBins  = fftSize / 2 + 1;
    static constexpr int maxChannels = 2;

    struct Params
    {
        float depth       = 0.6f;    // 0..1  how much of the excess is removed
        float sharpness   = 0.5f;    // 0..1  0 = broad detection, 1 = narrow peaks only
        float sensitivity = 0.5f;    // 0..1  higher = reacts to smaller peaks
        float maxCutDb    = 12.0f;   // maximum reduction per bin
        float attackMs    = 8.0f;
        float releaseMs   = 120.0f;
        float lowHz       = 200.0f;  // processing range
        float highHz      = 12000.0f;
    };

    ResonanceSuppressor() : fft (fftOrder)
    {
        for (int i = 0; i < fftSize; ++i)
            window[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * (float) i / (float) fftSize);
        for (auto& a : displayGr) a.store (0.0f);
        reset();
    }

    static constexpr int getLatencySamples() { return fftSize; }

    void prepare (double newSampleRate, int channels)
    {
        sampleRate = newSampleRate;
        numChannels = juce::jlimit (1, maxChannels, channels);
        reset();
    }

    void reset()
    {
        for (auto& c : ch)
        {
            c.inRing.assign ((size_t) fftSize, 0.0f);
            c.outRing.assign ((size_t) fftSize, 0.0f);
        }
        grState.assign ((size_t) numBins, 0.0f);
        writePos = 0;
        hopCount = 0;
    }

    void setParams (const Params& p) { params = p; }

    // In-place. Output is delayed by getLatencySamples().
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

    float getDisplayReductionDb (int bin) const { return displayGr[(size_t) bin].load (std::memory_order_relaxed); }
    float getSampleRate() const { return (float) sampleRate; }

private:
    struct Chan
    {
        std::vector<float> inRing, outRing;
        std::array<float, 2 * fftSize> data {};
    };

    void processFrame (int chans)
    {
        // analysis
        for (int c = 0; c < chans; ++c)
        {
            auto& s = ch[(size_t) c];
            for (int i = 0; i < fftSize; ++i)
                s.data[(size_t) i] = s.inRing[(size_t) ((writePos + i) % fftSize)] * window[(size_t) i];
            for (int i = fftSize; i < 2 * fftSize; ++i)
                s.data[(size_t) i] = 0.0f;
            fft.performRealOnlyForwardTransform (s.data.data(), true);
        }

        // level in dB (max across channels => linked reduction)
        for (int b = 0; b < numBins; ++b)
        {
            float m = 0.0f;
            for (int c = 0; c < chans; ++c)
            {
                const float re = ch[(size_t) c].data[(size_t) (2 * b)];
                const float im = ch[(size_t) c].data[(size_t) (2 * b + 1)];
                m = juce::jmax (m, re * re + im * im);
            }
            levelDb[(size_t) b] = 10.0f * std::log10 (m + 1.0e-12f);
        }

        // prefix sums for fast moving average in dB
        prefix[0] = 0.0;
        for (int b = 0; b < numBins; ++b)
            prefix[(size_t) b + 1] = prefix[(size_t) b] + (double) levelDb[(size_t) b];

        const float binHz = (float) sampleRate / (float) fftSize;
        const float frameRate = (float) sampleRate / (float) hopSize;
        const float aCoef = std::exp (-1.0f / (juce::jmax (0.1f, params.attackMs)  * 0.001f * frameRate));
        const float rCoef = std::exp (-1.0f / (juce::jmax (1.0f, params.releaseMs) * 0.001f * frameRate));

        // width of the envelope smoother in octaves: sharp => narrow window => only narrow peaks stick out
        const float widthOct = juce::jmap (params.sharpness, 1.2f, 0.25f);
        const float widthFactor = std::pow (2.0f, widthOct * 0.5f) - 1.0f;
        const float thresholdDb = juce::jmap (params.sensitivity, 6.0f, 0.0f);

        for (int b = 0; b < numBins; ++b)
        {
            const float hz = (float) b * binHz;
            float target = 0.0f;

            if (hz >= params.lowHz * 0.7f && hz <= params.highHz * 1.3f)
            {
                const int half = juce::jmax (3, (int) std::lround ((float) b * widthFactor));
                const int lo = juce::jmax (0, b - half);
                const int hi = juce::jmin (numBins - 1, b + half);
                const float env = (float) ((prefix[(size_t) hi + 1] - prefix[(size_t) lo]) / (double) (hi - lo + 1));
                const float excess = levelDb[(size_t) b] - env - thresholdDb;

                if (excess > 0.0f)
                    target = -juce::jmin (params.maxCutDb, excess * params.depth * 1.5f);

                // soft edges of the processing range
                float mask = 1.0f;
                if (hz < params.lowHz)   mask = juce::jmax (0.0f, (hz - params.lowHz * 0.7f)  / (params.lowHz * 0.3f));
                if (hz > params.highHz)  mask = juce::jmax (0.0f, (params.highHz * 1.3f - hz) / (params.highHz * 0.3f));
                target *= mask;
            }

            float& g = grState[(size_t) b];
            g = (target < g) ? aCoef * (g - target) + target
                             : rCoef * (g - target) + target;
        }

        // smooth across frequency
        for (int b = 0; b < numBins; ++b)
        {
            const float l = grState[(size_t) juce::jmax (0, b - 1)];
            const float r = grState[(size_t) juce::jmin (numBins - 1, b + 1)];
            gain[(size_t) b] = std::pow (10.0f, (0.25f * l + 0.5f * grState[(size_t) b] + 0.25f * r) / 20.0f);
            displayGr[(size_t) b].store (20.0f * std::log10 (gain[(size_t) b]), std::memory_order_relaxed);
        }

        // apply, synthesize, overlap-add
        constexpr float olaScale = 1.0f / 1.5f; // sum of hann^2 at 75% overlap
        for (int c = 0; c < chans; ++c)
        {
            auto& s = ch[(size_t) c];
            for (int b = 0; b < numBins; ++b)
            {
                s.data[(size_t) (2 * b)]     *= gain[(size_t) b];
                s.data[(size_t) (2 * b + 1)] *= gain[(size_t) b];
            }
            fft.performRealOnlyInverseTransform (s.data.data());
            for (int i = 0; i < fftSize; ++i)
                s.outRing[(size_t) ((writePos + i) % fftSize)] += s.data[(size_t) i] * window[(size_t) i] * olaScale;
        }
    }

    juce::dsp::FFT fft;
    std::array<float, fftSize> window {};
    std::array<Chan, maxChannels> ch;
    std::array<float, numBins> levelDb {}, gain {};
    std::array<double, numBins + 1> prefix {};
    std::vector<float> grState;
    std::array<std::atomic<float>, numBins> displayGr;
    Params params;
    double sampleRate = 44100.0;
    int numChannels = 2, writePos = 0, hopCount = 0;
};
