#pragma once
#include <cstddef>
#include <darkvoice/QualityMode.hpp>
#include <memory>
#include <optional>
#include <span>
namespace darkvoice {
struct HarmonicCharacterParameters {
    float amount = 0;
};
// Replaces input with an additive harmonic contribution, not a complete wet path.
class HarmonicCharacter {
  public:
    HarmonicCharacter() noexcept;
    ~HarmonicCharacter();
    HarmonicCharacter(const HarmonicCharacter &) = delete;
    HarmonicCharacter &operator=(const HarmonicCharacter &) = delete;
    static constexpr std::size_t maximumLatencySamples = 368;
    static constexpr std::size_t latencyForQuality(QualityMode mode) noexcept {
        switch (mode) {
        case QualityMode::Live:
            return 124;
        case QualityMode::Studio:
            return 184;
        case QualityMode::Offline:
            return 368;
        }
        return 0;
    }
    static bool valid(const HarmonicCharacterParameters &) noexcept;
    bool prepare(double rate, std::size_t maxFrames, std::size_t channels,
                 QualityMode quality = QualityMode::Live) noexcept;
    bool setParameters(const HarmonicCharacterParameters &) noexcept;
    bool process(std::span<float *> channels, std::size_t frames) noexcept;
    void reset() noexcept;
    std::size_t latencySamples() const noexcept;
    std::optional<QualityMode> qualityMode() const noexcept;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    HarmonicCharacterParameters target_{};
};
} // namespace darkvoice
