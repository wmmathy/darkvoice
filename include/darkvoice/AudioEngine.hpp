#pragma once
#include <array>
#include <cstddef>
#include <span>
namespace darkvoice {
// v1: snapshots are delivered by the host on the audio thread between blocks.
struct EngineParameters {
    float inputDb = 0.0f;
    float outputDb = 0.0f;
    float highPassHz = 20.0f;
    float drive = 0.0f;
    float thresholdDb = -18.0f;
    float ratio = 1.0f;
    float mix = 1.0f;
    bool bypass = false;
};
struct EngineMeters {
    float inputPeak = 0.0f;
    float outputPeak = 0.0f;
};
class AudioEngine {
  public:
    bool prepare(double sampleRate, std::size_t maxFrames, std::size_t channels) noexcept;
    bool setParameters(const EngineParameters &) noexcept;
    bool process(std::span<float *> channels, std::size_t frames) noexcept;
    void reset() noexcept;
    [[nodiscard]] EngineMeters meters() const noexcept {
        return meters_;
    }
    [[nodiscard]] std::size_t latencySamples() const noexcept {
        return 0;
    }

  private:
    double rate_ = 0.0;
    std::size_t maxFrames_ = 0, channels_ = 0;
    EngineParameters target_{}, current_{};
    std::array<float, 2> previousInput_{}, previousOutput_{}, envelope_{};
    EngineMeters meters_{};
};
} // namespace darkvoice
