#include "BandlimitedColourOversampling.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <darkvoice/HarmonicCharacter.hpp>
#include <limits>
#include <numbers>
namespace darkvoice {
namespace {
double clean(double x) noexcept {
    return std::isfinite(x) && std::abs(x) >= std::numeric_limits<double>::min() ? x : 0;
}
double input(float x) noexcept {
    return std::isfinite(x) && std::abs(x) >= std::numeric_limits<float>::min() ? x : 0;
}
struct Dc {
    double previous = 0, output = 0;
    double process(double x, double pole) noexcept {
        output = clean(x - previous + pole * output);
        previous = x;
        return output;
    }
};
struct Channel {
    bandlimited_colour_detail::Oversampler16 odd, even;
    Dc oddDc, evenDc;
    void reset() noexcept {
        odd.reset();
        even.reset();
        oddDc = {};
        evenDc = {};
    }
};
} // namespace
struct HarmonicCharacter::Impl {
    std::array<Channel, 2> channel;
    std::size_t maxFrames = 0, channels = 0;
    QualityMode quality = QualityMode::Live;
    double smooth = 0, dcPole = 0, negative = 0, positive = 0;
};
HarmonicCharacter::HarmonicCharacter() noexcept = default;
HarmonicCharacter::~HarmonicCharacter() = default;
bool HarmonicCharacter::valid(const HarmonicCharacterParameters &p) noexcept {
    return std::isfinite(p.amount) && p.amount >= -1 && p.amount <= 1;
}
bool HarmonicCharacter::prepare(double rate, std::size_t frames, std::size_t channels,
                                QualityMode quality) noexcept {
    impl_.reset();
    if (!validQualityMode(quality) ||
        (rate != 44100 && rate != 48000 && rate != 88200 && rate != 96000) || frames == 0 ||
        frames > 65536 || (channels != 1 && channels != 2))
        return false;
    try {
        auto next = std::make_unique<Impl>();
        next->maxFrames = frames;
        next->channels = channels;
        next->quality = quality;
        next->smooth = 1 - std::exp(-1 / (.010 * rate));
        next->dcPole = std::exp(-2 * std::numbers::pi * 10 / rate);
        const auto boundary =
            bandlimited_colour_detail::guardKernel(quality == QualityMode::Live     ? 193
                                                   : quality == QualityMode::Studio ? 257
                                                                                    : 513);
        const auto higher =
            bandlimited_colour_detail::kernel(quality == QualityMode::Live     ? 65
                                              : quality == QualityMode::Studio ? 129
                                                                               : 257);
        for (std::size_t c = 0; c < channels; ++c) {
            next->channel[c].odd.prepare(boundary, higher, boundary);
            next->channel[c].even.prepare(boundary, higher, boundary);
        }
        impl_ = std::move(next);
        target_ = {};
        reset();
        return true;
    } catch (...) {
        return false;
    }
}
bool HarmonicCharacter::setParameters(const HarmonicCharacterParameters &p) noexcept {
    if (!valid(p))
        return false;
    target_ = p;
    return true;
}
void HarmonicCharacter::reset() noexcept {
    if (!impl_)
        return;
    impl_->negative = std::max(-static_cast<double>(target_.amount), 0.);
    impl_->positive = std::max(static_cast<double>(target_.amount), 0.);
    for (auto &c : impl_->channel)
        c.reset();
}
std::size_t HarmonicCharacter::latencySamples() const noexcept {
    return impl_ ? latencyForQuality(impl_->quality) : 0;
}
std::optional<QualityMode> HarmonicCharacter::qualityMode() const noexcept {
    return impl_ ? std::optional<QualityMode>(impl_->quality) : std::nullopt;
}
bool HarmonicCharacter::process(std::span<float *> channels, std::size_t frames) noexcept {
    if (!impl_ || channels.size() != impl_->channels || frames > impl_->maxFrames)
        return false;
    if (!frames)
        return true;
    for (std::size_t c = 0; c < channels.size(); ++c) {
        if (!channels[c])
            return false;
        for (std::size_t j = 0; j < c; ++j)
            if (channels[c] == channels[j])
                return false;
    }
    auto &p = *impl_;
    const double n = std::max(-static_cast<double>(target_.amount), 0.);
    const double positive = std::max(static_cast<double>(target_.amount), 0.);
    const double normalization = std::tanh(2.);
    const auto smooth = [&](double &x, double target) {
        x = clean(x + p.smooth * (target - x));
        if (std::abs(x - target) < 1e-7)
            x = target;
    };
    for (std::size_t i = 0; i < frames; ++i) {
        smooth(p.negative, n);
        smooth(p.positive, positive);
        for (std::size_t c = 0; c < channels.size(); ++c) {
            auto &s = p.channel[c];
            const double x = input(channels[c][i]);
            const double odd =
                s.oddDc.process(s.odd.process(x,
                                              [&](double v) {
                                                  const double z = std::tanh(2 * v) / normalization;
                                                  return z * z * z;
                                              }),
                                p.dcPole);
            const double even =
                s.evenDc.process(s.even.process(x,
                                                [&](double v) {
                                                    const double z =
                                                        std::tanh(2 * v) / normalization;
                                                    return z * z;
                                                }),
                                 p.dcPole);
            const double delta = .25 * (p.negative * odd + p.positive * even);
            const float value = static_cast<float>(delta);
            channels[c][i] =
                std::isfinite(value) && std::abs(value) >= std::numeric_limits<float>::min() ? value
                                                                                             : 0;
        }
    }
    return true;
}
} // namespace darkvoice
