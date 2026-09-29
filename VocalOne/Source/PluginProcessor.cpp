#include "PluginProcessor.h"
#include "PluginEditor.h"

VocalOneProcessor::VocalOneProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VOCALONE", createLayout())
{
    for (auto& d : params::all())
        raw[d.id] = apvts.getRawParameterValue (d.id);
}

juce::AudioProcessorValueTreeState::ParameterLayout VocalOneProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> list;
    for (auto& d : params::all())
    {
        const juce::ParameterID pid { d.id, 1 };
        switch (d.type)
        {
            case params::Type::Bool:
                list.push_back (std::make_unique<juce::AudioParameterBool> (pid, d.name, d.def > 0.5f));
                break;
            case params::Type::Choice:
                list.push_back (std::make_unique<juce::AudioParameterChoice> (pid, d.name, params::choicesOf (d), (int) d.def));
                break;
            case params::Type::Float:
            {
                juce::NormalisableRange<float> range (d.min, d.max, 0.0f, d.skew);
                const juce::String unit (d.unit), id (d.id);
                list.push_back (std::make_unique<juce::AudioParameterFloat> (pid, d.name, range, d.def,
                    juce::AudioParameterFloatAttributes().withLabel (unit)
                        .withStringFromValueFunction ([unit, id] (float v, int)
                        {
                            if (id == "hiCutHz" && v >= 19500.0f) return juce::String ("Off");
                            if (id == "tuneSpeed" && v < 0.5f) return juce::String ("0 ms robot");
                            if (unit.isEmpty()) return juce::String (juce::roundToInt (v * 100.0f)) + "%";
                            if (unit == "Hz" && v >= 1000.0f) return juce::String (v / 1000.0f, 1) + " kHz";
                            return juce::String (v, std::abs (v) < 10.0f ? 1 : 0) + " " + unit;
                        })));
                break;
            }
        }
    }
    return { list.begin(), list.end() };
}

void VocalOneProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    chain.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    setLatencySamples (chain.getLatencySamples());
}

bool VocalOneProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return in == out || (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo());
}

void VocalOneProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // mono vocal on a stereo output: copy it to both sides so width/reverb can spread it
    if (getTotalNumInputChannels() == 1 && buffer.getNumChannels() > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, buffer.getNumSamples());

    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) bpm = *b;

    chain.setSettings (params::makeSettings ([this] (const char* id) { return raw.at (id)->load(); }, bpm));
    chain.process (buffer);
}

const juce::String VocalOneProcessor::getProgramName (int index)
{
    const auto& list = presets::all();
    return juce::isPositiveAndBelow (index, (int) list.size()) ? juce::String (list[(size_t) index].name) : juce::String();
}

void VocalOneProcessor::applyPreset (int index)
{
    const auto& list = presets::all();
    if (! juce::isPositiveAndBelow (index, (int) list.size())) return;
    currentPreset = index;
    for (auto& [id, value] : presets::valuesFor (list[(size_t) index]))
    {
        if (! presets::presetControlsParam (id)) continue; // noise capture and the song's key/scale are yours
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
    }
}

juce::AudioProcessorEditor* VocalOneProcessor::createEditor()
{
    return new VocalOneEditor (*this);
}

void VocalOneProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset, nullptr);
    auto& noise = chain.getNoise();
    if (noise.hasLearnedProfile())
    {
        const auto prof = noise.getLearnedProfile();
        juce::MemoryBlock mb (prof.data(), prof.size() * sizeof (float));
        state.setProperty ("noiseProfile", mb.toBase64Encoding(), nullptr);
    }
    else state.removeProperty ("noiseProfile", nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void VocalOneProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto tree = juce::ValueTree::fromXml (*xml);

    juce::MemoryBlock mb;
    const auto encoded = tree.getProperty ("noiseProfile").toString();
    if (encoded.isNotEmpty() && mb.fromBase64Encoding (encoded)
        && mb.getSize() == sizeof (float) * (size_t) NoiseReducer::numBins)
    {
        std::vector<float> prof ((size_t) NoiseReducer::numBins);
        std::memcpy (prof.data(), mb.getData(), mb.getSize());
        chain.getNoise().setLearnedProfile (prof);
    }
    currentPreset = (int) tree.getProperty ("preset", 0);
    apvts.replaceState (tree);
    if (auto* learn = apvts.getParameter ("noiseLearn")) learn->setValueNotifyingHost (0.0f); // never reopen mid-capture
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VocalOneProcessor();
}
