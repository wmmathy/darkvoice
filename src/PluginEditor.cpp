#include "PluginEditor.hpp"
namespace {
const juce::Colour background {0xff111418}, panel {0xff1c2229}, accent {0xffe54b52}, text {0xffecedf0};
constexpr std::array<const char*, 7> ids {"inputDb", "highPassHz", "drive", "thresholdDb", "ratio", "mix", "outputDb"};
constexpr std::array<const char*, 7> names {"INPUT GAIN", "HIGH-PASS", "WARMTH DRIVE", "COMPRESSOR THRESHOLD", "COMPRESSOR RATIO", "DRY / WET", "OUTPUT GAIN"};
constexpr std::array<const char*, 7> suffixes {" dB", " Hz", "", " dB", ":1", "", " dB"};
}
DarkVoiceEditor::DarkVoiceEditor(DarkVoiceProcessor& p) : AudioProcessorEditor(p), processor_(p) {
    look_.setColour(juce::Slider::thumbColourId, accent);
    look_.setColour(juce::Slider::trackColourId, accent);
    look_.setColour(juce::Slider::backgroundColourId, juce::Colour(0xff303943));
    look_.setColour(juce::Slider::textBoxTextColourId, text);
    look_.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    look_.setColour(juce::Slider::textBoxBackgroundColourId, panel);
    look_.setColour(juce::ToggleButton::textColourId, text);
    look_.setColour(juce::ToggleButton::tickColourId, accent);
    setLookAndFeel(&look_);
    for (std::size_t i = 0; i < sliders_.size(); ++i) {
        auto& slider = sliders_[i];
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 90, 28);
        slider.setTextValueSuffix(suffixes[i]);
        slider.setName(names[i]);
        slider.setTitle(names[i]);
        slider.setTooltip("Automatable " + juce::String(names[i]).toLowerCase());
        addAndMakeVisible(slider);
        labels_[i].setText(names[i], juce::dontSendNotification);
        labels_[i].setColour(juce::Label::textColourId, juce::Colour(0xffb5bdc7));
        labels_[i].setFont(juce::Font(juce::FontOptions(12.0f)));
        addAndMakeVisible(labels_[i]);
        attachments_[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters, ids[i], slider);
    }
    addAndMakeVisible(bypass_);
    bypassAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters, "bypass", bypass_);
    meter_.setColour(juce::Label::textColourId, text);
    meter_.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(meter_);
    setResizable(true, true);
    setResizeLimits(640, 610, 1100, 900);
    setSize(780, 640);
    timerCallback();
    startTimerHz(20);
}
DarkVoiceEditor::~DarkVoiceEditor() { stopTimer(); setLookAndFeel(nullptr); }
void DarkVoiceEditor::paint(juce::Graphics& g) {
    g.fillAll(background);
    g.setColour(accent);
    g.fillRect(24, 24, 4, 45);
    g.setColour(text);
    g.setFont(juce::Font(juce::FontOptions(27.0f, juce::Font::bold)));
    g.drawText(processor_.isMastering() ? "DARKVOICE / MASTER" : "DARKVOICE / VOCAL", 40, 19, getWidth() - 65, 34, juce::Justification::centredLeft);
    g.setFont(juce::Font(juce::FontOptions(13.0f)));
    g.setColour(juce::Colour(0xff9ba6b4));
    g.drawText("DEVELOPMENT BUILD  /  Gain · Filter · Saturation · Compression", 40, 55, getWidth() - 65, 24, juce::Justification::centredLeft);
    g.setColour(panel);
    g.fillRoundedRectangle(24.0f, 96.0f, static_cast<float>(getWidth() - 48), static_cast<float>(getHeight() - 176), 12.0f);
    g.setColour(juce::Colour(0xff9ba6b4));
    g.setFont(juce::Font(juce::FontOptions(12.0f)));
    g.drawText("Starter signal chain · Peak meters, not LUFS · Pitch / AI / true-peak mastering pending", 24, getHeight() - 36, getWidth() - 48, 22, juce::Justification::centredLeft);
}
void DarkVoiceEditor::resized() {
    const int rowHeight = (getHeight() - 210) / 7;
    for (std::size_t i = 0; i < sliders_.size(); ++i) {
        const int y = 111 + static_cast<int>(i) * rowHeight;
        labels_[i].setBounds(42, y, 190, 20);
        sliders_[i].setBounds(230, y - 3, getWidth() - 280, 29);
    }
    bypass_.setBounds(28, getHeight() - 69, 200, 28);
    meter_.setBounds(240, getHeight() - 69, getWidth() - 268, 28);
}
void DarkVoiceEditor::timerCallback() {
    const auto db = [](float value) { return value > 0 ? juce::String(juce::Decibels::gainToDecibels(value), 1) : juce::String("−inf"); };
    meter_.setText("IN " + db(processor_.inputPeak()) + " dBFS    OUT " + db(processor_.outputPeak()) + " dBFS", juce::dontSendNotification);
}
