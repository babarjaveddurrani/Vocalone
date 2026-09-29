#include "PluginEditor.h"

using namespace ui;

// ============================================================================ look & feel
LookAndFeel::LookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, bg);
    setColour (juce::ComboBox::backgroundColourId, panel);
    setColour (juce::ComboBox::outlineColourId, panelEdge);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::arrowColourId, dim);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, panelEdge);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::TextButton::buttonColourId, panel);
    setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffe07b2a));
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::Label::textColourId, text);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float size = juce::jmin (area.getWidth(), area.getHeight());
    auto r = area.withSizeKeepingCentre (size, size);
    const float thick = juce::jmax (3.0f, size * 0.09f);
    const auto c = r.getCentre();
    const float rad = size * 0.5f - thick * 0.5f;
    const auto col = s.findColour (juce::Slider::rotarySliderFillColourId);
    const bool enabled = s.isEnabled();

    juce::Path track;
    track.addCentredArc (c.x, c.y, rad, rad, 0.0f, start, end, true);
    g.setColour (panelEdge.brighter (0.15f));
    g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
    const float angle = start + pos * (end - start);
    const float from = bipolar ? (start + end) * 0.5f : start;
    juce::Path val;
    val.addCentredArc (c.x, c.y, rad, rad, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
    g.setColour (enabled ? col : dim);
    g.strokePath (val, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float capR = rad - thick * 1.4f;
    g.setColour (panel.brighter (0.12f));
    g.fillEllipse (c.x - capR, c.y - capR, capR * 2, capR * 2);
    g.setColour (text);
    const juce::Point<float> tip (c.x + (capR - 4) * std::sin (angle), c.y - (capR - 4) * std::cos (angle));
    g.drawLine ({ c.getPointOnCircumference (capR * 0.25f, angle), tip }, 2.5f);
}

void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat();
    const bool on = b.getToggleState();
    const auto col = b.findColour (juce::ToggleButton::tickColourId);
    auto pill = r.removeFromLeft (30.0f).withSizeKeepingCentre (30.0f, 16.0f);
    g.setColour (on ? col : panelEdge.brighter (0.2f));
    g.fillRoundedRectangle (pill, 8.0f);
    const float knobX = on ? pill.getRight() - 14.0f : pill.getX() + 2.0f;
    g.setColour (on ? juce::Colours::white : dim);
    g.fillEllipse (knobX, pill.getY() + 2.0f, 12.0f, 12.0f);
    if (b.getButtonText().isNotEmpty())
    {
        g.setColour (highlighted ? text : text.withAlpha (0.85f));
        g.setFont (13.5f);
        g.drawText (b.getButtonText(), r.withTrimmedLeft (7.0f), juce::Justification::centredLeft);
    }
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& base, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto c = b.getToggleState() ? b.findColour (juce::TextButton::buttonOnColourId) : base;
    if (highlighted) c = c.brighter (0.08f);
    if (down) c = c.darker (0.1f);
    g.setColour (c);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (panelEdge.brighter (0.2f));
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
}

// ============================================================================ widgets
Knob::Knob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& label, juce::Colour c, bool bigKnob)
    : labelText (label), big (bigKnob)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, true, 90, 18);
    slider.setColour (juce::Slider::rotarySliderFillColourId, c);
    slider.setDoubleClickReturnValue (true, s.getParameterRange (id).convertFrom0to1 (s.getParameter (id)->getDefaultValue()));
    slider.setPopupDisplayEnabled (false, false, nullptr);
    if (auto* p = s.getParameter (id))
        if (auto* f = dynamic_cast<juce::AudioParameterFloat*> (p))
            slider.getProperties().set ("bipolar", f->range.start < 0.0f && f->range.end > 0.0f);
    addAndMakeVisible (slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
}

void Knob::resized()
{
    slider.setBounds (getLocalBounds().withTrimmedTop (big ? 20 : 17));
}

void Knob::paint (juce::Graphics& g)
{
    g.setColour (text);
    g.setFont (juce::FontOptions (big ? 14.5f : 13.0f, juce::Font::bold));
    g.drawText (labelText, getLocalBounds().removeFromTop (big ? 20 : 17), juce::Justification::centred);
}

Section::Section (const juce::String& t, const juce::String& s, juce::Colour c) : colour (c), title (t), step (s)
{
    power.setColour (juce::ToggleButton::tickColourId, c);
    power.onStateChange = [this]
    {
        // grey out the section's controls when it's switched off
        const bool on = power.getToggleState() || ! power.isVisible();
        for (auto* child : getChildren()) if (child != &power) child->setAlpha (on ? 1.0f : 0.4f);
        repaint();
    };
    addAndMakeVisible (power);
}

void Section::resized()
{
    power.setBounds (getWidth() - 44, 8, 34, 20);
}

void Section::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
    g.setColour (colour);
    g.fillRoundedRectangle (r.removeFromTop (4.0f).reduced (14.0f, 0.0f), 2.0f);

    int x = 12;
    if (step.isNotEmpty())
    {
        g.setColour (colour);
        g.fillEllipse ((float) x, 9.0f, 20.0f, 20.0f);
        g.setColour (bg);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (step, x, 9, 20, 20, juce::Justification::centred);
        x += 28;
    }
    g.setColour (power.isVisible() && ! power.getToggleState() ? dim : text);
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.drawText (title, x, 8, getWidth() - x - 50, 22, juce::Justification::centredLeft);
}

void Meter::tick()
{
    const float v = read();
    shown = reduction ? juce::jmin (v, shown * 0.85f + v * 0.15f + 0.0f) : juce::jmax (v, shown - 1.5f);
    if (reduction && v > shown) shown = shown + (v - shown) * 0.15f;
    repaint();
}

void Meter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (dim);
    g.setFont (12.0f);
    g.drawText (label, r.removeFromLeft (46.0f), juce::Justification::centredLeft);
    auto valueArea = r.removeFromRight (52.0f);
    auto bar = r.reduced (0.0f, r.getHeight() * 0.3f);
    g.setColour (bg);
    g.fillRoundedRectangle (bar, 3.0f);
    float frac;
    if (reduction) frac = juce::jlimit (0.0f, 1.0f, -shown / 18.0f);
    else           frac = juce::jlimit (0.0f, 1.0f, (shown + 60.0f) / 60.0f);
    g.setColour (reduction ? colour : (shown > -1.5f ? juce::Colour (0xffef5350) : colour));
    if (reduction) g.fillRoundedRectangle (bar.withTrimmedLeft (bar.getWidth() * (1.0f - frac)), 3.0f);
    else           g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * frac), 3.0f);
    g.setColour (text);
    g.drawText (reduction ? juce::String (shown, 1) + " dB" : (shown <= -99.0f ? juce::String ("-inf") : juce::String (shown, 1)),
                valueArea, juce::Justification::centredRight);
}

void Spectrum::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (r, 10.0f);
    auto plot = r.reduced (12.0f, 26.0f).withTrimmedBottom (-12.0f);
    auto& chain = proc.getChain();
    auto& nr = chain.getNoise();
    auto& rs = chain.getResonance();
    const float binHz = rs.getSampleRate() / (float) ResonanceSuppressor::fftSize;
    auto xFor = [&] (float hz) { return plot.getX() + plot.getWidth() * std::log10 (hz / 20.0f) / 3.0f; };

    g.setFont (11.0f);
    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawVerticalLine ((int) xFor (f), plot.getY(), plot.getBottom());
        g.setColour (dim);
        g.drawText (f >= 1000 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f), (int) xFor (f) - 15, (int) plot.getBottom() - 12, 30, 12, juce::Justification::centred);
    }

    auto curve = [&] (auto value, float lo, float hi)
    {
        juce::Path p; bool first = true;
        for (int b = 1; b < ResonanceSuppressor::numBins; ++b)
        {
            const float hz = (float) b * binHz;
            if (hz < 20.0f || hz > 20000.0f) continue;
            const float y = juce::jmap (juce::jlimit (lo, hi, value (b)), lo, hi, plot.getBottom() - 14.0f, plot.getY());
            if (first) { p.startNewSubPath (xFor (hz), y); first = false; } else p.lineTo (xFor (hz), y);
        }
        return p;
    };
    g.setColour (dim.withAlpha (0.7f));
    g.strokePath (curve ([&] (int b) { return nr.getDisplayNoiseDb (b); }, -30.0f, 70.0f), juce::PathStrokeType (1.2f));
    g.setColour (clean);
    g.strokePath (curve ([&] (int b) { return nr.getDisplayReductionDb (b); }, -30.0f, 0.0f), juce::PathStrokeType (1.6f));
    g.setColour (dyn);
    g.strokePath (curve ([&] (int b) { return rs.getDisplayReductionDb (b); }, -30.0f, 0.0f), juce::PathStrokeType (1.6f));

    g.setFont (11.0f);
    g.setColour (dim);
    g.drawText ("0 dB",  (int) plot.getX() + 2, (int) plot.getY() - 2, 40, 12, juce::Justification::left);
    g.drawText ("-30 dB", (int) plot.getX() + 2, (int) plot.getBottom() - 26, 44, 12, juce::Justification::left);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.setColour (text);  g.drawText ("WHAT'S BEING CLEANED", 14, 6, 200, 16, juce::Justification::left);
    g.setFont (12.0f);
    g.setColour (clean); g.drawText ("noise removed", 220, 6, 100, 16, juce::Justification::left);
    g.setColour (dyn);   g.drawText ("harshness removed", 320, 6, 120, 16, juce::Justification::left);
    g.setColour (dim);   g.drawText ("room noise print", 445, 6, 120, 16, juce::Justification::left);
}

// ============================================================================ editor
static const std::map<juce::String, juce::String>& helpTexts()
{
    static const std::map<juce::String, juce::String> m {
        { "noiseDb",     "NOISE REMOVAL - removes steady room noise (AC, fan, hiss). Click LEARN NOISE on a quiet part first for best results. 8-15 is natural." },
        { "resonance",   "DE-HARSH - finds and smooths ringing, honky or piercing frequencies in the voice as they happen." },
        { "warmth",      "WARMTH - adds (right) or removes (left) chest and body. Turn left if the vocal is boomy." },
        { "clarity",     "CLARITY - cuts the boxy/muddy zone around 300 Hz so the vocal sounds clean and expensive." },
        { "presence",    "PRESENCE - brings the words forward so they cut through the beat. Too much sounds harsh." },
        { "air",         "AIR - adds shine and breath on top. Pairs well with De-Ess." },
        { "compress",    "COMPRESS - evens out loud and quiet words so every line is heard. Loudness is kept the same automatically." },
        { "deess",       "DE-ESS - tames sharp 's', 'sh' and 't' sounds without dulling the voice. Works the same on quiet or loud recordings." },
        { "saturation",  "SATURATION - analog-style warmth and grit. A little makes the vocal thicker; a lot sounds lo-fi." },
        { "width",       "WIDTH - adds a subtle double on the left and right, like a second take. Keep low for lead vocals." },
        { "reverb",      "REVERB - puts the vocal in a room/hall. It automatically dips while you sing and blooms in the gaps." },
        { "echo",        "ECHO - delay repeats locked to your FL Studio tempo. Pick the rhythm under the knob." },
        { "outputDb",    "OUTPUT - final volume. The limiter always keeps the result from clipping." },
        { "lowCutHz",    "LOW CUT - removes rumble below this frequency (mic stand bumps, traffic, plosive thumps). 80-120 Hz for vocals." },
        { "noiseThresh", "NOISE THRESHOLD - raise if noise still gets through, lower if quiet word endings get cut off." },
        { "resSharp",    "HARSH FOCUS - higher only catches narrow, piercing peaks; lower smooths broader harsh areas." },
        { "levelTarget", "LEVEL TARGET - the level Auto Level rides the vocal to before the compressor." },
        { "compAttack",  "ATTACK - slower lets the start of each word punch through; faster is smoother." },
        { "compRelease", "RELEASE - how fast compression lets go between words." },
        { "deessFreq",   "DE-ESS FREQ - where the 's' sounds live. Lower for deep voices (~5k), higher for bright voices (~8k). Use Listen to find it." },
        { "reverbSize",  "REVERB SIZE - small room to big hall." },
        { "reverbPre",   "PRE-DELAY - a short gap before the reverb starts, keeps words clear. 20-50 ms is typical." },
        { "echoFeedback","ECHO REPEATS - how many times the echo repeats." },
        { "duck",        "FX DUCKING - how much reverb/echo dip while you're singing so they don't wash the words out." },
        { "inputDb",     "INPUT - only needed for extremely quiet or loud recordings; Auto Level handles normal ones." },
        { "ceiling",     "CEILING - the loudest the output can ever peak. -1 dB is safe for streaming." },
        { "tuneAmount",  "TUNE AMOUNT - how much of the pitch correction is applied. 100% = fully on the note." },
        { "tuneSpeed",   "TUNE SPEED - 0 ms = instant robotic hard-tune (trap / T-Pain effect). 15-40 ms = modern natural tuning. 80+ ms = only long notes get corrected and vibrato stays." },
        { "hiCutHz",     "HIGH CUT - rolls off the top end for darker, lo-fi or far-away vocals. 'Off' at the top." },
    };
    return m;
}

VocalOneEditor::VocalOneEditor (VocalOneProcessor& p)
    : AudioProcessorEditor (&p), proc (p), spectrum (p)
{
    setLookAndFeel (&lnf);
    defaultHelp = "Pick a preset, click LEARN NOISE during a quiet moment, then adjust the big knobs. Hover anything to see what it does. Double-click a knob to reset it.";
    helpTextForReset = defaultHelp;

    // ---- header ----
    {
        juce::String lastCategory;
        const auto& list = presets::all();
        for (int i = 0; i < (int) list.size(); ++i)
        {
            if (lastCategory != list[(size_t) i].category)
            {
                if (lastCategory.isNotEmpty()) presetBox.addSeparator();
                lastCategory = list[(size_t) i].category;
                presetBox.addSectionHeading (lastCategory);
            }
            presetBox.addItem (list[(size_t) i].name, i + 1);
        }
    }
    presetBox.onChange = [this]
    {
        const int i = presetBox.getSelectedId() - 1;
        if (i >= 0 && i != proc.getCurrentPresetIndex()) proc.applyPreset (i);
        showPresetInfo();
    };
    prevPreset.onClick = [this] { const int n = (int) presets::all().size(); proc.applyPreset ((proc.getCurrentPresetIndex() + n - 1) % n); refreshPresetBox(); showPresetInfo(); };
    nextPreset.onClick = [this] { const int n = (int) presets::all().size(); proc.applyPreset ((proc.getCurrentPresetIndex() + 1) % n); refreshPresetBox(); showPresetInfo(); };
    for (auto* t : { &easyTab, &detailTab })
    {
        t->setClickingTogglesState (false);
        t->setColour (juce::TextButton::buttonOnColourId, panelEdge.brighter (0.3f));
    }
    easyTab.setComponentID ("easyTab"); detailTab.setComponentID ("detailTab");
    easyTab.onClick = [this] { showPage (true); };
    detailTab.onClick = [this] { showPage (false); };
    for (auto* c : std::initializer_list<juce::Component*> { &presetBox, &prevPreset, &nextPreset, &easyTab, &detailTab, &easyPage, &detailPage })
        addAndMakeVisible (c);
    setHelp (presetBox, "PRESETS - complete starting points for different styles. Everything can be adjusted after.");
    setHelp (easyTab, "EASY - the main controls, one knob per job, in the order the sound flows.");
    setHelp (detailTab, "DETAIL - fine-tuning controls and a live view of what's being cleaned.");

    // ---- easy page ----
    for (auto* s : { &clean, &tune, &tone, &dyn, &deess, &color, &space, &output }) easyPage.addAndMakeVisible (s);
    attachToggle (clean.power, "cleanOn");  attachToggle (tune.power, "tuneOn");   attachToggle (tone.power, "toneOn");   attachToggle (dyn.power, "dynOn");
    attachToggle (deess.power, "deessOn");  attachToggle (color.power, "colorOn"); attachToggle (space.power, "spaceOn");
    output.power.setVisible (false);
    for (auto* s : { &clean, &tone, &dyn, &deess, &color, &space })
        setHelp (s->power, "Switch this whole section on/off to hear the difference.");
    setHelp (tune.power, "TUNE on/off - automatic pitch correction. Off by default; the rap, pop and R&B presets switch it on.");

    knob (clean, "noiseDb", "Noise", ui::clean);
    knob (clean, "resonance", "De-Harsh", ui::clean);
    learnBtn.setClickingTogglesState (true);
    learnBtn.onClick = [this]
    {
        if (learnBtn.getToggleState())
            if (auto* a = proc.apvts.getParameter ("noiseAuto")) a->setValueNotifyingHost (0.0f);
    };
    clean.addAndMakeVisible (learnBtn);
    attachToggle (learnBtn, "noiseLearn");
    setHelp (learnBtn, "LEARN NOISE - play a part with no singing, click once, wait 2 seconds, click again. The plugin remembers your room's noise with the project.");
    autoNoiseBtn.setColour (juce::ToggleButton::tickColourId, ui::clean);
    clean.addAndMakeVisible (autoNoiseBtn);
    attachToggle (autoNoiseBtn, "noiseAuto");
    setHelp (autoNoiseBtn, "AUTO - tracks the room noise by itself. Use it if you don't have a quiet moment to learn from.");
    noiseStatus.setFont (juce::FontOptions (12.0f));
    noiseStatus.setColour (juce::Label::textColourId, dim);
    clean.addAndMakeVisible (noiseStatus);

    knob (tune, "tuneAmount", "Amount", ui::tune);
    knob (tune, "tuneSpeed", "Speed", ui::tune);
    tuneKey.addItemList (PitchCorrector::keyNames(), 1);
    tuneScale.addItemList (PitchCorrector::scaleNames(), 1);
    tune.addAndMakeVisible (tuneKey);
    tune.addAndMakeVisible (tuneScale);
    tuneKeyAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "tuneKey", tuneKey);
    tuneScaleAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "tuneScale", tuneScale);
    setHelp (tuneKey, "KEY - the key of your beat. Not sure? Leave Scale on Chromatic and the key doesn't matter.");
    setHelp (tuneScale, "SCALE - which notes are allowed. Chromatic works for any song. Major/Minor or a raag-based scale (Bhairav, Kafi, Bhairavi, Yaman) is more musical if you know it.");
    tuneReadout.setFont (juce::FontOptions (12.5f));
    tuneReadout.setColour (juce::Label::textColourId, ui::tune);
    tune.addAndMakeVisible (tuneReadout);
    setHelp (tuneReadout, "Live: the note you're singing (and how many cents off) and the note it's being tuned to.");

    knob (tone, "warmth", "Warmth", ui::tone);
    knob (tone, "clarity", "Clarity", ui::tone);
    knob (tone, "presence", "Presence", ui::tone);
    knob (tone, "air", "Air", ui::tone);

    knob (dyn, "compress", "Compress", ui::dyn);
    autoLevelBtn.setColour (juce::ToggleButton::tickColourId, ui::dyn);
    dyn.addAndMakeVisible (autoLevelBtn);
    attachToggle (autoLevelBtn, "autoLevel");
    setHelp (autoLevelBtn, "AUTO LEVEL - rides the vocal to a steady level first (like riding a fader), so the presets work on any recording.");
    compMeter = std::make_unique<Meter> ("Squash", [this] { return proc.getChain().getCompReductionDb(); }, true, ui::dyn);
    dyn.addAndMakeVisible (*compMeter);
    setHelp (*compMeter, "How much the compressor is turning loud words down right now. 3-8 dB on loud parts is normal.");

    knob (deess, "deess", "De-Ess", ui::deess);
    deessMeter = std::make_unique<Meter> ("'S' cut", [this] { return proc.getChain().getDeessReductionDb(); }, true, ui::deess);
    deess.addAndMakeVisible (*deessMeter);
    setHelp (*deessMeter, "Shows the de-esser working. It should flicker on 's' sounds and stay still on vowels.");

    knob (color, "saturation", "Saturation", ui::color);

    knob (space, "width", "Width", ui::space);
    knob (space, "reverb", "Reverb", ui::space);
    knob (space, "echo", "Echo", ui::space);
    echoTime.addItemList (EchoDelay::divisionNames(), 1);
    space.addAndMakeVisible (echoTime);
    echoTimeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "echoDiv", echoTime);
    setHelp (echoTime, "ECHO TIME - the rhythm of the repeats, locked to your song's tempo. 1/8 dotted is a classic vocal echo.");

    knob (output, "outputDb", "Output", ui::out);
    limiterBtn.setColour (juce::ToggleButton::tickColourId, ui::out);
    output.addAndMakeVisible (limiterBtn);
    attachToggle (limiterBtn, "limiterOn");
    setHelp (limiterBtn, "LIMITER - stops the vocal from ever clipping. Leave on.");
    inMeter  = std::make_unique<Meter> ("In",  [this] { return proc.getChain().getInputLevelDb(); },  false, ui::clean);
    outMeter = std::make_unique<Meter> ("Out", [this] { return proc.getChain().getOutputLevelDb(); }, false, ui::clean);
    limMeter = std::make_unique<Meter> ("Limit", [this] { return proc.getChain().getLimiterReductionDb(); }, true, ui::out);
    for (auto* m : { inMeter.get(), outMeter.get(), limMeter.get() }) output.addAndMakeVisible (*m);
    setHelp (*inMeter, "Input peak level (dBFS).");
    setHelp (*outMeter, "Output peak level (dBFS). Turns red near the ceiling.");
    setHelp (*limMeter, "How hard the limiter is working. Occasional small numbers are fine.");

    // ---- detail page ----
    detailPage.addAndMakeVisible (spectrum);
    setHelp (spectrum, "Live view: grey = your room's noise print, green = noise being removed, red = harsh frequencies being smoothed.");
    for (auto* s : { &dClean, &dDyn, &dDeess, &dSpace, &dOut }) { detailPage.addAndMakeVisible (s); s->power.setVisible (false); }
    lowCutBtn.setColour (juce::ToggleButton::tickColourId, ui::clean);
    dClean.addAndMakeVisible (lowCutBtn);
    attachToggle (lowCutBtn, "lowCutOn");
    setHelp (lowCutBtn, "LOW CUT on/off - removes rumble under the voice.");
    knob (dClean, "lowCutHz", "Low Cut", ui::clean, false);
    knob (dClean, "hiCutHz", "High Cut", ui::clean, false);
    knob (dClean, "noiseThresh", "Noise Thresh", ui::clean, false);
    knob (dClean, "resSharp", "Harsh Focus", ui::clean, false);

    knob (dDyn, "levelTarget", "Level Target", ui::dyn, false);
    knob (dDyn, "compAttack", "Attack", ui::dyn, false);
    knob (dDyn, "compRelease", "Release", ui::dyn, false);

    knob (dDeess, "deessFreq", "'S' Freq", ui::deess, false);
    deessListenBtn.setColour (juce::ToggleButton::tickColourId, ui::deess);
    dDeess.addAndMakeVisible (deessListenBtn);
    attachToggle (deessListenBtn, "deessListen");
    setHelp (deessListenBtn, "LISTEN - hear only what the de-esser hears, to set the 'S' Freq. Turn it off when done!");

    knob (dSpace, "reverbSize", "Size", ui::space, false);
    knob (dSpace, "reverbPre", "Pre-Delay", ui::space, false);
    knob (dSpace, "echoFeedback", "Repeats", ui::space, false);
    knob (dSpace, "duck", "Ducking", ui::space, false);
    pingBtn.setColour (juce::ToggleButton::tickColourId, ui::space);
    dSpace.addAndMakeVisible (pingBtn);
    attachToggle (pingBtn, "echoPing");
    setHelp (pingBtn, "PING-PONG - echoes bounce left and right.");

    knob (dOut, "inputDb", "Input", ui::out, false);
    knob (dOut, "ceiling", "Ceiling", ui::out, false);

    // ---- help bar ----
    helpBar.setFont (juce::FontOptions (13.0f));
    helpBar.setColour (juce::Label::textColourId, dim);
    helpBar.setText (defaultHelp, juce::dontSendNotification);
    addAndMakeVisible (helpBar);

    for (auto& [id, k] : knobById)
    {
        auto it = helpTexts().find (id);
        if (it != helpTexts().end()) setHelp (*k, it->second);
    }
    addMouseListener (this, true);

    refreshPresetBox();
    showPage (true);
    setSize (1170, 680);
    startTimerHz (30);
}

VocalOneEditor::~VocalOneEditor()
{
    removeMouseListener (this);
    setLookAndFeel (nullptr);
}

ui::Knob& VocalOneEditor::knob (juce::Component& parent, const juce::String& id, const juce::String& label, juce::Colour c, bool big)
{
    knobs.push_back (std::make_unique<ui::Knob> (proc.apvts, id, label, c, big));
    auto& k = *knobs.back();
    parent.addAndMakeVisible (k);
    knobById[id] = &k;
    return k;
}

void VocalOneEditor::attachToggle (juce::Button& b, const juce::String& id)
{
    buttonAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id, b));
}

void VocalOneEditor::setHelp (juce::Component& c, const juce::String& t) { help[&c] = t; }

void VocalOneEditor::mouseEnter (const juce::MouseEvent& e)
{
    for (auto* comp = e.eventComponent; comp != nullptr && comp != this; comp = comp->getParentComponent())
    {
        auto it = help.find (comp);
        if (it != help.end()) { helpBar.setText (it->second, juce::dontSendNotification); return; }
    }
}

void VocalOneEditor::mouseExit (const juce::MouseEvent& e)
{
    if (e.eventComponent != nullptr && ! getLocalBounds().contains (e.getEventRelativeTo (this).getPosition()))
        helpBar.setText (defaultHelp, juce::dontSendNotification);
}

void VocalOneEditor::refreshPresetBox()
{
    presetBox.setSelectedId (proc.getCurrentPresetIndex() + 1, juce::dontSendNotification);
}

void VocalOneEditor::showPresetInfo()
{
    const auto& list = presets::all();
    const int i = proc.getCurrentPresetIndex();
    if (juce::isPositiveAndBelow (i, (int) list.size()))
        helpBar.setText (juce::String (list[(size_t) i].name).toUpperCase() + " - " + list[(size_t) i].about, juce::dontSendNotification);
}

void VocalOneEditor::showPage (bool easy)
{
    easyShown = easy;
    easyPage.setVisible (easy);
    detailPage.setVisible (! easy);
    easyTab.setToggleState (easy, juce::dontSendNotification);
    detailTab.setToggleState (! easy, juce::dontSendNotification);
    repaint();
}

void VocalOneEditor::timerCallback()
{
    for (auto* m : { compMeter.get(), deessMeter.get(), inMeter.get(), outMeter.get(), limMeter.get() }) m->tick();
    if (! easyShown) spectrum.repaint();

    auto& nr = proc.getChain().getNoise();
    const bool learning = proc.apvts.getRawParameterValue ("noiseLearn")->load() > 0.5f;
    const bool autoMode = proc.apvts.getRawParameterValue ("noiseAuto")->load() > 0.5f;
    juce::String status;
    juce::Colour c = dim;
    if (learning)                       { status = "Listening to the room... click again when done"; c = juce::Colour (0xfff0a040); }
    else if (autoMode)                  { status = "Auto: tracking room noise"; }
    else if (nr.hasLearnedProfile())    { status = "Room noise learned";  c = ui::clean; }
    else                                { status = "No noise print yet - using Auto"; }
    learnBtn.setButtonText (learning ? "STOP LEARNING" : "LEARN NOISE");
    if (noiseStatus.getText() != status) { noiseStatus.setText (status, juce::dontSendNotification); noiseStatus.setColour (juce::Label::textColourId, c); }
    if (presetBox.getSelectedId() != proc.getCurrentPresetIndex() + 1) refreshPresetBox();

    tuneKey.setEnabled (tuneScale.getSelectedItemIndex() > 0); // key doesn't matter for Chromatic

    // tuner readout: sung note -> corrected note
    auto& tn = proc.getChain().getTuner();
    const float det = tn.getDetectedMidi();
    juce::String rd;
    if (! tune.power.getToggleState()) rd = "Tuning off";
    else if (det < 0.0f)               rd = "Listening...";
    else
    {
        auto noteName = [] (int m) { static const char* nm[] { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" }; return juce::String (nm[m % 12]) + juce::String (m / 12 - 1); };
        const int nearest = (int) std::lround (det);
        const int cents = (int) std::lround ((det - (float) nearest) * 100.0f);
        const int target = (int) std::lround (tn.getTargetMidi());
        rd = "You: " + noteName (nearest) + (cents >= 0 ? " +" : " ") + juce::String (cents) + "c   ->   " + noteName (juce::jmax (0, target));
    }
    if (tuneReadout.getText() != rd) tuneReadout.setText (rd, juce::dontSendNotification);
}

void VocalOneEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);
    // logo
    g.setColour (brand);
    g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    juce::String studio ("D U R R A N I   S T U D I O");
    g.drawText (studio, 19, 6, 260, 14, juce::Justification::left);
    g.setColour (text);
    g.setFont (juce::FontOptions (25.0f, juce::Font::bold));
    g.drawText ("Vocal", 18, 18, 80, 32, juce::Justification::left);
    g.setColour (brand);
    g.drawText ("One", 86, 18, 60, 32, juce::Justification::left);
    g.setColour (dim);
    g.setFont (12.0f);
    g.drawText ("complete vocal chain", 146, 28, 150, 16, juce::Justification::left);

    g.setColour (panel);
    g.fillRect (helpBar.getBounds().expanded (16, 0).withRight (getWidth()).withX (0));
}

// arrows showing the signal flow between sections
void VocalOneEditor::paintOverChildren (juce::Graphics& g)
{
    if (! easyShown) return;
    auto rel = [this] (juce::Component& c) { return c.getBounds().translated (easyPage.getX(), easyPage.getY()).toFloat(); };
    g.setColour (dim.withAlpha (0.8f));
    auto arrowRight = [&] (juce::Rectangle<float> a, juce::Rectangle<float> b)
    {
        const float y = a.getCentreY();
        juce::Path p;
        p.addArrow ({ a.getRight() + 3.0f, y, b.getX() - 3.0f, y }, 2.0f, 9.0f, 7.0f);
        g.fillPath (p);
    };
    arrowRight (rel (clean), rel (tune));
    arrowRight (rel (tune), rel (tone));
    arrowRight (rel (dyn), rel (deess));
    arrowRight (rel (deess), rel (color));
    arrowRight (rel (color), rel (space));
    arrowRight (rel (space), rel (output));
    // wrap from end of row 1 to start of row 2
    auto d = rel (tone), s = rel (dyn);
    const float midY = (d.getBottom() + s.getY()) * 0.5f;
    juce::Path wrap;
    wrap.startNewSubPath (d.getCentreX(), d.getBottom() + 2.0f);
    wrap.lineTo (d.getCentreX(), midY);
    wrap.lineTo (s.getCentreX(), midY);
    g.strokePath (wrap, juce::PathStrokeType (2.0f));
    juce::Path head;
    head.addArrow ({ s.getCentreX(), midY - 1.0f, s.getCentreX(), s.getY() - 2.0f }, 2.0f, 9.0f, 7.0f);
    g.fillPath (head);
}

static void rowOfKnobs (juce::Rectangle<int> area, std::initializer_list<ui::Knob*> list)
{
    const int w = area.getWidth() / (int) list.size();
    for (auto* k : list) k->setBounds (area.removeFromLeft (w).reduced (4, 0));
}

void VocalOneEditor::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (56).reduced (16, 12);
    detailTab.setBounds (header.removeFromRight (84));
    header.removeFromRight (6);
    easyTab.setBounds (header.removeFromRight (84));
    auto mid = header.withSizeKeepingCentre (380, 32).translated (60, 0);
    prevPreset.setBounds (mid.removeFromLeft (32));
    nextPreset.setBounds (mid.removeFromRight (32));
    presetBox.setBounds (mid.reduced (6, 0));

    helpBar.setBounds (r.removeFromBottom (34).reduced (16, 0));
    auto page = r.reduced (16, 6);
    easyPage.setBounds (page);
    detailPage.setBounds (page);
    layoutEasy();
    layoutDetail();
}

void VocalOneEditor::layoutEasy()
{
    auto a = easyPage.getLocalBounds();
    const int gap = 22;
    const int rowH = (a.getHeight() - 30) / 2;
    auto row1 = a.removeFromTop (rowH);
    a.removeFromTop (30);
    auto row2 = a.removeFromTop (rowH);

    clean.setBounds (row1.removeFromLeft (318)); row1.removeFromLeft (gap);
    tune.setBounds (row1.removeFromLeft (330));  row1.removeFromLeft (gap);
    tone.setBounds (row1);

    dyn.setBounds (row2.removeFromLeft (200));   row2.removeFromLeft (gap);
    deess.setBounds (row2.removeFromLeft (150)); row2.removeFromLeft (gap);
    color.setBounds (row2.removeFromLeft (140)); row2.removeFromLeft (gap);
    space.setBounds (row2.removeFromLeft (340)); row2.removeFromLeft (gap);
    output.setBounds (row2);

    const int knobH = 132;
    { auto c = clean.content(); auto top = c.removeFromTop (knobH);
      rowOfKnobs (top, { knobById["noiseDb"], knobById["resonance"] });
      c.removeFromTop (6);
      auto line = c.removeFromTop (32);
      learnBtn.setBounds (line.removeFromLeft (150));
      line.removeFromLeft (12);
      autoNoiseBtn.setBounds (line);
      noiseStatus.setBounds (c.removeFromTop (22)); }
    { auto c = tune.content(); rowOfKnobs (c.removeFromTop (knobH), { knobById["tuneAmount"], knobById["tuneSpeed"] });
      c.removeFromTop (6);
      auto line = c.removeFromTop (28);
      tuneKey.setBounds (line.removeFromLeft (92));
      line.removeFromLeft (8);
      tuneScale.setBounds (line);
      c.removeFromTop (4);
      tuneReadout.setBounds (c.removeFromTop (22)); }
    { auto c = tone.content(); rowOfKnobs (c.removeFromTop (knobH), { knobById["warmth"], knobById["clarity"], knobById["presence"], knobById["air"] }); }
    { auto c = dyn.content(); rowOfKnobs (c.removeFromTop (knobH), { knobById["compress"] });
      c.removeFromTop (6);
      autoLevelBtn.setBounds (c.removeFromTop (26).reduced (4, 0));
      compMeter->setBounds (c.removeFromTop (24).reduced (4, 0)); }
    { auto c = deess.content(); rowOfKnobs (c.removeFromTop (knobH), { knobById["deess"] });
      c.removeFromTop (10);
      deessMeter->setBounds (c.removeFromTop (24)); }
    { auto c = color.content(); rowOfKnobs (c.removeFromTop (knobH), { knobById["saturation"] }); }
    { auto c = space.content(); rowOfKnobs (c.removeFromTop (knobH), { knobById["width"], knobById["reverb"], knobById["echo"] });
      c.removeFromTop (8);
      const int w = c.getWidth() / 3;
      echoTime.setBounds (c.removeFromTop (28).withX (c.getX() + 2 * w + 4).withWidth (w - 8)); }
    { auto c = output.content();
      auto top = c.removeFromTop (knobH);
      rowOfKnobs (top.removeFromLeft (top.getWidth() / 2), { knobById["outputDb"] });
      limiterBtn.setBounds (top.withSizeKeepingCentre (top.getWidth(), 26).reduced (6, 0));
      c.removeFromTop (4);
      for (auto* m : { inMeter.get(), outMeter.get(), limMeter.get() }) { m->setBounds (c.removeFromTop (24)); c.removeFromTop (2); } }
}

void VocalOneEditor::layoutDetail()
{
    auto a = detailPage.getLocalBounds();
    spectrum.setBounds (a.removeFromTop (170));
    a.removeFromTop (10);
    const int rowH = (a.getHeight() - 10) / 2;
    auto row1 = a.removeFromTop (rowH);
    a.removeFromTop (10);
    auto row2 = a;

    dClean.setBounds (row1.removeFromLeft (470)); row1.removeFromLeft (10);
    dDyn.setBounds (row1.removeFromLeft (330));   row1.removeFromLeft (10);
    dDeess.setBounds (row1);
    dSpace.setBounds (row2.removeFromLeft (720)); row2.removeFromLeft (10);
    dOut.setBounds (row2);

    const int kh = 110;
    { auto c = dClean.content(); lowCutBtn.setBounds (c.removeFromTop (22).removeFromLeft (120));
      rowOfKnobs (c.removeFromTop (kh), { knobById["lowCutHz"], knobById["hiCutHz"], knobById["noiseThresh"], knobById["resSharp"] }); }
    { auto c = dDyn.content(); c.removeFromTop (22); rowOfKnobs (c.removeFromTop (kh), { knobById["levelTarget"], knobById["compAttack"], knobById["compRelease"] }); }
    { auto c = dDeess.content(); deessListenBtn.setBounds (c.removeFromTop (22)); rowOfKnobs (c.removeFromTop (kh), { knobById["deessFreq"] }); }
    { auto c = dSpace.content(); pingBtn.setBounds (c.removeFromTop (22).removeFromLeft (130));
      rowOfKnobs (c.removeFromTop (kh), { knobById["reverbSize"], knobById["reverbPre"], knobById["echoFeedback"], knobById["duck"] }); }
    { auto c = dOut.content(); c.removeFromTop (22); rowOfKnobs (c.removeFromTop (kh), { knobById["inputDb"], knobById["ceiling"] }); }
}
