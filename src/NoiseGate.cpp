#include <algorithm>
#include <cmath>
#include <darkvoice/NoiseGate.hpp>

namespace darkvoice {
namespace {
bool range(float value, float minimum, float maximum) noexcept {
    return std::isfinite(value) && value >= minimum && value <= maximum;
}
float sample(float value) noexcept {
    return std::isfinite(value) && std::abs(value) >= 1.0e-20f ? value : 0.0f;
}
} // namespace
bool NoiseGate::valid(const NoiseGateParameters &p) noexcept {
    return range(p.thresholdDb, -80, 0) && range(p.ratio, 1, 20) && range(p.releaseMs, 10, 1000);
}
bool NoiseGate::prepare(double rate, std::size_t maxFrames, std::size_t channels) noexcept {
    rate_ = 0;
    if ((rate != 44100 && rate != 48000 && rate != 88200 && rate != 96000) || maxFrames == 0 ||
        maxFrames > 65536 || (channels != 1 && channels != 2))
        return false;
    rate_ = rate;
    maxFrames_ = maxFrames;
    channels_ = channels;
    smoothing_ = 1.0 - std::exp(-1.0 / (0.010 * rate));
    detectorRelease_ = std::exp(-1.0 / (0.020 * rate));
    opening_ = std::exp(-1.0 / (0.001 * rate));
    target_ = {};
    reset();
    return true;
}
bool NoiseGate::setParameters(const NoiseGateParameters &p) noexcept {
    if (!valid(p))
        return false;
    target_ = p;
    return true;
}
void NoiseGate::reset() noexcept {
    current_ = target_;
    envelope_ = gainDb_ = 0;
}
bool NoiseGate::process(std::span<float *> channels, std::size_t frames) noexcept {
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
    for (std::size_t i = 0; i < frames; ++i) {
        smooth(current_.thresholdDb, target_.thresholdDb);
        smooth(current_.ratio, target_.ratio);
        smooth(current_.releaseMs, target_.releaseMs);
        double peak = 0;
        for (auto *channel : channels)
            peak = std::max(peak, static_cast<double>(std::abs(sample(channel[i]))));
        envelope_ = std::max(peak, detectorRelease_ * envelope_);
        if (envelope_ < 1.0e-20)
            envelope_ = 0;
        const double level = envelope_ > 0 ? 20.0 * std::log10(envelope_) : -400.0;
        const double desired =
            std::clamp((current_.ratio - 1.0) * (level - current_.thresholdDb), -80.0, 0.0);
        const double coefficient =
            desired > gainDb_ ? opening_ : std::exp(-1.0 / (0.001 * current_.releaseMs * rate_));
        gainDb_ = desired + coefficient * (gainDb_ - desired);
        if (std::abs(gainDb_) < 1.0e-20)
            gainDb_ = 0;
        const double gain = std::pow(10.0, gainDb_ / 20.0);
        for (auto *channel : channels)
            channel[i] = sample(static_cast<float>(sample(channel[i]) * gain));
    }
    return true;
}
} // namespace darkvoice
