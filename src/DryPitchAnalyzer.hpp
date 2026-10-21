#pragma once
#include <array>
#include <atomic>
#include <darkvoice/PitchDetector.hpp>
#include <juce_audio_basics/juce_audio_basics.h>
namespace darkvoice {
// Single audio-thread producer, single analysis-thread consumer. No audio mutation.
class DryPitchAnalyzer final : private juce::Thread {
  public:
    DryPitchAnalyzer();
    ~DryPitchAnalyzer() override;
    bool prepare(double rate);
    void release();
    void push(const juce::AudioBuffer<float> &input) noexcept;
    float frequencyHz() const noexcept {
        return frequency_.load(std::memory_order_relaxed);
    }
    float confidence() const noexcept {
        return confidence_.load(std::memory_order_relaxed);
    }

  private:
    void run() override;
    static constexpr int ringSize = 32769, windowSize = 8192, hopSize = 4096;
    juce::AbstractFifo fifo_{ringSize};
    std::array<float, ringSize> ring_{};
    std::array<float, windowSize> history_{}, analysis_{};
    PitchDetector detector_;
    std::atomic<float> frequency_{0}, confidence_{0};
    std::atomic<bool> discontinuity_{false};
    bool ready_ = false; // Lifecycle is serialized by the host outside processBlock.
};
} // namespace darkvoice
