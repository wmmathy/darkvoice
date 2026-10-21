#pragma once
#include <array>
#include <cstddef>
#include <span>
#include <vector>
namespace darkvoice {
struct ModulationParameters {
    float chorusRateHz = .8f, chorusDepthMs = 6.0f, chorusMix = 0.0f, stereoWidth = 1.0f;
};
class ModulationEffects {
  public:
    static bool valid(const ModulationParameters &) noexcept;
    bool prepare(double rate, std::size_t maxFrames, std::size_t channels) noexcept;
    bool setParameters(const ModulationParameters &) noexcept;
    bool process(std::span<float *> channels, std::size_t frames) noexcept;
    void reset() noexcept;

  private:
    double rate_ = 0, smoothing_ = 0, phase_ = 0;
    std::size_t maxFrames_ = 0, channels_ = 0, position_ = 0;
    std::array<std::vector<float>, 2> rings_;
    ModulationParameters current_{}, target_{};
};
} // namespace darkvoice
