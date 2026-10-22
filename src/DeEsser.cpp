#include <algorithm>
#include <cmath>
#include <darkvoice/DeEsser.hpp>
#include <limits>
#include <numbers>

namespace darkvoice {
namespace {
bool range(float value, float minimum, float maximum) noexcept {
    return std::isfinite(value) && value >= minimum && value <= maximum;
}
double sample(double value) noexcept {
    return std::isfinite(value) && std::abs(value) >= std::numeric_limits<float>::min() ? value
                                                                                        : 0.0;
}
double clean(double value) noexcept {
    return std::isfinite(value) && std::abs(value) >= 1.0e-20 ? value : 0.0;
}
} // namespace
bool DeEsser::valid(const DeEsserParameters &p) noexcept {
    return range(p.frequencyHz, 2000, 12000) && range(p.thresholdDb, -60, 0) &&
           range(p.maxReductionDb, 0, 18);
}
bool DeEsser::prepare(double rate, std::size_t maxFrames, std::size_t channels) noexcept {
    rate_ = 0;
    if ((rate != 44100 && rate != 48000 && rate != 88200 && rate != 96000) || maxFrames == 0 ||
        maxFrames > 65536 || (channels != 1 && channels != 2))
        return false;
    rate_ = rate;
    maxFrames_ = maxFrames;
    channels_ = channels;
    smoothing_ = 1.0 - std::exp(-1.0 / (0.010 * rate));
    detectorAttack_ = std::exp(-1.0 / (0.0003 * rate));
    release_ = std::exp(-1.0 / (0.050 * rate));
    gainAttack_ = std::exp(-1.0 / (0.001 * rate));
    target_ = {};
    reset();
    return true;
}
bool DeEsser::setParameters(const DeEsserParameters &p) noexcept {
    if (!valid(p))
        return false;
    target_ = p;
    return true;
}
void DeEsser::reset() noexcept {
    current_ = target_;
    low_ = {};
    envelope_ = gainDb_ = 0;
}
bool DeEsser::process(std::span<float *> channels, std::size_t frames) noexcept {
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
        smooth(current_.frequencyHz, target_.frequencyHz);
        smooth(current_.thresholdDb, target_.thresholdDb);
        smooth(current_.maxReductionDb, target_.maxReductionDb);
        const double coefficient = std::exp(-2.0 * std::numbers::pi * current_.frequencyHz / rate_);
        std::array<double, 2> input{}, high{};
        double peak = 0;
        for (std::size_t c = 0; c < channels_; ++c) {
            input[c] = sample(channels[c][i]);
            low_[c] = clean((1.0 - coefficient) * input[c] + coefficient * low_[c]);
            high[c] = input[c] - low_[c];
            peak = std::max(peak, std::abs(high[c]));
        }
        const double detector = peak > envelope_ ? detectorAttack_ : release_;
        envelope_ = clean(peak + detector * (envelope_ - peak));
        const double level = envelope_ > 0 ? 20.0 * std::log10(envelope_) : -400.0;
        const double desired = -std::clamp(level - current_.thresholdDb, 0.0,
                                           static_cast<double>(current_.maxReductionDb));
        const double gainCoefficient = desired < gainDb_ ? gainAttack_ : release_;
        gainDb_ = clean(desired + gainCoefficient * (gainDb_ - desired));
        if (std::abs(gainDb_) < 1.0e-6)
            gainDb_ = 0;
        const double gain = std::pow(10.0, gainDb_ / 20.0);
        constexpr double limit = std::numeric_limits<float>::max();
        for (std::size_t c = 0; c < channels_; ++c)
            channels[c][i] = static_cast<float>(
                std::clamp(sample(input[c] + (gain - 1.0) * high[c]), -limit, limit));
    }
    return true;
}
} // namespace darkvoice
