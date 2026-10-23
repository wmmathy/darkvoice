#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace darkvoice {
struct DoublerParameters {
    float separationMs = 20, variationMs = 2, rateHz = .4f, mix = 0;
};
// Simulated double tracking using independent smooth random delay trajectories.
class Doubler {
  public:
    static bool valid(const DoublerParameters &) noexcept;
    bool prepare(double rate, std::size_t maxFrames, std::size_t channels) noexcept;
    bool setParameters(const DoublerParameters &) noexcept;
    bool process(std::span<float *> channels, std::size_t frames) noexcept;
    void reset() noexcept;

  private:
    struct Wander {
        std::uint32_t state = 1;
        double from = 0, to = 0, phase = 0;
        double random() noexcept;
        void reset(std::uint32_t seed) noexcept;
        double next(double step) noexcept;
    };
    std::array<std::vector<float>, 2> rings_;
    std::array<Wander, 2> wander_;
    double rate_ = 0, smoothing_ = 0;
    double separation_ = 20, variation_ = 2, speed_ = .4, mix_ = 0;
    std::size_t maxFrames_ = 0, channels_ = 0, position_ = 0;
    DoublerParameters target_{};
};
} // namespace darkvoice
