#pragma once
#include <juce_dsp/juce_dsp.h>

// ---------------------------------------------------------------------------
// ToneEQ: four musical vocal bands.
//   Warmth   low shelf 150 Hz   (-6..+6 dB)   chest / body
//   Clarity  bell 320 Hz        (0..-8 dB)    removes "boxy / muddy"
//   Presence bell 3.5 kHz       (-6..+8 dB)   words cut through the beat
//   Air      high shelf 11 kHz  (0..+10 dB)   shine / breath
// ---------------------------------------------------------------------------
class ToneEQ
{
public:
    struct Params { float warmthDb = 0.0f, clarity = 0.3f, presenceDb = 2.0f, airDb = 3.0f, hiCutHz = 20000.0f; };

    void prepare (double sr, int numChannels)
    {
        sampleRate = sr;
        chans = juce::jlimit (1, 2, numChannels);
        for (auto& band : filters) for (auto& f : band) f.reset();
        cached = {};
        cached.warmthDb = 1000.0f; // force coefficient build
    }

    void setParams (const Params& p)
    {
        auto same = [] (float a, float b) { return std::abs (a - b) < 1.0e-4f; };
        if (same (p.warmthDb, cached.warmthDb) && same (p.clarity, cached.clarity)
            && same (p.presenceDb, cached.presenceDb) && same (p.airDb, cached.airDb) && same (p.hiCutHz, cached.hiCutHz)) return;
        cached = p;
        using C = juce::dsp::IIR::Coefficients<float>;
        const float nyq = (float) sampleRate * 0.45f;
        auto g = [] (float db) { return juce::Decibels::decibelsToGain (db); };
        set (0, C::makeLowShelf  (sampleRate, 150.0f, 0.7f, g (p.warmthDb)));
        set (1, C::makePeakFilter (sampleRate, 320.0f, 1.0f, g (-8.0f * juce::jlimit (0.0f, 1.0f, p.clarity))));
        set (2, C::makePeakFilter (sampleRate, juce::jmin (3500.0f, nyq), 0.8f, g (p.presenceDb)));
        set (3, C::makeHighShelf (sampleRate, juce::jmin (11000.0f, nyq), 0.7f, g (p.airDb)));
        // high cut: 12 dB/oct low-pass; "off" at the top of the range
        hiCutActive = p.hiCutHz < 19500.0f;
        if (hiCutActive) set (4, C::makeLowPass (sampleRate, juce::jmin (p.hiCutHz, nyq), 0.7071f));
    }

    void process (juce::AudioBuffer<float>& b)
    {
        const int c = juce::jmin (chans, b.getNumChannels());
        for (int ch = 0; ch < c; ++ch)
        {
            float* d = b.getWritePointer (ch);
            for (size_t band = 0; band < filters.size(); ++band)
            {
                if (band == 4 && ! hiCutActive) continue;
                for (int i = 0; i < b.getNumSamples(); ++i)
                    d[i] = filters[band][(size_t) ch].processSample (d[i]);
            }
        }
    }

private:
    void set (int band, juce::dsp::IIR::Coefficients<float>::Ptr c)
    {
        for (auto& f : filters[(size_t) band]) f.coefficients = c;
    }

    std::array<std::array<juce::dsp::IIR::Filter<float>, 2>, 5> filters;
    Params cached;
    bool hiCutActive = false;
    double sampleRate = 44100.0;
    int chans = 2;
};

// ---------------------------------------------------------------------------
// Saturation: analog-style warmth. Soft tube-like curve (tanh with a little
// asymmetry for even harmonics), 2x oversampled so the top end stays clean,
// blended in parallel so quiet detail stays natural. Always runs (even at 0)
// so the plugin's latency never changes.
// ---------------------------------------------------------------------------
class Saturation
{
public:
    void prepare (double sr, int maxBlock, int numChannels)
    {
        chans = juce::jlimit (1, 2, numChannels);
        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
            (size_t) chans, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
        oversampler->initProcessing ((size_t) maxBlock);
        dcX.fill (0.0f); dcY.fill (0.0f);
        dcR = 1.0f - (2.0f * juce::MathConstants<float>::pi * 10.0f / (float) sr);
    }

    int getLatencySamples() const { return oversampler ? (int) std::lround (oversampler->getLatencyInSamples()) : 0; }
    void setAmount (float a) { amount = juce::jlimit (0.0f, 1.0f, a); }

    void process (juce::AudioBuffer<float>& b)
    {
        juce::dsp::AudioBlock<float> block (b.getArrayOfWritePointers(), (size_t) juce::jmin (chans, b.getNumChannels()), (size_t) b.getNumSamples());
        auto up = oversampler->processSamplesUp (block);

        const float drive = juce::Decibels::decibelsToGain (14.0f * amount);
        const float bias = 0.15f * amount;
        const float tb = std::tanh (bias);
        const float mix = juce::jmin (1.0f, amount * 1.1f);
        if (amount > 0.0001f)
        {
            for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
            {
                float* d = up.getChannelPointer (ch);
                for (size_t i = 0; i < up.getNumSamples(); ++i)
                {
                    const float x = d[i];
                    const float wet = (std::tanh (drive * x + bias) - tb) / drive;
                    d[i] = x + mix * (wet - x);
                }
            }
        }
        oversampler->processSamplesDown (block);

        // DC blocker (the asymmetry creates a tiny offset)
        for (int ch = 0; ch < (int) block.getNumChannels(); ++ch)
        {
            float* d = b.getWritePointer (ch);
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const float y = d[i] - dcX[(size_t) ch] + dcR * dcY[(size_t) ch];
                dcX[(size_t) ch] = d[i]; dcY[(size_t) ch] = y; d[i] = y;
            }
        }
    }

private:
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    std::array<float, 2> dcX {}, dcY {};
    float dcR = 0.999f, amount = 0.0f;
    int chans = 2;
};
