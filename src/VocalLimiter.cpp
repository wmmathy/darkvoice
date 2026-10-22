#include <algorithm>
#include <array>
#include <cmath>
#include <darkvoice/VocalLimiter.hpp>
#include <limits>
namespace darkvoice {
namespace {
bool range(float value, float low, float high) noexcept {
    return std::isfinite(value) && value >= low && value <= high;
}
double sample(float value) noexcept {
    return std::isfinite(value) && std::abs(value) >= std::numeric_limits<float>::min() ? value : 0;
}
} // namespace
bool VocalLimiter::valid(const VocalLimiterParameters &p) noexcept {
    return range(p.ceilingDb, -24, 0) && range(p.releaseMs, 5, 1000);
}
bool VocalLimiter::prepare(double rate, std::size_t maxFrames, std::size_t channels) noexcept {
    rate_ = 0;
    if ((rate != 44100 && rate != 48000 && rate != 88200 && rate != 96000) || maxFrames == 0 ||
        maxFrames > 65536 || (channels != 1 && channels != 2))
        return false;
    rate_ = rate;
    maxFrames_ = maxFrames;
    channels_ = channels;
    setParameters({});
    reset();
    return true;
}
bool VocalLimiter::setParameters(const VocalLimiterParameters &p) noexcept {
    if (!valid(p))
        return false;
    const double exactCeiling = std::pow(10., p.ceilingDb / 20.);
    float representable = static_cast<float>(exactCeiling);
    if (static_cast<double>(representable) > exactCeiling)
        representable = std::nextafter(representable, 0.f);
    ceiling_ = representable;
    release_ = rate_ ? 1 - std::exp(-1 / (.001 * p.releaseMs * rate_)) : 0;
    return true;
}
void VocalLimiter::reset() noexcept {
    gain_ = 1;
}
bool VocalLimiter::process(std::span<float *> channels, std::size_t frames) noexcept {
    if (rate_ == 0 || channels.size() != channels_ || frames > maxFrames_)
        return false;
    if (frames == 0)
        return true;
    for (std::size_t c = 0; c < channels_; ++c) {
        if (channels[c] == nullptr)
            return false;
        for (std::size_t other = 0; other < c; ++other)
            if (channels[c] == channels[other])
                return false;
    }
    for (std::size_t i = 0; i < frames; ++i) {
        std::array<double, 2> input{};
        double peak = 0;
        for (std::size_t c = 0; c < channels_; ++c) {
            input[c] = sample(channels[c][i]);
            peak = std::max(peak, std::abs(input[c]));
        }
        const double maximumGain = peak > ceiling_ ? ceiling_ / peak : 1;
        gain_ += release_ * (1 - gain_);
        if (1 - gain_ < 1e-9)
            gain_ = 1;
        gain_ = std::min(gain_, maximumGain);
        for (std::size_t c = 0; c < channels_; ++c) {
            const double value = std::clamp(input[c] * gain_, -ceiling_, ceiling_);
            channels[c][i] =
                std::abs(value) < std::numeric_limits<float>::min() ? 0 : static_cast<float>(value);
        }
    }
    return true;
}
} // namespace darkvoice
