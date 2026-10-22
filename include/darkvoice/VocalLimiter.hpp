#pragma once
#include <cstddef>
#include <span>
namespace darkvoice {
struct VocalLimiterParameters {
    float ceilingDb = -1, releaseMs = 100;
};
// Linked, zero-latency sample-peak limiting; not true-peak or oversampled.
class VocalLimiter {
  public:
    static bool valid(const VocalLimiterParameters &) noexcept;
    bool prepare(double rate, std::size_t maxFrames, std::size_t channels) noexcept;
    bool setParameters(const VocalLimiterParameters &) noexcept;
    bool process(std::span<float *> channels, std::size_t frames) noexcept;
    void reset() noexcept;

  private:
    double rate_ = 0, gain_ = 1, ceiling_ = 0, release_ = 0;
    std::size_t maxFrames_ = 0, channels_ = 0;
};
} // namespace darkvoice
