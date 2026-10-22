#pragma once
#include <array>
#include <cstddef>
#include <span>
#include <vector>
namespace darkvoice {
struct AlignedMixParameters {
    float mix = 1;
    bool bypass = false;
};
class LatencyAlignedMix {
  public:
    bool prepare(double rate, std::size_t maxFrames, std::size_t channels,
                 std::size_t latencySamples) noexcept;
    bool setParameters(const AlignedMixParameters &) noexcept;
    bool process(std::span<const float *> originalDry, std::span<float *> processedWet,
                 std::size_t frames) noexcept;
    void reset() noexcept;
    std::size_t latencySamples() const noexcept;

  private:
    std::array<std::vector<float>, 2> rings_;
    std::size_t maxFrames_ = 0, channels_ = 0, latency_ = 0, position_ = 0;
    double smoothing_ = 0, wet_ = 1;
    AlignedMixParameters target_{};
};
} // namespace darkvoice
