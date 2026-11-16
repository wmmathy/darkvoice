#pragma once
#include "EffectPreviewService.hpp"
#include <juce_gui_basics/juce_gui_basics.h>

class EffectPreviewPanel final : public juce::Component, private juce::Timer {
  public:
    explicit EffectPreviewPanel(darkvoice::EffectPreviewService &);
    ~EffectPreviewPanel() override;
    void resized() override;
    void paint(juce::Graphics &) override;

  private:
    void timerCallback() override;
    void selectionChanged();
    darkvoice::EffectPreviewService &service_;
    juce::ComboBox effect_, voice_, source_;
    juce::TextButton play_{"Play"}, stop_{"Stop"}, credits_{"Recording credits"};
    juce::ToggleButton processed_{"Effect (off = Dry)"};
    juce::Slider level_;
    juce::Label title_, help_, status_, levelLabel_{"", "Preview level"};
    juce::TextEditor notices_;
    double progress_ = 0;
    juce::ProgressBar progressBar_{progress_};
};
