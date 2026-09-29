#pragma once
#include <juce_dsp/juce_dsp.h>

// Level-independent de-esser.
// Instead of a fixed dB threshold (which depends on how loud the recording is),
// it compares the energy in the sibilance band with the energy of the whole
// voice. Vowels sit 15-30 dB below full-band in that range; "s", "sh", "t"
// sounds come close to full-band level. When the band gets too dominant,
// only the band is turned down (split-band), so the vowel tone is untouched.
class DeEsser
{
public:
    struct Params
    {
        float amount = 0.5f;     // 0 = off, 1 = strong
        float freqHz = 6500.0f;  // bottom of the sibilance band
        bool  listen = false;    // solo what the detector hears
    };

    void prepare (double sr, int numChannels)
    {
        sampleRate = sr;
        chans = juce::jlimit (1, 2, numChannels);
        for (auto& f : bandHP) f.reset();
        for (auto& f : detLP) f.reset();
        lastFreq = -1.0f;
        reset();
    }

    void reset()
    {
        for (auto& f : bandHP) f.reset();
        for (auto& f : detLP) f.reset();
        bandEnv = fullEnv = 0.0f;
        grDb = 0.0f;
    }

    void setParams (const Params& p)
    {
        params = p;
        if (std::abs (p.freqHz - lastFreq) > 1.0f)
        {
            lastFreq = p.freqHz;
            const float f = juce::jlimit (1000.0f, (float) sampleRate * 0.4f, p.freqHz);
            auto hp = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, f, 0.7071f);
            auto lp = juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, juce::jmin (14000.0f, (float) sampleRate * 0.45f), 0.7071f);
            for (auto& flt : bandHP) flt.coefficients = hp;
            for (auto& flt : detLP) flt.coefficients = lp;
        }
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int n = buffer.getNumSamples();
        const int c = juce::jmin (buffer.getNumChannels(), chans);
        const float aCoef = std::exp (-1.0f / (0.0008f * (float) sampleRate));
        const float rCoef = std::exp (-1.0f / (0.060f * (float) sampleRate));
        const float gA = std::exp (-1.0f / (0.001f * (float) sampleRate));
        const float gR = std::exp (-1.0f / (0.050f * (float) sampleRate));

        const float thresholdRel = -2.0f - 14.0f * params.amount;   // band vs full, dB
        const float maxCut = 3.0f + 12.0f * params.amount;
        const bool active = params.amount > 0.001f;

        for (int i = 0; i < n; ++i)
        {
            float band[2] {}, full = 0.0f, det = 0.0f;
            for (int ch = 0; ch < c; ++ch)
            {
                const float x = buffer.getSample (ch, i);
                band[ch] = bandHP[(size_t) ch].processSample (x);
                const float d = detLP[(size_t) ch].processSample (band[ch]);
                det = juce::jmax (det, std::abs (d));
                full = juce::jmax (full, std::abs (x));
            }
            bandEnv = follow (bandEnv, det, aCoef, rCoef);
            fullEnv = follow (fullEnv, full, aCoef, rCoef);

            float target = 0.0f;
            if (active && bandEnv > 1.0e-3f) // ignore near-silence
            {
                const float rel = juce::Decibels::gainToDecibels (bandEnv / (fullEnv + 1.0e-9f));
                const float over = rel - thresholdRel;
                if (over > 0.0f)
                    target = -juce::jmin (maxCut, over * 3.0f);
            }
            grDb = (target < grDb) ? gA * (grDb - target) + target : gR * (grDb - target) + target;
            const float g = juce::Decibels::decibelsToGain (grDb);

            for (int ch = 0; ch < c; ++ch)
            {
                const float x = buffer.getSample (ch, i);
                buffer.setSample (ch, i, params.listen ? band[ch] : x - band[ch] + band[ch] * g);
            }
        }
        meterGr.store (grDb, std::memory_order_relaxed);
    }

    float getReductionDb() const { return meterGr.load (std::memory_order_relaxed); }

private:
    static float follow (float env, float x, float a, float r)
    {
        return (x > env) ? a * (env - x) + x : r * (env - x) + x;
    }

    std::array<juce::dsp::IIR::Filter<float>, 2> bandHP, detLP;
    Params params;
    double sampleRate = 44100.0;
    int chans = 2;
    float lastFreq = -1.0f, bandEnv = 0.0f, fullEnv = 0.0f, grDb = 0.0f;
    std::atomic<float> meterGr { 0.0f };
};
