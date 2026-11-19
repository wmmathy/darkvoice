#include "PluginEditor.hpp"
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <set>
#include <thread>
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
#if JUCE_LINUX || JUCE_BSD
        const auto configured = juce::SystemStats::getEnvironmentVariable("XDG_CONFIG_HOME", {});
        if (juce::File::isAbsolutePath(configured)) {
            std::unique_ptr<juce::AudioProcessorEditor> standardEditor(processor.createEditor());
            if (!juce::File(configured).getChildFile("DarkVoiceStudio/Presets").isDirectory())
                return 14;
        }
#endif
        std::unique_ptr<juce::AudioProcessorEditor> editor(new DarkVoiceEditor(processor, root));
        if (!editor || editor->getWidth() < 900 || editor->getHeight() < 780)
            return 1;
        std::vector<juce::Component *> components;
        std::set<juce::Component *> visited;
        collect(*editor, components, visited);
        int controls = 0;
        juce::Slider *input = nullptr;
        juce::ToggleButton *bypass = nullptr;
        juce::ComboBox *presetSelection = nullptr;
        juce::ComboBox *quality = nullptr;
        juce::Label *qualityStatus = nullptr;
        juce::TextButton *load = nullptr;
        for (auto *child : components) {
            if (auto *combo = dynamic_cast<juce::ComboBox *>(child);
                combo && combo->getName() == "Processing quality")
                quality = combo;
            if (auto *label = dynamic_cast<juce::Label *>(child);
                label && label->getName() == "Processing quality status")
                qualityStatus = label;
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
        if (controls != 67 || !input || !bypass || !presetSelection || !load)
            return 2;
        if (!quality || !qualityStatus || quality->getNumItems() != 3 ||
            !editor->getLocalBounds().contains(quality->getBounds()) ||
            !editor->getLocalBounds().contains(qualityStatus->getBounds()))
            return 36;
        processor.prepareToPlay(48000, 64);
        quality->setSelectedId(3, juce::sendNotificationSync);
        if (processor.requestedQualityMode() != darkvoice::QualityMode::Offline ||
            processor.activeQualityMode() != darkvoice::QualityMode::Live ||
            processor.getLatencySamples() !=
                static_cast<int>(darkvoice::BandlimitedColour::latencyForQuality(
                    darkvoice::QualityMode::Live)) ||
            !qualityStatus->getText().contains("pending"))
            return 37;
        quality->setSelectedId(1, juce::sendNotificationSync);
        input->setValue(6.0, juce::sendNotificationSync);
        if (std::abs(processor.parameters.getRawParameterValue("inputDb")->load() - 6.0f) > 0.01f)
            return 3;
        bypass->setToggleState(true, juce::sendNotificationSync);
        if (processor.parameters.getRawParameterValue("bypass")->load() != 1.0f)
            return 4;
        constexpr std::array<const char *, 67> names{"INPUT GAIN",
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
                                                     "REVERB MIX",
                                                     "GATE THRESHOLD",
                                                     "GATE RATIO (1 = OFF)",
                                                     "GATE RELEASE",
                                                     "DE-ESSER FREQUENCY",
                                                     "DE-ESSER THRESHOLD",
                                                     "DE-ESSER MAX REDUCTION",
                                                     "CHORUS RATE",
                                                     "CHORUS DEPTH",
                                                     "CHORUS MIX",
                                                     "STEREO WIDTH",
                                                     "FLANGER RATE",
                                                     "FLANGER DEPTH",
                                                     "FLANGER FEEDBACK",
                                                     "FLANGER MIX",
                                                     "LIMITER ENABLE (0 = OFF, 1 = ON)",
                                                     "LIMITER CEILING",
                                                     "LIMITER RELEASE",
                                                     "STEREO DELAY MODE",
                                                     "STEREO DELAY TIME",
                                                     "STEREO DELAY FEEDBACK",
                                                     "STEREO DELAY MIX",
                                                     "STEREO DELAY SYNC (0 / 1)",
                                                     "MANUAL TEMPO",
                                                     "BEAT DIVISION",
                                                     "USE HOST TEMPO (0 / 1)",
                                                     "DOUBLER SEPARATION",
                                                     "DOUBLER VARIATION",
                                                     "DOUBLER RATE",
                                                     "DOUBLER MIX",
                                                     "SPACE MODEL",
                                                     "SPACE DECAY",
                                                     "SPACE HF TONE",
                                                     "SPACE PREDELAY",
                                                     "SPACE MIX",
                                                     "COLOUR STYLE",
                                                     "COLOUR DRIVE",
                                                     "COLOUR HF TONE",
                                                     "COLOUR PARALLEL MIX",
                                                     "BODY 180 Hz",
                                                     "WARMTH TILT",
                                                     "PRESENCE 3 kHz",
                                                     "BREATH TEXTURE",
                                                     "DEPTH LOWS",
                                                     "RESONANCE 700 Hz",
                                                     "CHARACTER",
                                                     "TRANSDUCER MODEL",
                                                     "TRANSDUCER MIX"};
        std::size_t soundIndex = 0;
        for (const auto &spec : darkvoice::parameterSchema) {
            if (juce::String(spec.id) == "bypass")
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
        // Feed a sustained dry A2, then dispatch the real UI timer and inspect feedback.
        for (int block = 0; block < 32; ++block) {
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 512; ++i)
                    actual.setSample(c, i,
                                     0.2f * static_cast<float>(std::sin(
                                                (block * 512 + i) * 2.0 *
                                                juce::MathConstants<double>::pi * 110.0 / 48000)));
            processor.processBlock(actual, midi);
        }
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (std::abs(processor.dryPitchHz() - 110.0f) > 1.0f &&
               std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (std::abs(processor.dryPitchHz() - 110.0f) > 1.0f)
            return 15;
        bool pitchShown = false;
        const auto uiDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (!pitchShown && std::chrono::steady_clock::now() < uiDeadline) {
            // This smoke harness has no native message loop. Pump actual pending
            // timers until the editor has consumed the asynchronous worker result.
            juce::Timer::callPendingTimersSynchronously();
            for (auto *component : components)
                if (auto *label = dynamic_cast<juce::Label *>(component);
                    label && label->getName() == "Dry pitch analysis")
                    pitchShown = label->getText().contains("Dry input: A2");
            if (!pitchShown)
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (!pitchShown) {
            std::cerr << "Dry pitch result did not reach editor timer\n";
            return 16;
        }
        bool gateTab = false;
        for (auto *component : components)
            if (auto *tabs = dynamic_cast<juce::TabbedComponent *>(component)) {
                if (tabs->getOrientation() != juce::TabbedButtonBar::TabsAtLeft ||
                    tabs->getTabBarDepth() < 160)
                    return 22;
                if (tabs->getNumTabs() != 14)
                    return 17;
                for (int i = 0; i < tabs->getNumTabs(); ++i) {
                    const auto *button = tabs->getTabbedButtonBar().getTabButton(i);
                    if (!button || !button->isVisible() || button->getHeight() < 30 ||
                        button->getBottom() > tabs->getTabbedButtonBar().getHeight())
                        return 24;
                }
                tabs->setCurrentTabIndex(12);
                gateTab = tabs->getCurrentTabName() == "VOICE SCULPTOR";
            }
        if (!gateTab)
            return 18;
        bool tempoShown = false;
        juce::Timer::callPendingTimersSynchronously();
        for (auto *component : components)
            if (auto *label = dynamic_cast<juce::Label *>(component);
                label && label->getName() == "Stereo delay tempo status")
                tempoShown = label->getText().contains("manual fallback");
        if (!tempoShown)
            return 19;
        juce::Slider *mode = nullptr;
        juce::Slider *division = nullptr;
        for (auto *component : components)
            if (auto *slider = dynamic_cast<juce::Slider *>(component)) {
                if (slider->getName() == "STEREO DELAY MODE")
                    mode = slider;
                if (slider->getName() == "BEAT DIVISION")
                    division = slider;
            }
        if (!mode || !division || !mode->textFromValueFunction(2).contains("Ping-pong") ||
            division->textFromValueFunction(5) != "1/4")
            return 20;
        juce::Slider *spaceModel = nullptr;
        for (auto *component : components)
            if (auto *slider = dynamic_cast<juce::Slider *>(component);
                slider && slider->getName() == "SPACE MODEL")
                spaceModel = slider;
        if (!spaceModel || spaceModel->textFromValueFunction(0) != "Room" ||
            spaceModel->textFromValueFunction(1) != "Hall" ||
            spaceModel->textFromValueFunction(2) != "Plate")
            return 23;
        juce::Slider *colourModel = nullptr;
        for (auto *component : components)
            if (auto *slider = dynamic_cast<juce::Slider *>(component);
                slider && slider->getName() == "COLOUR STYLE")
                colourModel = slider;
        if (!colourModel || colourModel->textFromValueFunction(0) != "Tape" ||
            colourModel->textFromValueFunction(1) != "Tube" ||
            processor.getLatencySamples() !=
                static_cast<int>(
                    darkvoice::BandlimitedColour::latencyForQuality(darkvoice::QualityMode::Live)))
            return 25;
        juce::Slider *warmth = nullptr;
        for (auto *component : components)
            if (auto *slider = dynamic_cast<juce::Slider *>(component);
                slider && slider->getName() == "WARMTH TILT")
                warmth = slider;
        if (!warmth || !warmth->getTooltip().contains("+6 dB span") ||
            !warmth->getTooltip().contains("+3 dB") || !warmth->getTooltip().contains("-3 dB"))
            return 26;
        juce::Slider *breath = nullptr;
        for (auto *component : components)
            if (auto *slider = dynamic_cast<juce::Slider *>(component);
                slider && slider->getName() == "BREATH TEXTURE")
                breath = slider;
        if (!breath || !breath->getTooltip().contains("Center-driven") ||
            !breath->getTooltip().contains("recent vocal level"))
            return 27;
        juce::Slider *character = nullptr;
        for (auto *component : components)
            if (auto *slider = dynamic_cast<juce::Slider *>(component);
                slider && slider->getName() == "CHARACTER")
                character = slider;
        if (!character || !character->getTooltip().contains("independently of Colour mix"))
            return 38;
        character->setValue(-.625, juce::sendNotificationSync);
        if (std::abs(processor.parameters.getRawParameterValue("sculptCharacter")->load() + .625f) >
            .001f)
            return 39;
        character->setValue(0, juce::sendNotificationSync);
        if (processor.parameters.getRawParameterValue("sculptCharacter")->load() != 0)
            return 40;
        juce::Slider *transducerModel = nullptr, *transducerMix = nullptr;
        for (auto *component : components) {
            if (auto *slider = dynamic_cast<juce::Slider *>(component)) {
                if (slider->getName() == "TRANSDUCER MODEL")
                    transducerModel = slider;
                if (slider->getName() == "TRANSDUCER MIX")
                    transducerMix = slider;
            }
            if (auto *tabs = dynamic_cast<juce::TabbedComponent *>(component)) {
                tabs->setCurrentTabIndex(13);
                if (tabs->getCurrentTabName() != "TRANSDUCER")
                    return 51;
            }
        }
        if (!transducerModel || !transducerMix)
            return 52;
        for (int model = 0; model < 3; ++model) {
            transducerModel->setValue(model, juce::sendNotificationSync);
            if (processor.parameters.getRawParameterValue("transducerModel")->load() != model ||
                transducerModel->getTextFromValue(model) != juce::String(model == 0   ? "Telephone"
                                                                         : model == 1 ? "Megaphone"
                                                                                      : "Radio"))
                return 53;
        }
        transducerMix->setValue(.731, juce::sendNotificationSync);
        if (std::abs(processor.parameters.getRawParameterValue("transducerMix")->load() - .731f) >
            .001f)
            return 54;
        transducerModel->setValue(0, juce::sendNotificationSync);
        transducerMix->setValue(0, juce::sendNotificationSync);
        juce::TextButton *previewButton = nullptr;
        for (auto *component : components)
            if (auto *button = dynamic_cast<juce::TextButton *>(component);
                button && button->getButtonText() == "Preview effects...")
                previewButton = button;
        if (!previewButton || previewButton->getBounds() != juce::Rectangle<int>(744, 141, 132, 30))
            return 41;
        previewButton->onClick();
        juce::DialogWindow *previewWindow = nullptr;
        for (int i = 0; i < juce::TopLevelWindow::getNumTopLevelWindows(); ++i)
            if (auto *window =
                    dynamic_cast<juce::DialogWindow *>(juce::TopLevelWindow::getTopLevelWindow(i));
                window && window->getName() == "Effect examples")
                previewWindow = window;
        if (!previewWindow || !previewWindow->isVisible())
            return 42;
        std::vector<juce::Component *> previewComponents;
        std::set<juce::Component *> previewVisited;
        collect(*previewWindow, previewComponents, previewVisited);
        juce::ComboBox *voice = nullptr, *effect = nullptr;
        juce::TextButton *play = nullptr, *stop = nullptr;
        juce::ToggleButton *processed = nullptr;
        for (auto *component : previewComponents) {
            if (auto *combo = dynamic_cast<juce::ComboBox *>(component);
                combo && combo->getName() == "Preview voice")
                voice = combo;
            if (auto *combo = dynamic_cast<juce::ComboBox *>(component);
                combo && combo->getName() == "Preview effect")
                effect = combo;
            if (auto *button = dynamic_cast<juce::TextButton *>(component)) {
                if (button->getButtonText() == "Play")
                    play = button;
                if (button->getButtonText() == "Stop")
                    stop = button;
            }
            if (auto *button = dynamic_cast<juce::ToggleButton *>(component);
                button && button->getButtonText() == "Effect (off = Dry)")
                processed = button;
        }
        if (!voice || !effect || effect->getNumItems() != 30 || !play || !stop || !processed)
            return 43;
        juce::MemoryBlock previewBefore;
        processor.getStateInformation(previewBefore);
        for (int selectedEffect : {1, 28, 29, 30}) {
            effect->setSelectedId(selectedEffect, juce::sendNotificationSync);
            for (int selectedVoice : {1, 2}) {
                voice->setSelectedId(selectedVoice, juce::sendNotificationSync);
                play->onClick();
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
                bool playing = false;
                juce::AudioBuffer<float> previewAudio(2, 512);
                juce::MidiBuffer previewMidi;
                while (std::chrono::steady_clock::now() < deadline) {
                    previewAudio.clear();
                    processor.processBlock(previewAudio, previewMidi);
                    juce::Timer::callPendingTimersSynchronously();
                    if (processor.effectPreview().status().state ==
                        darkvoice::PreviewState::Playing) {
                        playing = true;
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                }
                if (!playing || processor.effectPreview().status().effect != selectedEffect ||
                    processor.effectPreview().status().voice !=
                        (selectedVoice == 1 ? darkvoice::PreviewVoice::Male
                                            : darkvoice::PreviewVoice::Female))
                    return 44;
                float previewPeak = 0;
                for (int block = 0; block < 96; ++block) {
                    previewAudio.clear();
                    processor.processBlock(previewAudio, previewMidi);
                    previewPeak = std::max(
                        previewPeak, previewAudio.getMagnitude(0, previewAudio.getNumSamples()));
                }
                if (previewPeak <= .0001f)
                    return 49;
                const auto cursor = processor.effectPreview().status().positionFrames;
                processed->setToggleState(false, juce::dontSendNotification);
                processed->onClick();
                previewAudio.clear();
                processor.processBlock(previewAudio, previewMidi);
                if (processor.effectPreview().status().processed ||
                    processor.effectPreview().status().positionFrames <= cursor)
                    return 45;
                processed->setToggleState(true, juce::dontSendNotification);
                processed->onClick();
                if (!processor.effectPreview().status().processed)
                    return 46;
                stop->onClick();
                for (int i = 0; i < 32; ++i) {
                    previewAudio.clear();
                    processor.processBlock(previewAudio, previewMidi);
                }
                if (processor.effectPreview().status().state == darkvoice::PreviewState::Playing)
                    return 47;
            }
        }
        if (argc > 2) {
            // Let the progress bar's own timer observe the stopped playback
            // before capturing; audio above deliberately runs faster than wall time.
            for (int tick = 0; tick < 2; ++tick) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                juce::Timer::callPendingTimersSynchronously();
            }
            auto previewImage =
                previewWindow->createComponentSnapshot(previewWindow->getLocalBounds());
            auto output = juce::File::getCurrentWorkingDirectory().getChildFile(argv[2]);
            output.deleteFile();
            auto stream = output.createOutputStream();
            if (!stream || !juce::PNGImageFormat().writeImageToStream(previewImage, *stream))
                return 50;
        }
        previewWindow->closeButtonPressed();
        if (previewWindow->isVisible())
            return 48;
        juce::MemoryBlock previewAfter;
        processor.getStateInformation(previewAfter);
        if (previewBefore != previewAfter)
            return 49;
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
        std::cout << "GUI smoke passed: 67 controls, all parameter attachments, bypass, actual "
                     "factory preset audio, dry-pitch feedback, snapshot\n";
        return 0;
    }();
    root.deleteRecursively();
    return result;
}
