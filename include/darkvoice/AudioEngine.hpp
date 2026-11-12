#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <darkvoice/Colour.hpp>
#include <darkvoice/DeEsser.hpp>
#include <darkvoice/Doubler.hpp>
#include <darkvoice/Flanger.hpp>
#include <darkvoice/ModulationEffects.hpp>
#include <darkvoice/NoiseGate.hpp>
#include <darkvoice/SpaceReverb.hpp>
#include <darkvoice/StereoDelay.hpp>
#include <darkvoice/ToneEffects.hpp>
#include <darkvoice/VocalLimiter.hpp>
#include <span>
#include <vector>
namespace darkvoice {
// v2: fixed56-sample alignment; snapshots are delivered by the host on the audio thread between
// blocks.
struct EngineParameters {
    float inputDb = 0.0f;
    float outputDb = 0.0f;
    float highPassHz = 20.0f;
    float drive = 0.0f;
    float thresholdDb = -18.0f;
    float ratio = 1.0f;
    float mix = 1.0f;
    bool bypass = false;
    float eqLowHz = 150.0f, eqLowGainDb = 0.0f;
    float eqMidHz = 1200.0f, eqMidGainDb = 0.0f, eqMidQ = 0.7f;
    float eqHighHz = 6000.0f, eqHighGainDb = 0.0f;
    float delayMs = 180.0f, delayFeedback = 0.25f, delayMix = 0.0f;
    float reverbSize = 0.5f, reverbDamping = 0.5f, reverbMix = 0.0f;
    float gateThresholdDb = -50.0f, gateRatio = 1.0f, gateReleaseMs = 120.0f;
    float deEssHz = 6000.0f, deEssThresholdDb = -24.0f, deEssReductionDb = 0.0f;
    float flangerRateHz = .3f, flangerDepthMs = 2, flangerFeedback = 0, flangerMix = 0;
    int spaceModel = 0;
    float spaceDecay = .5f, spaceToneHz = 8000, spacePreDelayMs = 0, spaceMix = 0;
    float doublerSeparationMs = 20, doublerVariationMs = 2, doublerRateHz = .4f, doublerMix = 0;
    float stereoDelayMs = 100, stereoDelayFeedback = .25f, stereoDelayMix = 0, stereoDelayBpm = 120;
    int stereoDelayMode = 0, stereoDelayDivision = 5;
    bool stereoDelaySync = false;
    bool limiterEnabled = false;
    float limiterCeilingDb = -1.0f, limiterReleaseMs = 100.0f;
    float chorusRateHz = .8f, chorusDepthMs = 6.0f, chorusMix = 0.0f, stereoWidth = 1.0f;
    int colourModel = 0;
    float colourDrive = .25f, colourToneHz = 16000, colourMix = 0;
};
struct EngineMeters {
    float inputPeak = 0.0f;
    float outputPeak = 0.0f;
};
class AudioEngine {
  public:
    bool prepare(double sampleRate, std::size_t maxFrames, std::size_t channels) noexcept;
    bool setParameters(const EngineParameters &) noexcept;
    bool process(std::span<float *> channels, std::size_t frames) noexcept;
    bool processBypassed(std::span<float *> channels, std::size_t frames) noexcept;
    void release() noexcept;
    void reset() noexcept;
    [[nodiscard]] EngineMeters meters() const noexcept {
        return meters_;
    }
    [[nodiscard]] std::size_t latencySamples() const noexcept {
        return rate_ == 0 ? 0 : 56;
    }

  private:
    bool processInternal(std::span<float *> channels, std::size_t frames,
                         bool forceBypass) noexcept;
    double rate_ = 0.0;
    std::size_t maxFrames_ = 0, channels_ = 0;
    EngineParameters target_{}, current_{};
    std::array<float, 2> previousInput_{}, previousOutput_{}, envelope_{};
    EngineMeters meters_{};
    ToneEffects effects_;
    NoiseGate gate_;
    DeEsser deEsser_;
    ModulationEffects modulation_;
    Flanger flanger_;
    Doubler doubler_;
    StereoDelay stereoDelay_;
    SpaceReverb spaceReverb_;
    VocalLimiter limiter_;
    Colour colour_;
    std::array<std::vector<float>, 2> wet_;
    std::array<std::vector<std::uint32_t>, 2> aligned_;
    std::array<std::array<std::uint32_t, 56>, 2> rawQueue_{};
    std::size_t rawPosition_ = 0, resumeGuard_ = 0, resumeFade_ = 128;
    bool bypassed_ = false, limiterResetPending_ = false;
};
} // namespace darkvoice
