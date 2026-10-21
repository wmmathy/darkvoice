#pragma once
#include <array>
#include <cstddef>
#include <span>

namespace darkvoice {
struct DeEsserParameters {
    float frequencyHz = 6000.0f;
    float thresholdDb = -24.0f;
    float maxReductionDb = 0.0f;
};
class DeEsser {
  public:
    static bool valid(const DeEsserParameters &) noexcept;
    bool prepare(double rate, std::size_t maxFrames, std::size_t channels) noexcept;
    bool setParameters(const DeEsserParameters &) noexcept;
    bool process(std::span<float *> channels, std::size_t frames) noexcept;
    void reset() noexcept;

  private:
    double rate_ = 0, envelope_ = 0, gainDb_ = 0;
    double smoothing_ = 0, detectorAttack_ = 0, release_ = 0, gainAttack_ = 0;
    std::size_t maxFrames_ = 0, channels_ = 0;
    std::array<double, 2> low_{};
    DeEsserParameters current_{}, target_{};
};
} // namespace darkvoice
