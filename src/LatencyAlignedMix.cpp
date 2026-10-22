#include <algorithm>
#include <cmath>
#include <darkvoice/LatencyAlignedMix.hpp>
#include <limits>
namespace darkvoice {
namespace {
float sample(double value) noexcept {
    if (!std::isfinite(value) || std::abs(value) < std::numeric_limits<float>::min())
        return 0;
    constexpr double limit = std::numeric_limits<float>::max();
    return static_cast<float>(std::clamp(value, -limit, limit));
}
} // namespace
bool LatencyAlignedMix::prepare(double rate, std::size_t maxFrames, std::size_t channels,
                                std::size_t latencySamples) noexcept {
    channels_ = 0;
    if ((rate != 44100 && rate != 48000 && rate != 88200 && rate != 96000) || maxFrames == 0 ||
        maxFrames > 65536 || (channels != 1 && channels != 2) || latencySamples > 65536)
        return false;
    try {
        for (std::size_t c = 0; c < channels; ++c)
            rings_[c].assign(latencySamples, 0);
    } catch (...) {
        return false;
    }
    maxFrames_ = maxFrames;
    channels_ = channels;
    latency_ = latencySamples;
    smoothing_ = 1 - std::exp(-1 / (.010 * rate));
    target_ = {};
    reset();
    return true;
}
bool LatencyAlignedMix::setParameters(const AlignedMixParameters &p) noexcept {
    if (!std::isfinite(p.mix) || p.mix < 0 || p.mix > 1)
        return false;
    target_ = p;
    return true;
}
void LatencyAlignedMix::reset() noexcept {
    wet_ = target_.bypass ? 0 : target_.mix;
    position_ = 0;
    for (auto &ring : rings_)
        std::fill(ring.begin(), ring.end(), 0);
}
std::size_t LatencyAlignedMix::latencySamples() const noexcept {
    return channels_ ? latency_ : 0;
}
bool LatencyAlignedMix::process(std::span<const float *> dry, std::span<float *> wet,
                                std::size_t frames) noexcept {
    if (channels_ == 0 || dry.size() != channels_ || wet.size() != channels_ || frames > maxFrames_)
        return false;
    for (std::size_t c = 0; c < channels_; ++c) {
        if (frames && (!dry[c] || !wet[c]))
            return false;
        for (std::size_t other = 0; other < c; ++other)
            if (dry[c] == dry[other] || wet[c] == wet[other] || dry[c] == wet[other] ||
                wet[c] == dry[other])
                return false;
    }
    const double goal = target_.bypass ? 0 : target_.mix;
    for (std::size_t i = 0; i < frames; ++i) {
        std::array<float, 2> delayed{}, processed{};
        for (std::size_t c = 0; c < channels_; ++c) {
            const auto input = sample(dry[c][i]);
            delayed[c] = latency_ ? rings_[c][position_] : input;
            if (latency_)
                rings_[c][position_] = input;
            processed[c] = sample(wet[c][i]);
        }
        wet_ += smoothing_ * (goal - wet_);
        if (std::abs(wet_ - goal) < 1e-6)
            wet_ = goal;
        for (std::size_t c = 0; c < channels_; ++c)
            wet[c][i] = wet_ == 0   ? delayed[c]
                        : wet_ == 1 ? processed[c]
                                    : sample(delayed[c] * (1 - wet_) + processed[c] * wet_);
        if (latency_ && ++position_ == latency_)
            position_ = 0;
    }
    return true;
}
} // namespace darkvoice
