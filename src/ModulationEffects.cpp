#include <algorithm>
#include <cmath>
#include <darkvoice/ModulationEffects.hpp>
#include <limits>
#include <numbers>
namespace darkvoice {
namespace {
bool range(float value, float minimum, float maximum) noexcept {
    return std::isfinite(value) && value >= minimum && value <= maximum;
}
double sample(double value) noexcept {
    return std::isfinite(value) && std::abs(value) >= std::numeric_limits<float>::min() ? value : 0;
}
float output(double value) noexcept {
    constexpr double limit = std::numeric_limits<float>::max();
    return static_cast<float>(std::clamp(sample(value), -limit, limit));
}
} // namespace
bool ModulationEffects::valid(const ModulationParameters &p) noexcept {
    return range(p.chorusRateHz, .1f, 5) && range(p.chorusDepthMs, 0, 12) &&
           range(p.chorusMix, 0, .5f) && range(p.stereoWidth, 0, 2);
}
bool ModulationEffects::prepare(double rate, std::size_t maxFrames, std::size_t channels) noexcept {
    rate_ = 0;
    if ((rate != 44100 && rate != 48000 && rate != 88200 && rate != 96000) || maxFrames == 0 ||
        maxFrames > 65536 || (channels != 1 && channels != 2))
        return false;
    try {
        const auto size = static_cast<std::size_t>(std::ceil(.030 * rate)) + 2;
        for (std::size_t c = 0; c < channels; ++c)
            rings_[c].assign(size, 0);
    } catch (...) {
        return false;
    }
    rate_ = rate;
    maxFrames_ = maxFrames;
    channels_ = channels;
    smoothing_ = 1.0 - std::exp(-1.0 / (.010 * rate));
    target_ = {};
    reset();
    return true;
}
bool ModulationEffects::setParameters(const ModulationParameters &p) noexcept {
    if (!valid(p))
        return false;
    target_ = p;
    return true;
}
void ModulationEffects::reset() noexcept {
    current_ = target_;
    position_ = 0;
    phase_ = 0;
    for (auto &ring : rings_)
        std::fill(ring.begin(), ring.end(), 0.0f);
}
bool ModulationEffects::process(std::span<float *> channels, std::size_t frames) noexcept {
    if (rate_ == 0 || channels.size() != channels_ || frames > maxFrames_)
        return false;
    for (std::size_t c = 0; c < channels.size(); ++c) {
        if (frames != 0 && channels[c] == nullptr)
            return false;
        for (std::size_t prior = 0; prior < c; ++prior)
            if (channels[c] == channels[prior])
                return false;
    }
    const auto smooth = [this](float &value, float target) {
        value = static_cast<float>(value + smoothing_ * (target - value));
        if (std::abs(value - target) < 1.0e-4f * std::max(1.0f, std::abs(target)))
            value = target;
    };
    const auto size = rings_[0].size();
    for (std::size_t i = 0; i < frames; ++i) {
        smooth(current_.chorusRateHz, target_.chorusRateHz);
        smooth(current_.chorusDepthMs, target_.chorusDepthMs);
        smooth(current_.chorusMix, target_.chorusMix);
        smooth(current_.stereoWidth, target_.stereoWidth);
        std::array<double, 2> wet{};
        for (std::size_t c = 0; c < channels_; ++c) {
            const double input = sample(channels[c][i]);
            auto &ring = rings_[c];
            ring[position_] = static_cast<float>(input);
            const double wave = std::sin(2 * std::numbers::pi * (phase_ + .25 * c));
            const double delay = (.015 + .001 * current_.chorusDepthMs * wave) * rate_;
            double location = static_cast<double>(position_) - delay;
            if (location < 0)
                location += size;
            const auto first = static_cast<std::size_t>(location);
            const auto second = first + 1 == size ? 0 : first + 1;
            const double fraction = location - first;
            const double delayed =
                ring[first] + fraction * (static_cast<double>(ring[second]) - ring[first]);
            wet[c] = current_.chorusMix == 0
                         ? input
                         : input * (1.0 - current_.chorusMix) + delayed * current_.chorusMix;
        }
        if (channels_ == 2 && current_.stereoWidth != 1) {
            const double mid = .5 * (wet[0] + wet[1]);
            const double side = .5 * (wet[0] - wet[1]) * current_.stereoWidth;
            wet[0] = mid + side;
            wet[1] = mid - side;
        }
        for (std::size_t c = 0; c < channels_; ++c)
            channels[c][i] = output(wet[c]);
        position_ = position_ + 1 == size ? 0 : position_ + 1;
        phase_ += current_.chorusRateHz / rate_;
        if (phase_ >= 1)
            phase_ -= 1;
    }
    return true;
}
} // namespace darkvoice
