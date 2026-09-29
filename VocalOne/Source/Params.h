#pragma once
#include "VocalChain.h"
#include <functional>
#include <map>
#include <string>
#include <vector>

// One table describes every parameter: the plugin builds its host parameters
// from it, presets refer to it by id, and tests use its defaults.
namespace params
{
    enum class Type { Float, Bool, Choice };

    struct Def
    {
        const char* id;
        const char* name;
        Type type;
        float min, max, def, skew;
        const char* unit;
        const char* choices = nullptr; // "|"-separated, Choice params only
    };

    inline juce::StringArray choicesOf (const Def& d)
    {
        return juce::StringArray::fromTokens (d.choices != nullptr ? d.choices : "", "|", "");
    }

    inline const std::vector<Def>& all()
    {
        static const std::vector<Def> defs {
            { "inputDb",     "Input",            Type::Float, -24.0f, 24.0f, 0.0f, 1.0f, "dB" },
            { "outputDb",    "Output",           Type::Float, -24.0f, 24.0f, 0.0f, 1.0f, "dB" },

            { "cleanOn",     "Clean On",         Type::Bool, 0, 1, 1, 1, "" },
            { "lowCutOn",    "Low Cut On",       Type::Bool, 0, 1, 1, 1, "" },
            { "lowCutHz",    "Low Cut",          Type::Float, 20.0f, 300.0f, 90.0f, 0.5f, "Hz" },
            { "noiseDb",     "Noise Removal",    Type::Float, 0.0f, 30.0f, 10.0f, 1.0f, "dB" },
            { "noiseThresh", "Noise Threshold",  Type::Float, -6.0f, 20.0f, 6.0f, 1.0f, "dB" },
            { "noiseLearn",  "Learn Noise",      Type::Bool, 0, 1, 0, 1, "" },
            { "noiseAuto",   "Auto Noise",       Type::Bool, 0, 1, 1, 1, "" },
            { "resonance",   "De-Harsh",         Type::Float, 0.0f, 1.0f, 0.4f, 1.0f, "" },
            { "resSharp",    "Harsh Focus",      Type::Float, 0.0f, 1.0f, 0.5f, 1.0f, "" },

            { "tuneOn",      "Tune On",          Type::Bool, 0, 1, 0, 1, "" },
            { "tuneAmount",  "Tune Amount",      Type::Float, 0.0f, 1.0f, 1.0f, 1.0f, "" },
            { "tuneSpeed",   "Tune Speed",       Type::Float, 0.0f, 200.0f, 25.0f, 0.45f, "ms" },
            { "tuneKey",     "Key",              Type::Choice, 0, 11, 0, 1, "", "C|C#/Db|D|D#/Eb|E|F|F#/Gb|G|G#/Ab|A|A#/Bb|B" },
            { "tuneScale",   "Scale",            Type::Choice, 0, 9, 0, 1, "",
              "Chromatic (any key)|Major|Minor|Harmonic Minor|Major Pentatonic|Minor Pentatonic|Bhairav (S r G m P d N)|Kafi / Dorian|Bhairavi / Phrygian|Yaman / Lydian" },

            { "toneOn",      "Tone On",          Type::Bool, 0, 1, 1, 1, "" },
            { "warmth",      "Warmth",           Type::Float, -6.0f, 6.0f, 0.0f, 1.0f, "dB" },
            { "clarity",     "Clarity",          Type::Float, 0.0f, 1.0f, 0.3f, 1.0f, "" },
            { "presence",    "Presence",         Type::Float, -6.0f, 8.0f, 2.0f, 1.0f, "dB" },
            { "air",         "Air",              Type::Float, 0.0f, 10.0f, 3.0f, 1.0f, "dB" },
            { "hiCutHz",     "High Cut",         Type::Float, 2000.0f, 20000.0f, 20000.0f, 0.4f, "Hz" },

            { "dynOn",       "Dynamics On",      Type::Bool, 0, 1, 1, 1, "" },
            { "autoLevel",   "Auto Level",       Type::Bool, 0, 1, 1, 1, "" },
            { "levelTarget", "Level Target",     Type::Float, -26.0f, -10.0f, -18.0f, 1.0f, "dB" },
            { "compress",    "Compress",         Type::Float, 0.0f, 1.0f, 0.4f, 1.0f, "" },
            { "compAttack",  "Comp Attack",      Type::Float, 1.0f, 50.0f, 10.0f, 0.5f, "ms" },
            { "compRelease", "Comp Release",     Type::Float, 30.0f, 500.0f, 120.0f, 0.5f, "ms" },

            { "deessOn",     "De-Ess On",        Type::Bool, 0, 1, 1, 1, "" },
            { "deess",       "De-Ess",           Type::Float, 0.0f, 1.0f, 0.5f, 1.0f, "" },
            { "deessFreq",   "De-Ess Freq",      Type::Float, 3000.0f, 10000.0f, 6500.0f, 0.5f, "Hz" },
            { "deessListen", "De-Ess Listen",    Type::Bool, 0, 1, 0, 1, "" },

            { "colorOn",     "Color On",         Type::Bool, 0, 1, 1, 1, "" },
            { "saturation",  "Saturation",       Type::Float, 0.0f, 1.0f, 0.15f, 1.0f, "" },

            { "spaceOn",     "Space On",         Type::Bool, 0, 1, 1, 1, "" },
            { "width",       "Width",            Type::Float, 0.0f, 1.0f, 0.2f, 1.0f, "" },
            { "reverb",      "Reverb",           Type::Float, 0.0f, 1.0f, 0.15f, 1.0f, "" },
            { "reverbSize",  "Reverb Size",      Type::Float, 0.0f, 1.0f, 0.5f, 1.0f, "" },
            { "reverbPre",   "Reverb Pre-Delay", Type::Float, 0.0f, 150.0f, 30.0f, 1.0f, "ms" },
            { "echo",        "Echo",             Type::Float, 0.0f, 1.0f, 0.0f, 1.0f, "" },
            { "echoFeedback","Echo Repeats",     Type::Float, 0.0f, 0.85f, 0.3f, 1.0f, "" },
            { "echoDiv",     "Echo Time",        Type::Choice, 0, 5, 1, 1, "", "1/4|1/8 dotted|1/8|1/16|1/4 dotted|1/2" },
            { "echoPing",    "Echo Ping-Pong",   Type::Bool, 0, 1, 1, 1, "" },
            { "duck",        "FX Ducking",       Type::Float, 0.0f, 1.0f, 0.5f, 1.0f, "" },

            { "limiterOn",   "Limiter On",       Type::Bool, 0, 1, 1, 1, "" },
            { "ceiling",     "Ceiling",          Type::Float, -6.0f, 0.0f, -1.0f, 1.0f, "dB" },
        };
        return defs;
    }

    using Getter = std::function<float (const char*)>;

    // Maps parameter values (real units) to the DSP settings.
    inline VocalChain::Settings makeSettings (const Getter& v, double bpm)
    {
        VocalChain::Settings s;
        auto b = [&] (const char* id) { return v (id) > 0.5f; };
        s.inputDb = v ("inputDb");        s.outputDb = v ("outputDb");
        s.cleanOn = b ("cleanOn");        s.lowCutOn = b ("lowCutOn");   s.lowCutHz = v ("lowCutHz");
        s.noiseDb = v ("noiseDb");        s.noiseThreshDb = v ("noiseThresh");
        s.noiseLearn = b ("noiseLearn");  s.noiseAuto = b ("noiseAuto");
        s.resonance = v ("resonance");    s.resSharpness = v ("resSharp");
        s.toneOn = b ("toneOn");          s.warmthDb = v ("warmth");     s.clarity = v ("clarity");
        s.presenceDb = v ("presence");    s.airDb = v ("air");           s.hiCutHz = v ("hiCutHz");
        s.tuneOn = b ("tuneOn");          s.tuneAmount = v ("tuneAmount"); s.tuneSpeedMs = v ("tuneSpeed");
        s.tuneKey = (int) std::lround (v ("tuneKey"));  s.tuneScale = (int) std::lround (v ("tuneScale"));
        s.dynOn = b ("dynOn");            s.autoLevel = b ("autoLevel"); s.levelTargetDb = v ("levelTarget");
        s.compress = v ("compress");      s.compAttackMs = v ("compAttack"); s.compReleaseMs = v ("compRelease");
        s.deessOn = b ("deessOn");        s.deess = v ("deess");         s.deessFreq = v ("deessFreq");
        s.deessListen = b ("deessListen");
        s.colorOn = b ("colorOn");        s.saturation = v ("saturation");
        s.spaceOn = b ("spaceOn");        s.width = v ("width");
        s.reverb = v ("reverb");          s.reverbSize = v ("reverbSize"); s.reverbPreMs = v ("reverbPre");
        s.echo = v ("echo");              s.echoFeedback = v ("echoFeedback");
        s.echoDivision = (int) std::lround (v ("echoDiv"));  s.echoPingPong = b ("echoPing");
        s.duck = v ("duck");
        s.limiterOn = b ("limiterOn");    s.ceilingDb = v ("ceiling");
        s.bpm = bpm;
        return s;
    }

    inline float defaultOf (const std::string& id)
    {
        for (auto& d : all()) if (id == d.id) return d.def;
        return 0.0f;
    }
}

// ---------------------------------------------------------------------------
// Factory presets, grouped by style. Each one is built from what engineers of
// that sound have published about their vocal chains (see README), translated
// onto VocalOne's controls. Anything not listed goes back to its default, so
// every preset is a complete, predictable starting point.
// Presets never touch: the learned noise print, Learn Noise, or the song's
// Key/Scale (those belong to your song, not to a style).
// ---------------------------------------------------------------------------
namespace presets
{
    struct Preset
    {
        const char* category;
        const char* name;
        const char* about;   // one line shown when the preset is picked
        std::vector<std::pair<const char*, float>> values;
    };

    inline bool presetControlsParam (const std::string& id)
    {
        return id != "noiseLearn" && id != "tuneKey" && id != "tuneScale";
    }

    inline const std::vector<Preset>& all()
    {
        static const std::vector<Preset> list {
            // ------------------------------------------------------------ STARTERS
            { "STARTERS", "Start Here (Natural)", "A balanced, natural vocal. Good first step for any song.", {} },
            { "STARTERS", "Clean Only (no effects)", "Only cleanup: rumble, room noise, harshness and 's' sounds. No tone, compression or effects.", {
                { "warmth", 0 }, { "clarity", 0 }, { "presence", 0 }, { "air", 0 },
                { "compress", 0 }, { "saturation", 0 }, { "width", 0 }, { "reverb", 0 }, { "echo", 0 } } },

            // ------------------------------------------------------------ HIP-HOP / RAP
            { "HIP-HOP / RAP", "Atlanta Trap - Hard Tune",
              "Instant tuning, low end shelved off, top lifted above 5 kHz, de-ess at 5.5 kHz, small room (15 ms pre-delay, ~15% wet), a touch of delay.", {
                { "tuneOn", 1 }, { "tuneSpeed", 0 }, { "tuneAmount", 1 },
                { "lowCutHz", 100 }, { "warmth", -2 }, { "clarity", 0.3f }, { "presence", 1.5f }, { "air", 5 },
                { "deess", 0.6f }, { "deessFreq", 5500 }, { "compress", 0.5f }, { "compAttack", 10 }, { "compRelease", 120 },
                { "saturation", 0.15f }, { "width", 0.12f },
                { "reverb", 0.28f }, { "reverbSize", 0.3f }, { "reverbPre", 15 },
                { "echo", 0.12f }, { "echoDiv", 2 }, { "echoFeedback", 0.2f }, { "duck", 0.4f } } },
            { "HIP-HOP / RAP", "Melodic Trap - Dark & Spacey",
              "Hard tuning, darker top, gritty saturation, long dotted echoes and a big dark space that blooms between lines.", {
                { "tuneOn", 1 }, { "tuneSpeed", 0 }, { "tuneAmount", 1 },
                { "lowCutHz", 110 }, { "warmth", -1 }, { "clarity", 0.4f }, { "presence", 1 }, { "air", 2 }, { "hiCutHz", 12000 },
                { "saturation", 0.45f }, { "compress", 0.6f }, { "compAttack", 5 }, { "compRelease", 90 }, { "deess", 0.6f },
                { "width", 0.3f }, { "reverb", 0.5f }, { "reverbSize", 0.85f }, { "reverbPre", 25 },
                { "echo", 0.3f }, { "echoDiv", 1 }, { "echoFeedback", 0.45f }, { "echoPing", 1 }, { "duck", 0.7f } } },
            { "HIP-HOP / RAP", "Toronto Melodic Rap - Upfront",
              "Light tuning to centre melodic rap, gentle opto-style compression, low-end and top lift, loud and in your face with only a little space.", {
                { "tuneOn", 1 }, { "tuneSpeed", 45 }, { "tuneAmount", 0.7f },
                { "warmth", 2 }, { "clarity", 0.35f }, { "presence", 1.5f }, { "air", 3 }, { "deess", 0.55f },
                { "compress", 0.4f }, { "compAttack", 12 }, { "compRelease", 160 }, { "saturation", 0.1f }, { "width", 0.08f },
                { "reverb", 0.12f }, { "reverbSize", 0.45f }, { "reverbPre", 30 },
                { "echo", 0.1f }, { "echoDiv", 0 }, { "echoFeedback", 0.2f }, { "duck", 0.6f } } },
            { "HIP-HOP / RAP", "Conscious Rap - Raw & Present",
              "No tuning. Firm compression on the mids, grit and bite on top, opened-up stereo and a plate-like space.", {
                { "lowCutHz", 100 }, { "warmth", 0.5f }, { "clarity", 0.45f }, { "presence", 3.5f }, { "air", 3 },
                { "deess", 0.55f }, { "compress", 0.65f }, { "compAttack", 8 }, { "compRelease", 100 },
                { "saturation", 0.3f }, { "width", 0.22f },
                { "reverb", 0.15f }, { "reverbSize", 0.6f }, { "reverbPre", 20 }, { "echo", 0 } } },
            { "HIP-HOP / RAP", "Drill - Layered & Tight",
              "Resonance smoothing, fast punchy compression, light chorus/flange, small tight room and short tape-style echo.", {
                { "resonance", 0.6f }, { "tuneOn", 1 }, { "tuneSpeed", 20 }, { "tuneAmount", 0.5f },
                { "lowCutHz", 110 }, { "warmth", -0.5f }, { "clarity", 0.4f }, { "presence", 2.5f }, { "air", 3.5f },
                { "deess", 0.65f }, { "compress", 0.65f }, { "compAttack", 3 }, { "compRelease", 60 },
                { "saturation", 0.25f }, { "width", 0.2f },
                { "reverb", 0.16f }, { "reverbSize", 0.35f }, { "reverbPre", 10 },
                { "echo", 0.14f }, { "echoDiv", 2 }, { "echoFeedback", 0.25f }, { "duck", 0.6f } } },
            { "HIP-HOP / RAP", "Rap Ad-Libs - Phone",
              "Telephone-style band-limited ad-libs: tuned hard, crunchy, wide, with a bouncing echo. Sits behind the lead.", {
                { "tuneOn", 1 }, { "tuneSpeed", 10 }, { "tuneAmount", 1 },
                { "lowCutHz", 300 }, { "hiCutHz", 3500 }, { "warmth", -3 }, { "clarity", 0.5f }, { "presence", 5 }, { "air", 0 },
                { "saturation", 0.5f }, { "compress", 0.7f }, { "width", 0.6f },
                { "reverb", 0.2f }, { "reverbPre", 10 }, { "echo", 0.25f }, { "echoDiv", 2 }, { "echoFeedback", 0.3f }, { "echoPing", 1 },
                { "outputDb", -4 } } },
            { "HIP-HOP / RAP", "Rap Doubles - Under the Lead",
              "For the double of your main verse: thinner, heavily de-essed, wide and quieter so the lead stays on top.", {
                { "tuneOn", 1 }, { "tuneSpeed", 20 }, { "tuneAmount", 0.8f },
                { "lowCutHz", 160 }, { "hiCutHz", 10000 }, { "clarity", 0.5f }, { "presence", 0 }, { "air", 1 },
                { "deess", 0.85f }, { "compress", 0.7f }, { "saturation", 0.1f }, { "width", 0.8f },
                { "reverb", 0.05f }, { "echo", 0 }, { "outputDb", -6 } } },

            // ------------------------------------------------------------ R&B / POP
            { "R&B / POP", "Arena Pop-Rap - Big & Wet",
              "Light tuning to keep it tight, warm fast compression, bright wide top end and a lot of big reverb.", {
                { "tuneOn", 1 }, { "tuneSpeed", 30 }, { "tuneAmount", 0.6f },
                { "warmth", 1 }, { "clarity", 0.3f }, { "presence", 2 }, { "air", 6 }, { "deess", 0.55f },
                { "compress", 0.55f }, { "compAttack", 5 }, { "compRelease", 100 }, { "saturation", 0.25f }, { "width", 0.35f },
                { "reverb", 0.5f }, { "reverbSize", 0.85f }, { "reverbPre", 35 },
                { "echo", 0.15f }, { "echoDiv", 0 }, { "echoFeedback", 0.3f }, { "duck", 0.55f } } },
            { "R&B / POP", "Whisper Pop - Intimate & Dry",
              "Soft close-mic vocal: strong noise removal, no tuning, warm and present, almost no reverb - like singing right next to you.", {
                { "noiseDb", 18 }, { "lowCutHz", 70 },
                { "warmth", 2.5f }, { "clarity", 0.2f }, { "presence", 1 }, { "air", 4 }, { "deess", 0.65f },
                { "compress", 0.5f }, { "compAttack", 15 }, { "compRelease", 150 }, { "saturation", 0.05f }, { "width", 0 },
                { "reverb", 0.06f }, { "reverbSize", 0.25f }, { "reverbPre", 5 }, { "echo", 0 } } },
            { "R&B / POP", "Dark R&B - Lo-Fi Haze",
              "Tuned, top and bottom rolled off for a lo-fi feel, fuzz and pumping compression, doubled and moving, dark reverb and tape echo.", {
                { "tuneOn", 1 }, { "tuneSpeed", 15 }, { "tuneAmount", 0.8f },
                { "lowCutHz", 150 }, { "hiCutHz", 9000 }, { "warmth", 0 }, { "clarity", 0.35f }, { "presence", 1.5f }, { "air", 0 },
                { "deess", 0.7f }, { "compress", 0.8f }, { "compAttack", 5 }, { "compRelease", 80 }, { "saturation", 0.45f },
                { "width", 0.5f }, { "reverb", 0.45f }, { "reverbSize", 0.75f }, { "reverbPre", 40 },
                { "echo", 0.22f }, { "echoDiv", 0 }, { "echoFeedback", 0.35f }, { "duck", 0.5f } } },
            { "R&B / POP", "Bright Pop Lead - Airy",
              "Big-voice pop: little EQ, two-stage style compression, natural-speed tuning, airy reverb and a soft dotted echo.", {
                { "tuneOn", 1 }, { "tuneSpeed", 25 }, { "tuneAmount", 0.7f },
                { "lowCutHz", 100 }, { "warmth", 0 }, { "clarity", 0.15f }, { "presence", 1 }, { "air", 3 }, { "deess", 0.55f },
                { "compress", 0.6f }, { "compAttack", 6 }, { "compRelease", 120 }, { "saturation", 0.1f }, { "width", 0.15f },
                { "reverb", 0.3f }, { "reverbSize", 0.6f }, { "reverbPre", 45 },
                { "echo", 0.12f }, { "echoDiv", 1 }, { "echoFeedback", 0.25f }, { "duck", 0.6f } } },
            { "R&B / POP", "Soul Ballad - Big & Natural",
              "Powerhouse ballad: harsh peaks notched out, extra de-essing, fast + slow compression, low and top lift, tape echo as the main effect.", {
                { "resonance", 0.6f }, { "deess", 0.65f },
                { "warmth", 2 }, { "clarity", 0.25f }, { "presence", 1 }, { "air", 5 },
                { "compress", 0.6f }, { "compAttack", 3 }, { "compRelease", 150 }, { "saturation", 0.15f }, { "width", 0.1f },
                { "reverb", 0.15f }, { "reverbSize", 0.6f }, { "reverbPre", 30 },
                { "echo", 0.2f }, { "echoDiv", 0 }, { "echoFeedback", 0.3f }, { "echoPing", 0 }, { "duck", 0.5f } } },

            // ------------------------------------------------------------ DESI
            { "DESI (URDU / HINDI / PUNJABI / PASHTO)", "Bollywood Romantic Ballad",
              "Sweet, bright playback-singer sound: gentle natural tuning, airy top, lush long reverb and a soft ping-pong echo.", {
                { "tuneOn", 1 }, { "tuneSpeed", 50 }, { "tuneAmount", 0.6f },
                { "warmth", 1.5f }, { "clarity", 0.3f }, { "presence", 2.5f }, { "air", 5 }, { "deess", 0.55f },
                { "compress", 0.5f }, { "compAttack", 10 }, { "compRelease", 140 }, { "saturation", 0.1f }, { "width", 0.25f },
                { "reverb", 0.45f }, { "reverbSize", 0.75f }, { "reverbPre", 50 },
                { "echo", 0.18f }, { "echoDiv", 0 }, { "echoFeedback", 0.3f }, { "echoPing", 1 }, { "duck", 0.55f } } },
            { "DESI (URDU / HINDI / PUNJABI / PASHTO)", "Punjabi Pop - Bright & Upfront",
              "Energetic, loud and forward to cut through dhol and bass: strong presence, punchy compression, short echo.", {
                { "tuneOn", 1 }, { "tuneSpeed", 25 }, { "tuneAmount", 0.7f },
                { "lowCutHz", 100 }, { "warmth", 0.5f }, { "clarity", 0.35f }, { "presence", 4 }, { "air", 4 }, { "deess", 0.6f },
                { "compress", 0.65f }, { "compAttack", 8 }, { "compRelease", 100 }, { "saturation", 0.25f }, { "width", 0.15f },
                { "reverb", 0.22f }, { "reverbSize", 0.5f }, { "reverbPre", 25 },
                { "echo", 0.15f }, { "echoDiv", 2 }, { "echoFeedback", 0.25f }, { "duck", 0.6f } } },
            { "DESI (URDU / HINDI / PUNJABI / PASHTO)", "Desi R&B - Warm & Chill",
              "Modern Punjabi/Urdu R&B: tuned smooth, warm and slightly dark, wide, with a laid-back reverb and dotted echo.", {
                { "tuneOn", 1 }, { "tuneSpeed", 15 }, { "tuneAmount", 0.85f },
                { "warmth", 2 }, { "clarity", 0.35f }, { "presence", 1.5f }, { "air", 3 }, { "hiCutHz", 13000 },
                { "saturation", 0.3f }, { "compress", 0.55f }, { "deess", 0.55f }, { "width", 0.35f },
                { "reverb", 0.38f }, { "reverbSize", 0.7f }, { "reverbPre", 35 },
                { "echo", 0.2f }, { "echoDiv", 4 }, { "echoFeedback", 0.35f }, { "duck", 0.6f } } },
            { "DESI (URDU / HINDI / PUNJABI / PASHTO)", "Sufi / Qawwali - Live Hall",
              "No tuning (it would flatten murki and meend). Light compression to keep the power and dynamics, big natural hall.", {
                { "noiseDb", 8 }, { "resonance", 0.35f },
                { "warmth", 1 }, { "clarity", 0.3f }, { "presence", 2 }, { "air", 2.5f }, { "deess", 0.4f },
                { "compress", 0.3f }, { "compAttack", 20 }, { "compRelease", 200 }, { "saturation", 0.1f }, { "width", 0.2f },
                { "reverb", 0.45f }, { "reverbSize", 0.85f }, { "reverbPre", 30 },
                { "echo", 0.08f }, { "echoDiv", 0 }, { "echoFeedback", 0.2f }, { "duck", 0.4f } } },
            { "DESI (URDU / HINDI / PUNJABI / PASHTO)", "Ghazal - Warm & Close",
              "Intimate and warm, no tuning so every ornament stays, slow gentle compression and a soft plate-like space.", {
                { "warmth", 2.5f }, { "clarity", 0.25f }, { "presence", 1 }, { "air", 2 }, { "deess", 0.5f },
                { "compress", 0.45f }, { "compAttack", 15 }, { "compRelease", 180 }, { "saturation", 0.15f }, { "width", 0.05f },
                { "reverb", 0.3f }, { "reverbSize", 0.55f }, { "reverbPre", 40 },
                { "echo", 0.05f }, { "echoDiv", 0 }, { "echoFeedback", 0.2f }, { "duck", 0.5f } } },
            { "DESI (URDU / HINDI / PUNJABI / PASHTO)", "Pashto Folk - Bright Echo",
              "Bright, forward folk vocal with the long bouncing echo typical of Pashto songs. Light tuning only, so ornaments survive.", {
                { "tuneOn", 1 }, { "tuneSpeed", 40 }, { "tuneAmount", 0.4f },
                { "warmth", 0 }, { "clarity", 0.35f }, { "presence", 3.5f }, { "air", 4 }, { "deess", 0.5f },
                { "compress", 0.55f }, { "saturation", 0.15f }, { "width", 0.2f },
                { "reverb", 0.35f }, { "reverbSize", 0.65f }, { "reverbPre", 30 },
                { "echo", 0.3f }, { "echoDiv", 1 }, { "echoFeedback", 0.45f }, { "echoPing", 1 }, { "duck", 0.5f } } },
            { "DESI (URDU / HINDI / PUNJABI / PASHTO)", "Naat / Nasheed - Pure Voice",
              "Voice-only devotional sound: extra noise cleanup, gentle tuning, warm and clear, with a wide, reverent space.", {
                { "noiseDb", 14 }, { "tuneOn", 1 }, { "tuneSpeed", 40 }, { "tuneAmount", 0.6f },
                { "warmth", 1.5f }, { "clarity", 0.3f }, { "presence", 2 }, { "air", 4 }, { "deess", 0.55f },
                { "compress", 0.5f }, { "saturation", 0.05f }, { "width", 0.3f },
                { "reverb", 0.5f }, { "reverbSize", 0.8f }, { "reverbPre", 40 },
                { "echo", 0.1f }, { "echoDiv", 0 }, { "echoFeedback", 0.25f }, { "duck", 0.5f } } },

            // ------------------------------------------------------------ BACKGROUND VOCALS
            { "BACKGROUND VOCALS", "BGV - Stack Behind the Lead",
              "High-passed at 180 Hz, top softened, heavy de-essing, slower tuning so layers don't phase, wide and 6 dB down.", {
                { "tuneOn", 1 }, { "tuneSpeed", 60 }, { "tuneAmount", 0.7f },
                { "lowCutHz", 180 }, { "hiCutHz", 11000 }, { "clarity", 0.4f }, { "presence", -1 }, { "air", 2 },
                { "deess", 0.85f }, { "compress", 0.45f }, { "saturation", 0.05f }, { "width", 0.7f },
                { "reverb", 0.1f }, { "reverbPre", 0 }, { "echo", 0 }, { "outputDb", -6 } } },
            { "BACKGROUND VOCALS", "BGV - Tight Harmonies",
              "For harmony parts: tuned tighter, bright but de-essed hard, very wide, a little room to blend.", {
                { "tuneOn", 1 }, { "tuneSpeed", 40 }, { "tuneAmount", 0.8f },
                { "lowCutHz", 160 }, { "hiCutHz", 14000 }, { "clarity", 0.4f }, { "presence", 0 }, { "air", 3 },
                { "deess", 0.8f }, { "compress", 0.5f }, { "width", 0.9f },
                { "reverb", 0.2f }, { "reverbSize", 0.6f }, { "reverbPre", 10 }, { "echo", 0 }, { "outputDb", -4 } } },
            { "BACKGROUND VOCALS", "BGV - Choir / Pad (Far Away)",
              "Pushes vocals far behind: dark top, thin bottom, widest spread, long reverb with no pre-delay.", {
                { "tuneOn", 1 }, { "tuneSpeed", 80 }, { "tuneAmount", 0.6f },
                { "lowCutHz", 200 }, { "hiCutHz", 8000 }, { "clarity", 0.3f }, { "presence", -3 }, { "air", 0 },
                { "deess", 0.9f }, { "compress", 0.4f }, { "width", 1 },
                { "reverb", 0.6f }, { "reverbSize", 0.9f }, { "reverbPre", 0 },
                { "echo", 0.1f }, { "echoDiv", 0 }, { "echoFeedback", 0.4f }, { "duck", 0.3f }, { "outputDb", -8 } } },
            { "BACKGROUND VOCALS", "BGV - Hook Doubles (Wide)",
              "Chorus/hook doubles that make the hook huge: full width, tuned, compressed, slightly below the lead.", {
                { "tuneOn", 1 }, { "tuneSpeed", 30 }, { "tuneAmount", 0.8f },
                { "lowCutHz", 140 }, { "clarity", 0.35f }, { "presence", 1 }, { "air", 3 },
                { "deess", 0.75f }, { "compress", 0.6f }, { "width", 1 },
                { "reverb", 0.15f }, { "reverbSize", 0.5f }, { "reverbPre", 20 }, { "echo", 0 }, { "outputDb", -3 } } },

            // ------------------------------------------------------------ SPOKEN
            { "SPOKEN WORD", "Podcast / Voice-over", "Clear, even, close voice with no effects.", {
                { "lowCutHz", 80 }, { "noiseDb", 15 }, { "resonance", 0.5f },
                { "warmth", 1.5f }, { "clarity", 0.4f }, { "presence", 2 }, { "air", 1.5f },
                { "levelTarget", -16 }, { "compress", 0.65f }, { "deess", 0.6f }, { "saturation", 0.1f },
                { "width", 0 }, { "reverb", 0 }, { "echo", 0 } } },
            { "SPOKEN WORD", "YouTube Vlog - Clear Voice", "Strong room-noise removal and clarity for camera or phone mics.", {
                { "noiseDb", 18 }, { "lowCutHz", 100 }, { "resonance", 0.5f },
                { "warmth", 1 }, { "clarity", 0.45f }, { "presence", 3 }, { "air", 2 },
                { "levelTarget", -16 }, { "compress", 0.7f }, { "deess", 0.6f }, { "saturation", 0.05f },
                { "width", 0 }, { "reverb", 0 }, { "echo", 0 } } },
        };
        return list;
    }

    // full value map for a preset (defaults + overrides), real units
    inline std::map<std::string, float> valuesFor (const Preset& p)
    {
        std::map<std::string, float> m;
        for (auto& d : params::all()) m[d.id] = d.def;
        for (auto& [id, v] : p.values) m[id] = v;
        return m;
    }
}
