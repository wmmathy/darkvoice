#pragma once
#include <array>
#include <cstddef>
#include <span>
#include <vector>
namespace darkvoice {
struct StereoDelayParameters {
    float timeMs = 100, feedback = .25f, mix = 0, bpm = 120;
    int mode = 0;
    bool sync = false;
    int division = 5;
};
// Musical delay, not host latency compensation. Modes: repeat, single, ping-pong.
class StereoDelay {
  public:
    static bool valid(const StereoDelayParameters &) noexcept;
    bool prepare(double rate, std::size_t maxFrames, std::size_t channels) noexcept;
    bool setParameters(const StereoDelayParameters &) noexcept;
    bool process(std::span<float *> channels, std::size_t frames) noexcept;
    void reset() noexcept;

  private:
    double delaySamples() const noexcept;
    std::array<std::vector<float>, 2> rings_;
    double rate_ = 0, smoothing_ = 0;
    double delay_ = 0, feedback_ = 0, mix_ = 0, cross_ = 0;
    std::size_t maxFrames_ = 0, channels_ = 0, position_ = 0;
    StereoDelayParameters target_{};
};
} // namespace darkvoice
