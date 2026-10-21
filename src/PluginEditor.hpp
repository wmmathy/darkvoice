#pragma once
#include "PluginProcessor.hpp"
#include <array>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
class DarkVoiceEditor final : public juce::AudioProcessorEditor, private juce::Timer {
  public:
    explicit DarkVoiceEditor(DarkVoiceProcessor &);
    ~DarkVoiceEditor() override;
    void paint(juce::Graphics &) override;
    void resized() override;

  private:
    void timerCallback() override;
    DarkVoiceProcessor &processor_;
    juce::LookAndFeel_V4 look_;
    std::array<juce::Slider, 7> sliders_;
    std::array<juce::Label, 7> labels_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 7>
        attachments_;
    juce::ToggleButton bypass_{"Bypass processing"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment_;
    juce::Label meter_;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DarkVoiceEditor)
};
