#pragma once
#include <cstddef>
#include <memory>
#include <span>
namespace darkvoice {
struct SpaceReverbParameters {
    int model = 0;
    float decay = .5f, toneHz = 8000, preDelayMs = 0, mix = 0;
};
// Distinct algorithmic Room/Hall/Plate networks, not physical-space emulation.
class SpaceReverb {
  public:
    SpaceReverb() noexcept;
    ~SpaceReverb();
    static bool valid(const SpaceReverbParameters &) noexcept;
    bool prepare(double rate, std::size_t maxFrames, std::size_t channels) noexcept;
    bool setParameters(const SpaceReverbParameters &) noexcept;
    bool process(std::span<float *> channels, std::size_t frames) noexcept;
    void reset() noexcept;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    SpaceReverbParameters target_{};
};
} // namespace darkvoice
