#pragma once
#include <cstddef>
#include <span>

namespace darkvoice {
struct NoiseGateParameters {
    float thresholdDb = -50.0f;
    float ratio = 1.0f;
    float releaseMs = 120.0f;
};
// A linked downward expander. Lifecycle and parameter delivery have one owner.
class NoiseGate {
  public:
    static bool valid(const NoiseGateParameters &) noexcept;
    bool prepare(double rate, std::size_t maxFrames, std::size_t channels) noexcept;
    bool setParameters(const NoiseGateParameters &) noexcept;
    bool process(std::span<float *> channels, std::size_t frames) noexcept;
    void reset() noexcept;

  private:
    double rate_ = 0, envelope_ = 0, gainDb_ = 0;
    double smoothing_ = 0, detectorRelease_ = 0, opening_ = 0;
    std::size_t maxFrames_ = 0, channels_ = 0;
    NoiseGateParameters current_{}, target_{};
};
} // namespace darkvoice
