# VocalOne by Durrani Studio — the whole vocal chain in one plugin

Put one plugin on your vocal track and you're done. Everything is already
routed inside it, in the order a mix engineer would set it up:

```
Your vocal
  -> 1 CLEAN     low cut (rumble) -> room-noise removal -> de-harsh (resonances)
  -> 2 TUNE      automatic pitch correction: natural or hard "robot" tune, any key/scale
  -> 3 TONE      warmth / clarity (mud cut) / presence / air / high cut
  -> 4 DYNAMICS  auto level (rides the fader) -> compressor (auto make-up)
  -> 5 DE-ESS    tames s / sh / t
  -> 6 COLOR     analog-style saturation (2x oversampled)
  -> 7 SPACE     width (doubler) + echo + reverb
                 (echo and reverb are built-in parallel sends: EQ'd,
                  tempo-synced, and they dip while you sing)
  -> 8 OUTPUT    output level -> brickwall limiter (never clips)
```

You don't need any mixer sends, extra reverb tracks, or sidechain routing.

## Install in FL Studio (Windows)

1. **Get the plugin file.** This folder is the source code. GitHub can build the plugin for free:
   - Make a free account on github.com, then click **New repository** (e.g. `vocalone`).
   - Click **uploading an existing file**, drag in the `VocalOne` folder **and** the `.github` folder, then click **Commit**.
     (`.github` is a hidden folder. In File Explorer, turn on View → Show → Hidden items.)
   - Open the **Actions** tab. The build takes about 10 minutes. When it shows a green check, open the run and download **VocalOne-VST3-Windows**.
2. **Unzip it** and copy the `VocalOne.vst3` folder into
   `C:\Program Files\Common Files\VST3\`
3. In FL Studio: **Options → Manage plugins → Find installed plugins**. When the scan finishes, find
   **VocalOne** (vendor: Durrani Studio) and click its ☆ star.
4. Route your vocal to its own **Mixer track**. Click an empty insert slot → **Select → VocalOne**.

## 30-second setup

1. **Pick a preset** from the menu at the top. Presets are grouped by style (see below), and picking one
   shows what it does in the bar at the bottom.
2. **Tuning:** if the preset uses it, set **Key** and **Scale** to match your beat. Not sure of the key?
   Leave the scale on **Chromatic**, which works for any song. Presets never change your key or scale.
3. Play a spot with **no singing**, click **LEARN NOISE**, wait about 2 seconds, then click it again.
   (If you don't have a quiet spot, leave **Auto** on.)
4. Adjust the big knobs to taste. **Hover over anything** and the bar at the bottom explains it.
   Double-click a knob to reset it. Each section's switch turns it on or off so you can compare.

## The Tune section

- **Speed 0 ms = hard tune.** Notes snap instantly: the robotic trap / T-Pain effect.
  **15–40 ms** is modern natural tuning. **80 ms or more** only corrects long notes and leaves vibrato alone.
- **Amount** sets how much of the correction is applied.
- **Scales:** Chromatic, Major, Minor, Harmonic Minor, Major/Minor Pentatonic, plus raag-based scales:
  **Bhairav** (S r G m P d N), **Kafi** (Dorian), **Bhairavi** (Phrygian) and **Yaman** (Lydian).
- The readout shows the note you sang, how many cents off it was, and the note it's being tuned to.
- It keeps the voice's natural character (formants), so there's no chipmunk sound.
- For qawwali, ghazal and classical singing, leave Tune **off**, as those presets do. Pitch correction
  flattens murki, meend and gamak.

## Presets

Every preset is named by **style**, not by artist. Wherever engineers have published how they process a
famous vocal sound, the preset copies that chain onto VocalOne's controls. The sources are interviews
in Sound On Sound's "Inside Track", MusicRadar, Universal Audio and Red Bull Music Academy.

| Category | Presets |
|---|---|
| Starters | Start Here (Natural) · Clean Only (no effects) |
| Hip-Hop / Rap | Atlanta Trap – Hard Tune · Melodic Trap – Dark & Spacey · Toronto Melodic Rap – Upfront · Conscious Rap – Raw & Present · Drill – Layered & Tight · Rap Ad-Libs – Phone · Rap Doubles – Under the Lead |
| R&B / Pop | Arena Pop-Rap – Big & Wet · Whisper Pop – Intimate & Dry · Dark R&B – Lo-Fi Haze · Bright Pop Lead – Airy · Soul Ballad – Big & Natural |
| Desi (Urdu / Hindi / Punjabi / Pashto) | Bollywood Romantic Ballad · Punjabi Pop – Bright & Upfront · Desi R&B – Warm & Chill · Sufi / Qawwali – Live Hall · Ghazal – Warm & Close · Pashto Folk – Bright Echo · Naat / Nasheed – Pure Voice |
| Background Vocals | BGV – Stack Behind the Lead · BGV – Tight Harmonies · BGV – Choir / Pad (Far Away) · BGV – Hook Doubles (Wide) |
| Spoken Word | Podcast / Voice-over · YouTube Vlog – Clear Voice |

How reliable each group is:
- **Documented chains.** The Atlanta trap, Toronto, conscious rap, drill, arena pop-rap, whisper pop,
  dark R&B, bright pop and soul ballad presets follow published engineer interviews. Where an interview
  gave numbers, the preset uses them. Examples: de-esser at 5.5 kHz; small room with 15 ms pre-delay at
  about 15% wet; top boost above 5 kHz; narrow resonance notches; fast plus slow compression; tape echo
  as the main effect.
- **Background vocals** follow standard published technique:
  - high-pass at 150–200 Hz
  - de-ess much harder than the lead
  - tuning below half speed so layers don't phase
  - less reverb, with a high cut to push them further back
- **Desi presets.** No engineer has published their settings for these genres. These presets are built
  from how the genres typically sound and from general engineering practice. Treat them as starting points.
- **Melodic Trap – Dark & Spacey** combines the documented hard-tune technique with the sound you hear on
  those records. That artist's engineer hasn't published their exact settings.

Presets get you in the right area. Your own voice, mic and room still matter, so adjust by ear.

## Things to know

- **It works on quiet or loud recordings.** Auto Level evens the level before the compressor, and the
  de-esser works on the balance of the voice, not a fixed volume. So the presets behave the same
  whatever your recording level.
- **Delay:** noise cleanup and tuning need about 120 ms of look-ahead. The plugin tells FL Studio,
  and FL compensates automatically, so everything stays in time on playback and export. Because of
  the delay, use it on recorded vocals, not as a live monitoring effect while you record.
- **Tempo:** Echo follows your FL Studio project tempo automatically.
- **Noise removal** handles steady noise (AC, fans, hiss, hum, computer noise). It does not remove
  sudden noises (barking, traffic, people talking) or room echo.

## Tested

The DSP test suite (`Tests/dsp_tests.cpp`, 82 checks, runs on every GitHub build) checks the
following against synthetic signals:
- **Tuning:**
  - a note sung 35 cents sharp ends up within 0.3 cents of the note, with no level change
  - the correct note is chosen for each key and scale
  - hard tune holds vibrato to ±4 cents and sits exactly on a note 96% of the time during a slide
  - slow tuning keeps full vibrato
  - breaths pass through unchanged
  - switched off, the signal is bit-exact
- **Cleanup:**
  - noise removal (~15 dB)
  - harshness removal
  - de-essing that works the same on quiet and loud takes
- **Levels and dynamics:**
  - Auto Level reaching its target
  - compression evening out loud and quiet words
- **Tone:**
  - every EQ band's gain
  - saturation harmonics
- **Output and effects:**
  - the limiter never passing the ceiling
  - the echo landing on the beat
  - the reported latency matching the measured latency
- **Presets:**
  - all 27 are valid
  - on loud and quiet takes they stay stable, under −1 dB, and at a similar output level

## Building it yourself (developers)

```bash
git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git VocalOne/JUCE
cmake -S VocalOne -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target VocalOne_VST3
```

## License note

VocalOne is built on JUCE 8. JUCE is free for personal use under the AGPLv3. If you ever want to
**sell or give away** VocalOne as a closed-source product, you'll need a JUCE commercial license
(or to publish the source under AGPLv3).

## Code layout

```
Source/
  VocalChain.h         the routing: every stage in order, sends, ducking, latency
  Params.h             every parameter (range/default) + the factory preset library
  PluginProcessor.*    FL Studio/VST3 glue: parameters, presets, tempo, saving the noise print
  PluginEditor.*       the Easy and Detail pages
  dsp/                 NoiseReducer, ResonanceSuppressor, PitchCorrector (YIN + PSOLA), DeEsser,
                       Dynamics (leveler, compressor, limiter), ToneColor (EQ, high cut, saturation),
                       Space (doubler, echo, reverb)
Tests/                 dsp_tests (headless checks), ui_snapshot (renders the window to PNG)
```
