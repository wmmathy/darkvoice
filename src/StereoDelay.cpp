#include <algorithm>
#include <cmath>
#include <darkvoice/StereoDelay.hpp>
#include <limits>
namespace darkvoice {
namespace {
bool range(float value, float low, float high) noexcept {
    return std::isfinite(value) && value >= low && value <= high;
}
float sample(double value) noexcept {
    if (!std::isfinite(value) || std::abs(value) < std::numeric_limits<float>::min())
        return 0;
    constexpr double maximum = std::numeric_limits<float>::max();
    return static_cast<float>(std::clamp(value, -maximum, maximum));
}
} // namespace
bool StereoDelay::valid(const StereoDelayParameters &p) noexcept {
    return range(p.timeMs, 20, 2000) && range(p.feedback, 0, .8f) && range(p.mix, 0, .5f) &&
           range(p.bpm, 20, 300) && p.mode >= 0 && p.mode <= 2 && p.division >= 0 &&
           p.division <= 5;
}
bool StereoDelay::prepare(double rate, std::size_t maxFrames, std::size_t channels) noexcept {
    rate_ = 0;
    if ((rate != 44100 && rate != 48000 && rate != 88200 && rate != 96000) || maxFrames == 0 ||
        maxFrames > 65536 || (channels != 1 && channels != 2))
        return false;
    try {
        const auto size = static_cast<std::size_t>(3 * rate) + 2;
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
bool StereoDelay::setParameters(const StereoDelayParameters &p) noexcept {
    if (!valid(p))
        return false;
    target_ = p;
    return true;
}
double StereoDelay::delaySamples() const noexcept {
    constexpr std::array<double, 6> beats{.25, 1. / 3, .5, 2. / 3, .75, 1};
    const double seconds =
        target_.sync ? 60.0 / target_.bpm * beats[target_.division] : .001 * target_.timeMs;
    return std::min(3.0, seconds) * rate_;
}
void StereoDelay::reset() noexcept {
    delay_ = delaySamples();
    feedback_ = target_.mode == 1 ? 0 : target_.feedback;
    mix_ = target_.mix;
    cross_ = target_.mode == 2 && channels_ == 2 ? 1 : 0;
    position_ = 0;
    for (auto &ring : rings_)
        std::fill(ring.begin(), ring.end(), 0);
}
bool StereoDelay::process(std::span<float *> channels, std::size_t frames) noexcept {
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
    const double targetDelay = delaySamples();
    const double targetFeedback = target_.mode == 1 ? 0 : target_.feedback;
    const double targetCross = target_.mode == 2 && channels_ == 2 ? 1 : 0;
    const auto smooth = [this](double &value, double goal, double tolerance) {
        value += smoothing_ * (goal - value);
        if (std::abs(value - goal) < tolerance)
            value = goal;
    };
    const auto size = rings_[0].size();
    for (std::size_t i = 0; i < frames; ++i) {
        smooth(delay_, targetDelay, 1e-5);
        smooth(feedback_, targetFeedback, 1e-7);
        smooth(mix_, target_.mix, 1e-7);
        smooth(cross_, targetCross, 1e-7);
        // Separate integer delay and fractional weight before wrapping. Adding
        // a nearly-zero negative location to the ring size can round to size
        // itself and read out of bounds at an almost-integer synchronized delay.
        const auto integerDelay = static_cast<std::size_t>(delay_);
        const double fraction = delay_ - integerDelay;
        const auto first =
            position_ >= integerDelay ? position_ - integerDelay : position_ + size - integerDelay;
        const auto second = first == 0 ? size - 1 : first - 1;
        std::array<double, 2> tap{}, input{};
        for (std::size_t c = 0; c < channels_; ++c) {
            const auto &ring = rings_[c];
            tap[c] = ring[first] + fraction * (static_cast<double>(ring[second]) - ring[first]);
            input[c] = sample(channels[c][i]);
        }
        for (std::size_t c = 0; c < channels_; ++c) {
            const double routed =
                channels_ == 2 ? tap[c] * (1 - cross_) + tap[1 - c] * cross_ : tap[c];
            rings_[c][position_] = sample(input[c] + feedback_ * routed);
            channels[c][i] = mix_ == 0 ? static_cast<float>(input[c])
                                       : sample(input[c] * (1 - mix_) + tap[c] * mix_);
        }
        position_ = position_ + 1 == size ? 0 : position_ + 1;
    }
    return true;
}
} // namespace darkvoice
