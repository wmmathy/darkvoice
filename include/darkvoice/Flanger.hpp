#pragma once
#include <array>
#include <cstddef>
#include <span>
#include <vector>
namespace darkvoice {
struct FlangerParameters {
    float flangerRateHz = .3f, flangerDepthMs = 2, flangerFeedback = 0, flangerMix = 0;
};
class Flanger {
  public:
    static bool valid(const FlangerParameters &) noexcept;
    bool prepare(double rate, std::size_t maxFrames, std::size_t channels) noexcept;
    bool setParameters(const FlangerParameters &) noexcept;
    bool process(std::span<float *> channels, std::size_t frames) noexcept;
    void reset() noexcept;

  private:
    std::array<std::vector<float>, 2> rings_;
    double rate_ = 0, smoothing_ = 0, phase_ = 0;
    std::size_t maxFrames_ = 0, channels_ = 0, position_ = 0;
    FlangerParameters current_{}, target_{};
};
} // namespace darkvoice
