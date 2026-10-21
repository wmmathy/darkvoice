#pragma once
#include <cstddef>
#include <memory>
#include <span>
namespace darkvoice {
struct ToneEffectsParameters {
    float eqLowHz = 150.0f, eqLowGainDb = 0.0f;
    float eqMidHz = 1200.0f, eqMidGainDb = 0.0f, eqMidQ = 0.7f;
    float eqHighHz = 6000.0f, eqHighGainDb = 0.0f;
    float delayMs = 180.0f, delayFeedback = 0.25f, delayMix = 0.0f;
    float reverbSize = 0.5f, reverbDamping = 0.5f, reverbMix = 0.0f;
};
class ToneEffects {
  public:
    ToneEffects() noexcept;
    ~ToneEffects();
    ToneEffects(const ToneEffects &) = delete;
    ToneEffects &operator=(const ToneEffects &) = delete;
    bool prepare(double rate, std::size_t maxFrames, std::size_t channels) noexcept;
    static bool valid(const ToneEffectsParameters &) noexcept;
    bool setParameters(const ToneEffectsParameters &) noexcept;
    bool process(std::span<float *> channels, std::size_t frames) noexcept;
    void reset() noexcept;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    ToneEffectsParameters target_{};
};
} // namespace darkvoice
