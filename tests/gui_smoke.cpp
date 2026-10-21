#include "PluginEditor.hpp"
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <set>
#include <vector>
namespace {
void collect(juce::Component &root, std::vector<juce::Component *> &result,
             std::set<juce::Component *> &visited) {
    if (!visited.insert(&root).second)
        return;
    result.push_back(&root);
    for (auto *child : root.getChildren())
        collect(*child, result, visited);
    if (auto *tabs = dynamic_cast<juce::TabbedComponent *>(&root))
        for (int i = 0; i < tabs->getNumTabs(); ++i)
            if (auto *content = tabs->getTabContentComponent(i))
                collect(*content, result, visited);
}
} // namespace
int main(int argc, char **argv) {
    juce::ScopedJuceInitialiser_GUI initialise;
    const auto root = juce::File(std::filesystem::canonical(
                                     juce::File::getSpecialLocation(juce::File::tempDirectory)
                                         .getFullPathName()
                                         .toStdString())
                                     .string())
                          .getChildFile("darkvoice-gui-" + juce::Uuid().toString());
    int result = [&]() {
        DarkVoiceProcessor processor;
        std::unique_ptr<juce::AudioProcessorEditor> editor(new DarkVoiceEditor(processor, root));
        if (!editor || editor->getWidth() < 760 || editor->getHeight() < 720)
            return 1;
        std::vector<juce::Component *> components;
        std::set<juce::Component *> visited;
        collect(*editor, components, visited);
        int controls = 0;
        juce::Slider *input = nullptr;
        juce::ToggleButton *bypass = nullptr;
        juce::ComboBox *presetSelection = nullptr;
        juce::TextButton *load = nullptr;
        for (auto *child : components) {
            if (auto *slider = dynamic_cast<juce::Slider *>(child)) {
                ++controls;
                if (slider->getName() == "INPUT GAIN")
                    input = slider;
            }
            if (auto *button = dynamic_cast<juce::ToggleButton *>(child);
                button && button->getName() == "Bypass processing")
                bypass = button;
            if (auto *combo = dynamic_cast<juce::ComboBox *>(child);
                combo && combo->getName() == "Preset selection")
                presetSelection = combo;
            if (auto *button = dynamic_cast<juce::TextButton *>(child);
                button && button->getName() == "Load preset")
                load = button;
        }
        if (controls != 20 || !input || !bypass || !presetSelection || !load)
            return 2;
        input->setValue(6.0, juce::sendNotificationSync);
        if (std::abs(processor.parameters.getRawParameterValue("inputDb")->load() - 6.0f) > 0.01f)
            return 3;
        bypass->setToggleState(true, juce::sendNotificationSync);
        if (processor.parameters.getRawParameterValue("bypass")->load() != 1.0f)
            return 4;
        constexpr std::array<const char *, 20> names{"INPUT GAIN",
                                                     "OUTPUT GAIN",
                                                     "HIGH-PASS",
                                                     "WARMTH DRIVE",
                                                     "COMPRESSOR THRESHOLD",
                                                     "COMPRESSOR RATIO",
                                                     "DRY / WET",
                                                     "LOW SHELF FREQUENCY",
                                                     "LOW SHELF GAIN",
                                                     "MID FREQUENCY",
                                                     "MID GAIN",
                                                     "MID Q",
                                                     "HIGH SHELF FREQUENCY",
                                                     "HIGH SHELF GAIN",
                                                     "DELAY TIME",
                                                     "DELAY FEEDBACK",
                                                     "DELAY MIX",
                                                     "REVERB SIZE",
                                                     "REVERB DAMPING",
                                                     "REVERB MIX"};
        std::size_t soundIndex = 0;
        for (const auto &spec : darkvoice::parameterSchema) {
            if (spec.boolean)
                continue;
            juce::Slider *found = nullptr;
            for (auto *component : components)
                if (auto *slider = dynamic_cast<juce::Slider *>(component);
                    slider && slider->getName() == names[soundIndex])
                    found = slider;
            if (!found)
                return 7;
            const auto value = spec.minimum + 0.6f * (spec.maximum - spec.minimum);
            found->setValue(value, juce::sendNotificationSync);
            if (std::abs(processor.parameters.getRawParameterValue(spec.id)->load() -
                         static_cast<float>(found->getValue())) > 0.001f)
                return 8;
            ++soundIndex;
        }
        // Loading the actual UI factory selection drives APVTS and real audio.
        int industrial = 0;
        for (int i = 0; i < presetSelection->getNumItems(); ++i)
            if (presetSelection->getItemText(i).contains("Industrial Bass"))
                industrial = presetSelection->getItemId(i);
        if (industrial == 0)
            return 9;
        presetSelection->setSelectedId(industrial, juce::sendNotificationSync);
        load->onClick();
        darkvoice::PresetBank bank(root);
        const auto list = bank.search("Industrial Bass");
        if (list.size() != 1)
            return 10;
        for (const auto &[id, expected] : list[0].values) {
            float tolerance = 0.001f;
            for (const auto &spec : darkvoice::parameterSchema)
                if (id == spec.id)
                    tolerance = std::max(tolerance, spec.step * 0.51f);
            if (std::abs(processor.parameters.getRawParameterValue(id)->load() - expected) >
                tolerance)
                return 11;
        }
        if (processor.parameters.getRawParameterValue("bypass")->load() != 1.0f)
            return 12;
        bypass->setToggleState(false, juce::sendNotificationSync);
        DarkVoiceProcessor neutral;
        processor.prepareToPlay(48000, 512);
        neutral.prepareToPlay(48000, 512);
        juce::AudioBuffer<float> actual(2, 512), reference(2, 512);
        juce::MidiBuffer midi;
        double difference = 0;
        for (int block = 0; block < 10; ++block) {
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 512; ++i) {
                    const auto value =
                        0.3f * std::sin(static_cast<float>((block * 512 + i) * 2.0 *
                                                           juce::MathConstants<double>::pi *
                                                           1000.0 / 48000));
                    actual.setSample(c, i, value);
                    reference.setSample(c, i, value);
                }
            processor.processBlock(actual, midi);
            neutral.processBlock(reference, midi);
            for (int i = 0; i < 512; ++i)
                difference += std::abs(actual.getSample(0, i) - reference.getSample(0, i));
        }
        if (difference < 0.01)
            return 13;
        auto image = editor->createComponentSnapshot(editor->getLocalBounds());
        if (image.isNull())
            return 5;
        if (argc > 1) {
            juce::File output = juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
            output.deleteFile();
            auto stream = output.createOutputStream();
            if (!stream || !juce::PNGImageFormat().writeImageToStream(image, *stream))
                return 6;
        }
        std::cout << "GUI smoke passed: 20 controls, all parameter attachments, bypass, actual "
                     "factory preset audio, snapshot\n";
        return 0;
    }();
    root.deleteRecursively();
    return result;
}
