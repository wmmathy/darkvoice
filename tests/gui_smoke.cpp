#include "PluginProcessor.hpp"
#include <cmath>
#include <iostream>
#include <memory>

int main(int argc, char **argv) {
    juce::ScopedJuceInitialiser_GUI initialise;
    DarkVoiceProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    if (!editor || editor->getWidth() < 640 || editor->getHeight() < 610)
        return 1;
    int controls = 0;
    juce::Slider *input = nullptr;
    juce::ToggleButton *bypass = nullptr;
    for (auto *child : editor->getChildren()) {
        if (auto *slider = dynamic_cast<juce::Slider *>(child)) {
            ++controls;
            if (slider->getName() == "INPUT GAIN")
                input = slider;
        }
        if (auto *button = dynamic_cast<juce::ToggleButton *>(child))
            bypass = button;
    }
    if (controls != 7 || !input || !bypass)
        return 2;
    input->setValue(6.0, juce::sendNotificationSync);
    if (std::abs(processor.parameters.getRawParameterValue("inputDb")->load() - 6.0f) > 0.01f)
        return 3;
    bypass->setToggleState(true, juce::sendNotificationSync);
    if (processor.parameters.getRawParameterValue("bypass")->load() != 1.0f)
        return 4;
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
    std::cout << "GUI smoke passed: seven controls, parameter attachments, bypass, snapshot\n";
}
