#pragma once
#include "DryPitchAnalyzer.hpp"
#include "PresetBank.hpp"
#include <array>
#include <atomic>
#include <darkvoice/AudioEngine.hpp>
#include <darkvoice/ParameterSchema.hpp>
#include <juce_audio_processors/juce_audio_processors.h>

class DarkVoiceProcessor final : public juce::AudioProcessor {
  public:
    explicit DarkVoiceProcessor(bool mastering = false);
    const juce::String getName() const override;
    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout &) const override;
    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;
    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override {
        return true;
    }
    double getTailLengthSeconds() const override {
        return 40.0;
    }
    bool acceptsMidi() const override {
        return false;
    }
    bool producesMidi() const override {
        return false;
    }
    bool isMidiEffect() const override {
        return false;
    }
    int getNumPrograms() override {
        return 1;
    }
    int getCurrentProgram() override {
        return 0;
    }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override {
        return "Default";
    }
    void changeProgramName(int, const juce::String &) override {}
    void getStateInformation(juce::MemoryBlock &) override;
    void setStateInformation(const void *, int) override;
    bool isMastering() const noexcept {
        return mastering_;
    }
    float inputPeak() const noexcept {
        return inputPeak_.load(std::memory_order_relaxed);
    }
    float outputPeak() const noexcept {
        return outputPeak_.load(std::memory_order_relaxed);
    }
    darkvoice::PresetValues snapshotPresetValues() const;
    bool applyPreset(const darkvoice::PresetValues &);
    float dryPitchHz() const noexcept {
        return dryAnalysis_.frequencyHz();
    }
    float dryPitchConfidence() const noexcept {
        return dryAnalysis_.confidence();
    }
    juce::AudioProcessorValueTreeState parameters;

  private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameters();
    darkvoice::EngineParameters snapshot() const noexcept;
    darkvoice::AudioEngine engine_;
    darkvoice::DryPitchAnalyzer dryAnalysis_;
    std::array<std::atomic<float> *, darkvoice::parameterSchema.size()> raw_{};
    std::atomic<float> inputPeak_{0}, outputPeak_{0};
    std::size_t maximumFrames_ = 0;
    bool prepared_ = false;
    bool mastering_ = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DarkVoiceProcessor)
};
