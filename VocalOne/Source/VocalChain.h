#pragma once
#include <juce_dsp/juce_dsp.h>
#include "dsp/NoiseReducer.h"
#include "dsp/ResonanceSuppressor.h"
#include "dsp/DeEsser.h"
#include "dsp/Dynamics.h"
#include "dsp/ToneColor.h"
#include "dsp/Space.h"
#include "dsp/PitchCorrector.h"

// The whole vocal chain, pre-routed in the order an engineer would set it up:
//
//  In -> Low Cut -> Noise Reduction -> Resonance -> Pitch Correction -> Tone EQ (+ High Cut) -> Auto Level -> Compressor
//     -> De-Esser -> Saturation -> Width -> (+ Echo return, + Reverb return, both ducked) -> Limiter -> Out
//
// Echo and reverb are parallel sends inside the plugin (fed from the finished
// dry vocal, EQ'd, and ducked while you sing), so there's nothing to route
// on the FL mixer. Every stage always runs, so latency is fixed.
class VocalChain
{
public:
    struct Settings
    {
        float inputDb = 0.0f, outputDb = 0.0f;

        bool  cleanOn = true;
        bool  lowCutOn = true;  float lowCutHz = 90.0f;
        float noiseDb = 10.0f;  float noiseThreshDb = 6.0f;  bool noiseLearn = false;  bool noiseAuto = true;
        float resonance = 0.4f; float resSharpness = 0.5f;

        bool  tuneOn = false;
        float tuneAmount = 1.0f, tuneSpeedMs = 25.0f; int tuneKey = 0, tuneScale = 0;

        bool  toneOn = true;
        float warmthDb = 0.0f, clarity = 0.3f, presenceDb = 2.0f, airDb = 3.0f, hiCutHz = 20000.0f;

        bool  dynOn = true;
        bool  autoLevel = true; float levelTargetDb = -18.0f;
        float compress = 0.4f;  float compAttackMs = 10.0f, compReleaseMs = 120.0f;

        bool  deessOn = true;
        float deess = 0.5f; float deessFreq = 6500.0f; bool deessListen = false;

        bool  colorOn = true;
        float saturation = 0.15f;

        bool  spaceOn = true;
        float width = 0.2f;
        float reverb = 0.15f, reverbSize = 0.5f, reverbPreMs = 30.0f;
        float echo = 0.0f, echoFeedback = 0.3f; int echoDivision = 1; bool echoPingPong = true;
        float duck = 0.5f;
        double bpm = 120.0;

        bool  limiterOn = true; float ceilingDb = -1.0f;
    };

    void prepare (double sr, int maxBlockSize, int numChannels)
    {
        sampleRate = sr;
        maxBlock = juce::jmax (32, maxBlockSize);
        chans = juce::jlimit (1, 2, numChannels);

        juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlock, (juce::uint32) chans };
        lowCut.prepare (spec);
        lowCut.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
        noise.prepare (sr, chans);
        resonance.prepare (sr, chans);
        tuner.prepare (sr, chans);
        tone.prepare (sr, chans);
        leveler.prepare (sr);
        comp.prepare (sr);
        deesser.prepare (sr, chans);
        saturation.prepare (sr, maxBlock, chans);
        doubler.prepare (sr, maxBlock);
        echo.prepare (sr, maxBlock);
        reverb.prepare (sr, maxBlock);
        limiter.prepare (sr, chans);

        send.assign ((size_t) maxBlock, 0.0f);
        for (auto* v : { &echoL, &echoR, &revL, &revR, &revIn }) v->assign ((size_t) maxBlock, 0.0f);
        inGain.reset (sr, 0.02);  outGain.reset (sr, 0.02);
        duckEnv = 0.0f;
    }

    int getLatencySamples() const
    {
        return NoiseReducer::getLatencySamples() + ResonanceSuppressor::getLatencySamples()
             + tuner.getLatencySamples() + saturation.getLatencySamples() + limiter.getLatencySamples();
    }

    void setSettings (const Settings& s)
    {
        settings = s;
        const bool clean = s.cleanOn;

        lowCut.setCutoffFrequency (juce::jlimit (20.0f, 400.0f, s.lowCutHz));

        NoiseReducer::Params np;
        np.reductionDb = clean ? s.noiseDb : 0.0f;
        np.thresholdDb = s.noiseThreshDb;
        np.learn = s.noiseLearn;
        np.adaptive = s.noiseAuto;
        noise.setParams (np);

        ResonanceSuppressor::Params rp;
        rp.depth = clean ? s.resonance : 0.0f;
        rp.sharpness = s.resSharpness;
        rp.sensitivity = 0.5f;
        rp.maxCutDb = 4.0f + 10.0f * s.resonance;
        resonance.setParams (rp);

        PitchCorrector::Params pp;
        pp.enabled = s.tuneOn; pp.amount = s.tuneAmount; pp.speedMs = s.tuneSpeedMs; pp.key = s.tuneKey; pp.scale = s.tuneScale;
        tuner.setParams (pp);

        ToneEQ::Params tp;
        if (s.toneOn) { tp.warmthDb = s.warmthDb; tp.clarity = s.clarity; tp.presenceDb = s.presenceDb; tp.airDb = s.airDb; tp.hiCutHz = s.hiCutHz; }
        else          { tp.warmthDb = 0; tp.clarity = 0; tp.presenceDb = 0; tp.airDb = 0; tp.hiCutHz = 20000.0f; }
        tone.setParams (tp);

        AutoLeveler::Params lp;
        lp.enabled = s.dynOn && s.autoLevel;
        lp.targetDb = s.levelTargetDb;
        leveler.setParams (lp);

        Compressor::Params cp;
        cp.amount = s.dynOn ? s.compress : 0.0f;
        cp.attackMs = s.compAttackMs;
        cp.releaseMs = s.compReleaseMs;
        comp.setParams (cp);

        DeEsser::Params dp;
        dp.amount = s.deessOn ? s.deess : 0.0f;
        dp.freqHz = s.deessFreq;
        dp.listen = s.deessOn && s.deessListen;
        deesser.setParams (dp);

        saturation.setAmount (s.colorOn ? s.saturation : 0.0f);
        doubler.setAmount (s.spaceOn ? s.width : 0.0f);

        EchoDelay::Params ep;
        ep.division = s.echoDivision; ep.feedback = s.echoFeedback; ep.pingPong = s.echoPingPong; ep.bpm = s.bpm;
        echo.setParams (ep);

        VocalReverb::Params vp;
        vp.size = s.reverbSize; vp.preDelayMs = s.reverbPreMs;
        reverb.setParams (vp);

        Limiter::Params mp;
        mp.enabled = s.limiterOn; mp.ceilingDb = s.ceilingDb;
        limiter.setParams (mp);

        inGain.setTargetValue (juce::Decibels::decibelsToGain (s.inputDb));
        outGain.setTargetValue (juce::Decibels::decibelsToGain (s.outputDb));
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int total = buffer.getNumSamples();
        const int c = juce::jmin (chans, buffer.getNumChannels());
        for (int start = 0; start < total; start += maxBlock)
        {
            const int n = juce::jmin (maxBlock, total - start);
            juce::AudioBuffer<float> chunk (buffer.getArrayOfWritePointers(), c, start, n);
            processChunk (chunk);
        }
    }

    // ---- access for GUI / state ----
    NoiseReducer& getNoise() { return noise; }
    ResonanceSuppressor& getResonance() { return resonance; }
    PitchCorrector& getTuner() { return tuner; }
    float getInputLevelDb() const  { return inMeter.load(); }
    float getOutputLevelDb() const { return outMeter.load(); }
    float getCompReductionDb() const { return comp.getReductionDb(); }
    float getLevelerGainDb() const { return leveler.getGainDb(); }
    float getDeessReductionDb() const { return deesser.getReductionDb(); }
    float getLimiterReductionDb() const { return limiter.getReductionDb(); }

    // return gains are exposed so tests can check the send levels
    static float reverbReturnGain (float amount) { return 0.9f * amount * amount + 0.25f * amount; }
    static float echoReturnGain (float amount)   { return 0.7f * amount; }

private:
    void processChunk (juce::AudioBuffer<float>& b)
    {
        const int n = b.getNumSamples(), c = b.getNumChannels();

        for (int i = 0; i < n; ++i)
        {
            const float g = inGain.getNextValue();
            for (int ch = 0; ch < c; ++ch) b.getWritePointer (ch)[i] *= g;
        }
        inMeter.store (peakDb (b));

        if (settings.cleanOn && settings.lowCutOn)
        {
            juce::dsp::AudioBlock<float> block (b);
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            lowCut.process (ctx);
        }
        noise.process (b);
        resonance.process (b);
        tuner.process (b);
        tone.process (b);
        leveler.process (b);
        comp.process (b);
        deesser.process (b);
        saturation.process (b);

        // ---- the finished dry vocal feeds the effect sends ----
        const bool fx = settings.spaceOn && ! settings.deessListen && ! settings.noiseLearn;
        const float revGain = fx ? reverbReturnGain (settings.reverb) : 0.0f;
        const float echoGain = fx ? echoReturnGain (settings.echo) : 0.0f;
        for (int i = 0; i < n; ++i)
        {
            float m = 0.0f;
            for (int ch = 0; ch < c; ++ch) m += b.getSample (ch, i);
            send[(size_t) i] = m / (float) c;
        }

        echo.process (send.data(), echoL.data(), echoR.data(), n);
        // a bit of the echo goes into the reverb so repeats sit in the same space
        for (int i = 0; i < n; ++i) revIn[(size_t) i] = send[(size_t) i] + 0.35f * echoGain * 0.5f * (echoL[(size_t) i] + echoR[(size_t) i]);
        reverb.process (revIn.data(), revL.data(), revR.data(), n);

        doubler.process (b);

        // ducking: effects dip while the vocal is loud and bloom in the gaps
        const float sr = (float) sampleRate;
        const float dA = std::exp (-1.0f / (0.010f * sr)), dR = std::exp (-1.0f / (0.300f * sr));
        for (int i = 0; i < n; ++i)
        {
            const float x = std::abs (send[(size_t) i]);
            duckEnv = (x > duckEnv) ? dA * (duckEnv - x) + x : dR * (duckEnv - x) + x;
            const float envDb = juce::Decibels::gainToDecibels (duckEnv, -100.0f);
            const float amt = juce::jlimit (0.0f, 1.0f, (envDb + 40.0f) / 28.0f); // -40 dB .. -12 dB
            const float duckGain = juce::Decibels::decibelsToGain (-12.0f * settings.duck * amt);
            const float wetL = (revGain * revL[(size_t) i] + echoGain * echoL[(size_t) i]) * duckGain;
            const float wetR = (revGain * revR[(size_t) i] + echoGain * echoR[(size_t) i]) * duckGain;
            if (c == 2) { b.getWritePointer (0)[i] += wetL; b.getWritePointer (1)[i] += wetR; }
            else        { b.getWritePointer (0)[i] += 0.5f * (wetL + wetR); }
        }

        for (int i = 0; i < n; ++i)
        {
            const float g = outGain.getNextValue();
            for (int ch = 0; ch < c; ++ch) b.getWritePointer (ch)[i] *= g;
        }
        limiter.process (b);   // last: protects the output ceiling after the output knob
        outMeter.store (peakDb (b));
    }

    static float peakDb (const juce::AudioBuffer<float>& b)
    {
        float m = 0.0f;
        for (int ch = 0; ch < b.getNumChannels(); ++ch) m = juce::jmax (m, b.getMagnitude (ch, 0, b.getNumSamples()));
        return juce::Decibels::gainToDecibels (m, -100.0f);
    }

    Settings settings;
    double sampleRate = 44100.0;
    int maxBlock = 512, chans = 2;

    juce::dsp::LinkwitzRileyFilter<float> lowCut;
    NoiseReducer noise;
    ResonanceSuppressor resonance;
    PitchCorrector tuner;
    ToneEQ tone;
    AutoLeveler leveler;
    Compressor comp;
    DeEsser deesser;
    Saturation saturation;
    Doubler doubler;
    EchoDelay echo;
    VocalReverb reverb;
    Limiter limiter;

    std::vector<float> send, echoL, echoR, revL, revR, revIn;
    juce::SmoothedValue<float> inGain { 1.0f }, outGain { 1.0f };
    float duckEnv = 0.0f;
    std::atomic<float> inMeter { -100.0f }, outMeter { -100.0f };
};
