#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"

class VocalOneProcessor : public juce::AudioProcessor
{
public:
    VocalOneProcessor();
    ~VocalOneProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "VocalOne"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; } // reverb / echo tails

    int getNumPrograms() override { return (int) presets::all().size(); }
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override { applyPreset (index); }
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void applyPreset (int index);
    int  getCurrentPresetIndex() const { return currentPreset; }

    VocalChain& getChain() { return chain; }
    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    VocalChain chain;
    std::map<std::string, std::atomic<float>*> raw;
    int currentPreset = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalOneProcessor)
};
