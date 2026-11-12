#pragma once
#include <cstddef>
#include <darkvoice/Colour.hpp>
#include <darkvoice/QualityMode.hpp>
#include <memory>
#include <optional>
#include <span>
namespace darkvoice {
class BandlimitedColour {
  public:
    BandlimitedColour() noexcept;
    ~BandlimitedColour();
    BandlimitedColour(const BandlimitedColour &) = delete;
    BandlimitedColour &operator=(const BandlimitedColour &) = delete;
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
    std::optional<QualityMode> qualityMode() const noexcept;
    static bool valid(const ColourParameters &) noexcept;
    bool prepare(double rate, std::size_t maxFrames, std::size_t channels,
                 QualityMode quality = QualityMode::Live) noexcept;
    bool setParameters(const ColourParameters &) noexcept;
    bool process(std::span<float *> channels, std::size_t frames) noexcept;
    void reset() noexcept;
    std::size_t latencySamples() const noexcept;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    ColourParameters target_{};
};
} // namespace darkvoice
