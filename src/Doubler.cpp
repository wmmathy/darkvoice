#include <algorithm>
#include <cmath>
#include <darkvoice/Doubler.hpp>
#include <limits>

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
double tap(const std::vector<float> &ring, std::size_t position, double delay) noexcept {
    const auto integer = static_cast<std::size_t>(delay);
    const double fraction = delay - integer;
    const auto first = position >= integer ? position - integer : position + ring.size() - integer;
    const auto second = first == 0 ? ring.size() - 1 : first - 1;
    return (1 - fraction) * ring[first] + fraction * ring[second];
}
} // namespace

double Doubler::Wander::random() noexcept {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return 2.0 * static_cast<double>(state) / std::numeric_limits<std::uint32_t>::max() - 1;
}
void Doubler::Wander::reset(std::uint32_t seed) noexcept {
    state = seed;
    from = random();
    to = random();
    phase = 0;
}
double Doubler::Wander::next(double step) noexcept {
    const double blend = phase * phase * (3 - 2 * phase);
    const double value = (1 - blend) * from + blend * to;
    phase += step;
    // The maximum step is 2*1.173/44100, so at most one knot is crossed.
    if (phase >= 1) {
        phase -= 1;
        from = to;
        to = random();
    }
    return value;
}
bool Doubler::valid(const DoublerParameters &p) noexcept {
    return range(p.separationMs, 10, 40) && range(p.variationMs, 0, 6) && range(p.rateHz, .1f, 2) &&
           range(p.mix, 0, .5f);
}
bool Doubler::prepare(double rate, std::size_t maxFrames, std::size_t channels) noexcept {
    rate_ = 0;
    if ((rate != 44100 && rate != 48000 && rate != 88200 && rate != 96000) || maxFrames == 0 ||
        maxFrames > 65536 || (channels != 1 && channels != 2))
        return false;
    try {
        const auto size = static_cast<std::size_t>(std::ceil(.060 * rate)) + 2;
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
bool Doubler::setParameters(const DoublerParameters &p) noexcept {
    if (!valid(p))
        return false;
    target_ = p;
    return true;
}
void Doubler::reset() noexcept {
    separation_ = target_.separationMs;
    variation_ = target_.variationMs;
    speed_ = target_.rateHz;
    mix_ = target_.mix;
    position_ = 0;
    wander_[0].reset(0x12345678U);
    wander_[1].reset(0x9e3779b9U);
    for (auto &ring : rings_)
        std::fill(ring.begin(), ring.end(), 0);
}
bool Doubler::process(std::span<float *> channels, std::size_t frames) noexcept {
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
    const auto smooth = [this](double &value, double goal) {
        value += smoothing_ * (goal - value);
        if (std::abs(value - goal) < 1e-7)
            value = goal;
    };
    const auto size = rings_[0].size();
    for (std::size_t i = 0; i < frames; ++i) {
        smooth(separation_, target_.separationMs);
        smooth(variation_, target_.variationMs);
        smooth(speed_, target_.rateHz);
        smooth(mix_, target_.mix);
        const double a =
            .001 * rate_ * (separation_ + variation_ * wander_[0].next(speed_ / rate_));
        const double b = .001 * rate_ *
                         (separation_ + 11 + variation_ * wander_[1].next(1.173 * speed_ / rate_));
        for (std::size_t c = 0; c < channels_; ++c) {
            auto &ring = rings_[c];
            const double first = tap(ring, position_, a), second = tap(ring, position_, b);
            const double firstWeight = channels_ == 1 ? .5 : c == 0 ? .75 : .25;
            const double wet = firstWeight * first + (1 - firstWeight) * second;
            const float input = sample(channels[c][i]);
            ring[position_] = input;
            channels[c][i] = mix_ == 0 ? input : sample((1 - mix_) * input + mix_ * wet);
        }
        position_ = position_ + 1 == size ? 0 : position_ + 1;
    }
    return true;
}
} // namespace darkvoice
