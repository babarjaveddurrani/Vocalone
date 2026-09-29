#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace ui
{
    const juce::Colour bg        { 0xff15171b };
    const juce::Colour panel     { 0xff1e2127 };
    const juce::Colour panelEdge { 0xff2b2f37 };
    const juce::Colour text      { 0xffe8eaee };
    const juce::Colour dim       { 0xff8a909c };
    const juce::Colour clean     { 0xff4fc3a1 };
    const juce::Colour tune      { 0xff6fd3e8 };
    const juce::Colour brand     { 0xffe07b2a };
    const juce::Colour tone      { 0xfff2b84b };
    const juce::Colour dyn       { 0xffef6f5e };
    const juce::Colour deess     { 0xffc98bf0 };
    const juce::Colour color     { 0xfff08a4b };
    const juce::Colour space     { 0xff5aa9f0 };
    const juce::Colour out       { 0xffd0d4dc };

    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel();
        void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
        void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
        juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (15.0f, juce::Font::bold)); }
    };

    // Rotary knob with label on top and value underneath.
    class Knob : public juce::Component
    {
    public:
        Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramID, const juce::String& label, juce::Colour, bool big);
        void resized() override;
        void paint (juce::Graphics&) override;
        juce::Slider slider;
    private:
        juce::String labelText;
        bool big;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    // Rounded module panel with a title and a power switch.
    class Section : public juce::Component
    {
    public:
        Section (const juce::String& title, const juce::String& step, juce::Colour);
        void paint (juce::Graphics&) override;
        void resized() override;
        juce::Rectangle<int> content() const { return getLocalBounds().reduced (10).withTrimmedTop (28); }
        juce::ToggleButton power;
        juce::Colour colour;
    private:
        juce::String title, step;
    };

    // Horizontal gain-reduction / level bar.
    class Meter : public juce::Component
    {
    public:
        Meter (juce::String labelText, std::function<float()> reader, bool isReduction, juce::Colour c)
            : label (std::move (labelText)), read (std::move (reader)), reduction (isReduction), colour (c) {}
        void paint (juce::Graphics&) override;
        void tick();
    private:
        juce::String label;
        std::function<float()> read;
        bool reduction;
        juce::Colour colour;
        float shown = 0.0f;
    };

    // Spectrum: noise print + what the cleanup stages are taking out.
    class Spectrum : public juce::Component
    {
    public:
        explicit Spectrum (VocalOneProcessor& p) : proc (p) {}
        void paint (juce::Graphics&) override;
    private:
        VocalOneProcessor& proc;
    };
}

class VocalOneEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VocalOneEditor (VocalOneProcessor&);
    ~VocalOneEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    ui::Knob& knob (juce::Component& parent, const juce::String& id, const juce::String& label, juce::Colour, bool big = true);
    void attachToggle (juce::Button&, const juce::String& id);
    void setHelp (juce::Component&, const juce::String& text);
    void showPage (bool easy);
    void layoutEasy();
    void layoutDetail();
    void refreshPresetBox();
    void showPresetInfo();

    VocalOneProcessor& proc;
    ui::LookAndFeel lnf;

    // header
    juce::ComboBox presetBox;
    juce::TextButton prevPreset { "<" }, nextPreset { ">" }, easyTab { "EASY" }, detailTab { "DETAIL" };

    juce::Component easyPage, detailPage;

    // easy page sections, in signal order
    ui::Section clean { "CLEAN", "1", ui::clean }, tune { "TUNE", "2", ui::tune }, tone { "TONE", "3", ui::tone },
                dyn { "DYNAMICS", "4", ui::dyn }, deess { "DE-ESS", "5", ui::deess }, color { "COLOR", "6", ui::color },
                space { "SPACE", "7", ui::space }, output { "OUTPUT", "8", ui::out };
    juce::ComboBox tuneKey, tuneScale;
    juce::Label tuneReadout;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> tuneKeyAtt, tuneScaleAtt;
    juce::TextButton learnBtn { "LEARN NOISE" };
    juce::ToggleButton autoNoiseBtn { "Auto" }, autoLevelBtn { "Auto Level" }, limiterBtn { "Limiter" };
    juce::ComboBox echoTime;
    juce::Label noiseStatus;
    std::unique_ptr<ui::Meter> compMeter, deessMeter, inMeter, outMeter, limMeter;

    // detail page
    ui::Section dClean { "CLEAN - FINE TUNE", "", ui::clean }, dDyn { "DYNAMICS - FINE TUNE", "", ui::dyn },
                dDeess { "DE-ESS - FINE TUNE", "", ui::deess }, dSpace { "SPACE - FINE TUNE", "", ui::space },
                dOut { "LEVELS", "", ui::out };
    juce::ToggleButton lowCutBtn { "Low Cut" }, deessListenBtn { "Listen to 's' only" }, pingBtn { "Ping-Pong" };
    ui::Spectrum spectrum;

    juce::Label helpBar;
    juce::String defaultHelp, helpTextForReset;

    std::vector<std::unique_ptr<ui::Knob>> knobs;
    std::map<juce::String, ui::Knob*> knobById;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> echoTimeAtt;
    std::map<juce::Component*, juce::String> help;
    bool easyShown = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalOneEditor)
};
