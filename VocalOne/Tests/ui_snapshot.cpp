// Renders the plugin window to PNG files (used to check the layout headlessly).
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

static void feed (VocalOneProcessor& p, double secs)
{
    juce::AudioBuffer<float> b (2, 512);
    juce::MidiBuffer midi;
    juce::Random rng (1);
    static long t = 0;
    for (int blk = 0; blk < (int) (secs * 48000 / 512); ++blk)
    {
        for (int i = 0; i < 512; ++i, ++t)
        {
            const bool voice = (t / 38400) % 2 == 0;
            float v = 0.004f * (rng.nextFloat() * 2 - 1);
            if (voice) for (int h = 1; h < 10; ++h) v += 0.25f / h * (float) std::sin (juce::MathConstants<double>::twoPi * 196.0 * h * t / 48000.0);
            if (voice && (t % 38400) < 3000) v += 0.3f * (rng.nextFloat() * 2 - 1);
            b.setSample (0, i, v); b.setSample (1, i, v);
        }
        p.processBlock (b, midi);
    }
}

static void shoot (juce::Component& c, const char* path)
{
    auto img = c.createComponentSnapshot (c.getLocalBounds(), true, 1.0f);
    juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (path));
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    {
        VocalOneProcessor p;
        p.setPlayConfigDetails (2, 2, 48000.0, 512);
        p.prepareToPlay (48000.0, 512);
        const auto& list = presets::all();
        for (int i = 0; i < (int) list.size(); ++i) if (juce::String (list[(size_t) i].name).startsWith ("Desi R&B")) p.applyPreset (i);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());

        ed->setVisible (true);
        for (int i = 0; i < 20; ++i) { feed (p, 0.1); juce::MessageManager::getInstance()->runDispatchLoopUntil (35); }
        shoot (*ed, "ui_easy.png");
        if (auto* t = dynamic_cast<juce::Button*> (ed->findChildWithID ("detailTab"))) t->onClick();
        for (int i = 0; i < 10; ++i) { feed (p, 0.1); juce::MessageManager::getInstance()->runDispatchLoopUntil (35); }
        shoot (*ed, "ui_detail.png");
        if (auto* t = dynamic_cast<juce::Button*> (ed->findChildWithID ("easyTab"))) t->onClick();
        if (auto* prm = p.apvts.getParameter ("colorOn")) prm->setValueNotifyingHost (0.0f);
        if (auto* prm = p.apvts.getParameter ("noiseLearn")) prm->setValueNotifyingHost (1.0f);
        for (int i = 0; i < 10; ++i) { feed (p, 0.1); juce::MessageManager::getInstance()->runDispatchLoopUntil (35); }
        shoot (*ed, "ui_states.png");
        ed = nullptr;
    }
    return 0;
}
