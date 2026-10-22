#pragma once
#include <cstddef>
#include <memory>
#include <span>
namespace darkvoice {
enum class PitchQuality { Live, Studio, Offline };
struct PitchFormantSpec {
    double sampleRate = 48000;
    std::size_t maxFrames = 512, channels = 1;
    PitchQuality quality = PitchQuality::Studio;
};
struct PitchFormantParameters {
    float pitchSemitones = 0, formantSemitones = 0;
    bool preserveFormants = true;
    float formantBaseHz = 0;
};
class PitchFormantProcessor {
  public:
    PitchFormantProcessor() noexcept;
    ~PitchFormantProcessor();
    static bool valid(const PitchFormantParameters &) noexcept;
    bool prepare(const PitchFormantSpec &) noexcept;
    bool setParameters(const PitchFormantParameters &) noexcept;
    bool process(std::span<float *> channels, std::size_t frames) noexcept;
    void reset() noexcept;
    std::size_t latencySamples() const noexcept;
    std::size_t tailSamples() const noexcept;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    PitchFormantParameters target_{};
};
} // namespace darkvoice
