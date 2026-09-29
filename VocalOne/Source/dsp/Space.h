#pragma once
#include <juce_dsp/juce_dsp.h>

// ---------------------------------------------------------------------------
// Doubler / Width: two slightly-detuned, slowly drifting short delays panned
// left and right, like a second take. Adds width without a stereo mic.
// ---------------------------------------------------------------------------
class Doubler
{
public:
    void prepare (double sr, int maxBlock)
    {
        sampleRate = sr;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlock, 1 };
        for (auto& d : lines) { d.prepare (spec); d.setMaximumDelayInSamples ((int) (0.05 * sr)); d.reset(); }
        phase = { 0.0, 1.3 };
        auto c = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, 180.0f);
        for (auto& f : hp) { f.coefficients = c; f.reset(); }
    }

    void setAmount (float a) { amount = juce::jlimit (0.0f, 1.0f, a); }

    void process (juce::AudioBuffer<float>& b)
    {
        if (b.getNumChannels() < 2) return;
        const int n = b.getNumSamples();
        const double inc[2] { 0.27 / sampleRate, 0.33 / sampleRate };
        const float base[2] { 0.0115f, 0.0175f }, depth = 0.0016f;
        const float level = 0.55f * amount;
        for (int i = 0; i < n; ++i)
        {
            const float mono = 0.5f * (b.getSample (0, i) + b.getSample (1, i));
            for (int k = 0; k < 2; ++k)
            {
                phase[(size_t) k] += inc[k];
                if (phase[(size_t) k] > 1.0) phase[(size_t) k] -= 1.0;
                const float dSec = base[k] + depth * (float) std::sin (juce::MathConstants<double>::twoPi * phase[(size_t) k]);
                auto& line = lines[(size_t) k];
                line.pushSample (0, mono);
                const float wet = hp[(size_t) k].processSample (line.popSample (0, dSec * (float) sampleRate));
                if (level > 0.0f) b.getWritePointer (k)[i] += level * wet;
            }
        }
    }

private:
    std::array<juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>, 2> lines;
    std::array<juce::dsp::IIR::Filter<float>, 2> hp;
    std::array<double, 2> phase {};
    double sampleRate = 44100.0;
    float amount = 0.0f;
};

// ---------------------------------------------------------------------------
// Tempo-synced echo (a "send" effect: returns wet only). Ping-pong optional.
// Repeats are filtered (250 Hz - 4.5 kHz) so they sit behind the lead.
// ---------------------------------------------------------------------------
class EchoDelay
{
public:
    static juce::StringArray divisionNames() { return { "1/4", "1/8 dotted", "1/8", "1/16", "1/4 dotted", "1/2" }; }
    static double divisionBeats (int idx)
    {
        static const double beats[] { 1.0, 0.75, 0.5, 0.25, 1.5, 2.0 };
        return beats[juce::jlimit (0, 5, idx)];
    }

    struct Params { int division = 0; float feedback = 0.3f; bool pingPong = true; double bpm = 120.0; };

    void prepare (double sr, int maxBlock)
    {
        sampleRate = sr;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlock, 1 };
        for (auto& d : lines) { d.prepare (spec); d.setMaximumDelayInSamples ((int) (4.1 * sr)); d.reset(); }
        auto hp = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, 250.0f);
        auto lp = juce::dsp::IIR::Coefficients<float>::makeLowPass (sr, 4500.0f);
        for (auto& f : fbHP) { f.coefficients = hp; f.reset(); }
        for (auto& f : fbLP) { f.coefficients = lp; f.reset(); }
        fb.fill (0.0f);
        delaySamples.reset (sr, 0.08);
        delaySamples.setCurrentAndTargetValue ((float) (0.5 * sr));
    }

    void setParams (const Params& p)
    {
        params = p;
        const double bpm = juce::jlimit (30.0, 300.0, p.bpm > 0.0 ? p.bpm : 120.0);
        const double secs = juce::jmin (4.0, 60.0 / bpm * divisionBeats (p.division));
        delaySamples.setTargetValue ((float) (secs * sampleRate));
    }

    // in: mono send signal; outL/outR: wet return
    void process (const float* in, float* outL, float* outR, int n)
    {
        const float fbAmt = juce::jlimit (0.0f, 0.9f, params.feedback);
        for (int i = 0; i < n; ++i)
        {
            const float ds = delaySamples.getNextValue();
            const float yL = lines[0].popSample (0, ds);
            const float yR = lines[1].popSample (0, ds);
            const float fL = fbLP[0].processSample (fbHP[0].processSample (yL));
            const float fR = fbLP[1].processSample (fbHP[1].processSample (yR));
            if (params.pingPong)
            {
                lines[0].pushSample (0, in[i] + fbAmt * fR);
                lines[1].pushSample (0, fbAmt * fL);
            }
            else
            {
                lines[0].pushSample (0, in[i] + fbAmt * fL);
                lines[1].pushSample (0, in[i] + fbAmt * fR);
            }
            outL[i] = fL;
            outR[i] = fR;
        }
    }

private:
    std::array<juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd>, 2> lines;
    std::array<juce::dsp::IIR::Filter<float>, 2> fbHP, fbLP;
    std::array<float, 2> fb {};
    juce::SmoothedValue<float> delaySamples;
    Params params;
    double sampleRate = 44100.0;
};

// ---------------------------------------------------------------------------
// Vocal reverb (a "send" effect: returns wet only). Pre-delay keeps the words
// clear of the tail; the send is EQ'd (250 Hz - 7 kHz) so the reverb never
// gets muddy or hissy.
// ---------------------------------------------------------------------------
class VocalReverb
{
public:
    struct Params { float size = 0.5f; float preDelayMs = 30.0f; };

    void prepare (double sr, int maxBlock)
    {
        sampleRate = sr;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlock, 1 };
        pre.prepare (spec);
        pre.setMaximumDelayInSamples ((int) (0.25 * sr));
        pre.reset();
        hp.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, 250.0f);
        lp.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass (sr, 7000.0f);
        hp.reset(); lp.reset();
        reverb.setSampleRate (sr);
        reverb.reset();
        preSamples.reset (sr, 0.05);
    }

    void setParams (const Params& p)
    {
        juce::Reverb::Parameters rp;
        rp.roomSize = 0.45f + 0.5f * juce::jlimit (0.0f, 1.0f, p.size);
        rp.damping = 0.45f;
        rp.width = 1.0f;
        rp.wetLevel = 1.0f;
        rp.dryLevel = 0.0f;
        rp.freezeMode = 0.0f;
        reverb.setParameters (rp);
        preSamples.setTargetValue ((float) (juce::jlimit (0.0f, 200.0f, p.preDelayMs) * 0.001 * sampleRate));
    }

    void process (const float* in, float* outL, float* outR, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            pre.pushSample (0, in[i]);
            const float x = lp.processSample (hp.processSample (pre.popSample (0, preSamples.getNextValue())));
            outL[i] = x;
            outR[i] = x;
        }
        reverb.processStereo (outL, outR, n);
    }

private:
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> pre;
    juce::dsp::IIR::Filter<float> hp, lp;
    juce::Reverb reverb;
    juce::SmoothedValue<float> preSamples;
    double sampleRate = 44100.0;
};
