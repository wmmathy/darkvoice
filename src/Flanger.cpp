#include <algorithm>
#include <cmath>
#include <darkvoice/Flanger.hpp>
#include <limits>
#include <numbers>
namespace darkvoice {
namespace {
bool range(float value, float low, float high) noexcept {
    return std::isfinite(value) && value >= low && value <= high;
}
float sample(double value) noexcept {
    if (!std::isfinite(value) || std::abs(value) < std::numeric_limits<float>::min())
        return 0;
    constexpr double limit = std::numeric_limits<float>::max();
    return static_cast<float>(std::clamp(value, -limit, limit));
}
} // namespace
bool Flanger::valid(const FlangerParameters &p) noexcept {
    return range(p.flangerRateHz, .05f, 5) && range(p.flangerDepthMs, 0, 4) &&
           range(p.flangerFeedback, -.85f, .85f) && range(p.flangerMix, 0, 1);
}
bool Flanger::prepare(double rate, std::size_t maxFrames, std::size_t channels) noexcept {
    rate_ = 0;
    if ((rate != 44100 && rate != 48000 && rate != 88200 && rate != 96000) || maxFrames == 0 ||
        maxFrames > 65536 || (channels != 1 && channels != 2))
        return false;
    try {
        const auto size = static_cast<std::size_t>(std::ceil(.010 * rate)) + 2;
        for (std::size_t c = 0; c < channels; ++c)
            rings_[c].assign(size, 0);
    } catch (...) {
        return false;
    }
    rate_ = rate;
    maxFrames_ = maxFrames;
    channels_ = channels;
    smoothing_ = 1 - std::exp(-1 / (.010 * rate));
    target_ = {};
    reset();
    return true;
}
bool Flanger::setParameters(const FlangerParameters &p) noexcept {
    if (!valid(p))
        return false;
    target_ = p;
    return true;
}
void Flanger::reset() noexcept {
    current_ = target_;
    position_ = 0;
    phase_ = 0;
    for (auto &ring : rings_)
        std::fill(ring.begin(), ring.end(), 0);
}
bool Flanger::process(std::span<float *> channels, std::size_t frames) noexcept {
    if (rate_ == 0 || channels.size() != channels_ || frames > maxFrames_)
        return false;
    for (std::size_t c = 0; c < channels_; ++c) {
        if (frames && channels[c] == nullptr)
            return false;
        for (std::size_t other = 0; other < c; ++other)
            if (channels[c] == channels[other])
                return false;
    }
    const auto smooth = [this](float &value, float goal) {
        value = static_cast<float>(value + smoothing_ * (goal - value));
        if (std::abs(value - goal) < 1e-4f * std::max(1.f, std::abs(goal)))
            value = goal;
    };
    const auto size = rings_[0].size();
    for (std::size_t i = 0; i < frames; ++i) {
        smooth(current_.flangerRateHz, target_.flangerRateHz);
        smooth(current_.flangerDepthMs, target_.flangerDepthMs);
        smooth(current_.flangerFeedback, target_.flangerFeedback);
        smooth(current_.flangerMix, target_.flangerMix);
        const double delay =
            (.0045 + .001 * current_.flangerDepthMs * std::sin(2 * std::numbers::pi * phase_)) *
            rate_;
        // Split before wrapping to keep near-integer taps inside the ring.
        const auto whole = static_cast<std::size_t>(delay);
        const double fraction = delay - whole;
        const auto first = position_ >= whole ? position_ - whole : position_ + size - whole;
        const auto second = first == 0 ? size - 1 : first - 1;
        for (std::size_t c = 0; c < channels_; ++c) {
            auto &ring = rings_[c];
            const double delayed =
                ring[first] + fraction * (static_cast<double>(ring[second]) - ring[first]);
            const float input = sample(channels[c][i]);
            ring[position_] = sample(input + current_.flangerFeedback * delayed);
            channels[c][i] =
                current_.flangerMix == 0
                    ? input
                    : sample(input * (1.0 - current_.flangerMix) + delayed * current_.flangerMix);
        }
        position_ = position_ + 1 == size ? 0 : position_ + 1;
        phase_ += current_.flangerRateHz / rate_;
        if (phase_ >= 1)
            phase_ -= 1;
    }
    return true;
}
} // namespace darkvoice
