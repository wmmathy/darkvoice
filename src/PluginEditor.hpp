#pragma once
#include "PluginProcessor.hpp"
#include <array>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
class DarkVoiceLookAndFeel final : public juce::LookAndFeel_V4 {
  public:
    int getTabButtonBestWidth(juce::TabBarButton &, int) override {
        return 42;
    }
    int getTabButtonOverlap(int) override {
        return 0;
    }
    void drawTabButton(juce::TabBarButton &, juce::Graphics &, bool, bool) override;
};
class DarkVoiceEditor final : public juce::AudioProcessorEditor, private juce::Timer {
  public:
    explicit DarkVoiceEditor(DarkVoiceProcessor &, juce::File presetRoot = {});
    ~DarkVoiceEditor() override;
    void paint(juce::Graphics &) override;
    void resized() override;

  private:
    void timerCallback() override;
    void refreshPresets(const juce::String &selected = {});
    void loadSelected();
    juce::String selectedId() const;
    void showResult(const juce::Result &, const juce::String &success);
    void chooseImport();
    void chooseExport();
    DarkVoiceProcessor &processor_;
    DarkVoiceLookAndFeel look_;
    juce::TabbedComponent tabs_{juce::TabbedButtonBar::TabsAtLeft};
    std::array<juce::Component, 11> pages_;
    std::array<juce::Slider, 54> sliders_;
    std::array<juce::Label, 54> labels_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 54>
        attachments_;
    juce::ToggleButton bypass_{"Bypass processing"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment_;
    juce::Label meter_, status_, pitch_, tempo_;
    darkvoice::PresetBank bank_;
    juce::TextEditor search_, name_;
    juce::ComboBox presets_;
    juce::ToggleButton favoritesOnly_{"Favorites"}, favorite_{"Favorite"};
    juce::TextButton load_{"Load"}, save_{"Save as"}, rename_{"Rename"}, import_{"Import"},
        export_{"Export"};
    std::vector<juce::String> visibleIds_;
    std::unique_ptr<juce::FileChooser> chooser_;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DarkVoiceEditor)
};
