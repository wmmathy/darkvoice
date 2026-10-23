#include <darkvoice/ToneEffects.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <vector>

namespace darkvoice {
namespace {
double clean(double x) noexcept {
    constexpr double limit = std::numeric_limits<float>::max();
    if (!std::isfinite(x) || std::abs(x) < 1.0e-20)
        return 0.0;
    return std::clamp(x, -limit, limit);
}
bool range(float value, float lo, float hi) noexcept {
    return std::isfinite(value) && value >= lo && value <= hi;
}
struct Coefficients {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
};
struct Biquad {
    double z1 = 0, z2 = 0;
    double process(double input, const Coefficients &c) noexcept {
        const double output = clean(c.b0 * input + z1);
        z1 = clean(c.b1 * input - c.a1 * output + z2);
        z2 = clean(c.b2 * input - c.a2 * output);
        return output;
    }
};
// Robert Bristow-Johnson cookbook biquads: fixed shelf slope S=1, peak Q.
Coefficients coefficients(double rate, double hz, double gainDb, double q, int kind) noexcept {
    // A zero-gain band is exactly neutral. Avoid pole/zero cancellation whose
    // rounding depends on the platform's floating-point contraction rules.
    if (gainDb == 0.0)
        return {};
    const double a = std::pow(10.0, gainDb / 40.0);
    const double omega = 2.0 * std::numbers::pi * hz / rate;
    const double cosine = std::cos(omega), sine = std::sin(omega);
    double b0, b1, b2, a0, a1, a2;
    if (kind == 0) {
        const double alpha = sine / (2 * q);
        b0 = 1 + alpha * a;
        b1 = -2 * cosine;
        b2 = 1 - alpha * a;
        a0 = 1 + alpha / a;
        a1 = -2 * cosine;
        a2 = 1 - alpha / a;
    } else {
        const double beta = std::sqrt(2.0 * a) * sine;
        if (kind < 0) {
            b0 = a * ((a + 1) - (a - 1) * cosine + beta);
            b1 = 2 * a * ((a - 1) - (a + 1) * cosine);
            b2 = a * ((a + 1) - (a - 1) * cosine - beta);
            a0 = (a + 1) + (a - 1) * cosine + beta;
            a1 = -2 * ((a - 1) + (a + 1) * cosine);
            a2 = (a + 1) + (a - 1) * cosine - beta;
        } else {
            b0 = a * ((a + 1) + (a - 1) * cosine + beta);
            b1 = -2 * a * ((a - 1) + (a + 1) * cosine);
            b2 = a * ((a + 1) + (a - 1) * cosine - beta);
            a0 = (a + 1) - (a - 1) * cosine + beta;
            a1 = 2 * ((a - 1) - (a + 1) * cosine);
            a2 = (a + 1) - (a - 1) * cosine - beta;
        }
    }
    return {b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
}
struct Ring {
    std::vector<float> samples;
    std::size_t position = 0;
    void prepare(std::size_t size) {
        samples.assign(size, 0.0f);
        position = 0;
    }
    void reset() noexcept {
        std::fill(samples.begin(), samples.end(), 0.0f);
        position = 0;
    }
    double read(double delay) const noexcept {
        // Keep fractional delay separate: adding a negative epsilon to the
        // ring length can round to length and produce an invalid index.
        const auto whole = static_cast<std::size_t>(delay);
        const double fraction = delay - whole;
        const auto first = position >= whole ? position - whole : position + samples.size() - whole;
        const auto second = first == 0 ? samples.size() - 1 : first - 1;
        return samples[first] + fraction * (static_cast<double>(samples[second]) - samples[first]);
    }
    void write(double input) noexcept {
        samples[position] = static_cast<float>(clean(input));
        position = (position + 1) % samples.size();
    }
};
struct Comb {
    Ring ring;
    double damped = 0;
    double process(double input, double delay, double feedback, double damping) noexcept {
        const double output = ring.read(delay);
        damped = clean((1.0 - damping) * output + damping * damped);
        ring.write(input + feedback * damped);
        return output;
    }
    void reset() noexcept {
        ring.reset();
        damped = 0;
    }
};
struct Channel {
    std::array<Biquad, 3> filters;
    Ring delay;
    std::array<Comb, 4> combs;
    std::array<Ring, 2> allpasses;
    void reset() noexcept {
        filters = {};
        delay.reset();
        for (auto &comb : combs)
            comb.reset();
        for (auto &allpass : allpasses)
            allpass.reset();
    }
};
constexpr std::array<double, 4> combSeconds{0.0297, 0.0371, 0.0411, 0.0437};
constexpr std::array<double, 2> allpassSeconds{0.0050, 0.0017};
} // namespace

struct ToneEffects::Impl {
    double rate = 0;
    std::size_t maxFrames = 0, channelCount = 0;
    ToneEffectsParameters current{};
    std::array<Channel, 2> channels;
};

ToneEffects::ToneEffects() noexcept = default;
ToneEffects::~ToneEffects() = default;

bool ToneEffects::valid(const ToneEffectsParameters &p) noexcept {
    return range(p.eqLowHz, 60, 600) && range(p.eqLowGainDb, -12, 12) &&
           range(p.eqMidHz, 200, 5000) && range(p.eqMidGainDb, -12, 12) &&
           range(p.eqMidQ, 0.2f, 6) && range(p.eqHighHz, 1500, 12000) &&
           range(p.eqHighGainDb, -12, 12) && range(p.delayMs, 20, 1000) &&
           range(p.delayFeedback, 0, 0.8f) && range(p.delayMix, 0, 0.5f) &&
           range(p.reverbSize, 0, 1) && range(p.reverbDamping, 0, 1) && range(p.reverbMix, 0, 0.5f);
}

bool ToneEffects::prepare(double rate, std::size_t maxFrames, std::size_t channels) noexcept {
    impl_.reset();
    if ((rate != 44100 && rate != 48000 && rate != 88200 && rate != 96000) || maxFrames == 0 ||
        maxFrames > 65536 || (channels != 1 && channels != 2))
        return false;
    try {
        auto next = std::make_unique<Impl>();
        next->rate = rate;
        next->maxFrames = maxFrames;
        next->channelCount = channels;
        for (std::size_t c = 0; c < channels; ++c) {
            auto &channel = next->channels[c];
            channel.delay.prepare(static_cast<std::size_t>(std::ceil(rate)) + 2);
            for (std::size_t i = 0; i < channel.combs.size(); ++i)
                channel.combs[i].ring.prepare(
                    static_cast<std::size_t>(std::ceil(rate * combSeconds[i] * 1.4)) + 2);
            for (std::size_t i = 0; i < channel.allpasses.size(); ++i)
                channel.allpasses[i].prepare(
                    static_cast<std::size_t>(std::ceil(rate * allpassSeconds[i] * 1.4)) + 2);
        }
        impl_ = std::move(next);
        target_ = {};
        reset();
        return true;
    } catch (...) {
        return false;
    }
}

bool ToneEffects::setParameters(const ToneEffectsParameters &parameters) noexcept {
    if (!valid(parameters))
        return false;
    target_ = parameters;
    return true;
}

void ToneEffects::reset() noexcept {
    if (!impl_)
        return;
    impl_->current = target_;
    for (std::size_t c = 0; c < impl_->channelCount; ++c)
        impl_->channels[c].reset();
}

bool ToneEffects::process(std::span<float *> channels, std::size_t frames) noexcept {
    if (!impl_ || channels.size() != impl_->channelCount || frames > impl_->maxFrames)
        return false;
    for (std::size_t c = 0; c < channels.size(); ++c) {
        if (frames != 0 && channels[c] == nullptr)
            return false;
        for (std::size_t other = 0; other < c; ++other)
            if (channels[c] == channels[other])
                return false;
    }
    const double smoothing = 1.0 - std::exp(-1.0 / (0.010 * impl_->rate));
    const auto smooth = [smoothing](float &current, float target) noexcept {
        current = static_cast<float>(current + smoothing * (target - current));
    };
    auto &p = impl_->current;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        smooth(p.eqLowHz, target_.eqLowHz);
        smooth(p.eqLowGainDb, target_.eqLowGainDb);
        smooth(p.eqMidHz, target_.eqMidHz);
        smooth(p.eqMidGainDb, target_.eqMidGainDb);
        smooth(p.eqMidQ, target_.eqMidQ);
        smooth(p.eqHighHz, target_.eqHighHz);
        smooth(p.eqHighGainDb, target_.eqHighGainDb);
        smooth(p.delayMs, target_.delayMs);
        smooth(p.delayFeedback, target_.delayFeedback);
        smooth(p.delayMix, target_.delayMix);
        smooth(p.reverbSize, target_.reverbSize);
        smooth(p.reverbDamping, target_.reverbDamping);
        smooth(p.reverbMix, target_.reverbMix);
        const std::array<Coefficients, 3> eq{
            coefficients(impl_->rate, p.eqLowHz, p.eqLowGainDb, 0.707, -1),
            coefficients(impl_->rate, p.eqMidHz, p.eqMidGainDb, p.eqMidQ, 0),
            coefficients(impl_->rate, p.eqHighHz, p.eqHighGainDb, 0.707, 1)};
        const double delaySamples = impl_->rate * p.delayMs * 0.001;
        const double size = 0.6 + 0.8 * p.reverbSize;
        const double feedback = 0.55 + 0.30 * p.reverbSize;
        const double damping = 0.05 + 0.90 * p.reverbDamping;
        for (std::size_t c = 0; c < channels.size(); ++c) {
            auto &channel = impl_->channels[c];
            double signal = clean(channels[c][frame]);
            for (std::size_t filter = 0; filter < eq.size(); ++filter)
                signal = channel.filters[filter].process(signal, eq[filter]);
            const double delayed = channel.delay.read(delaySamples);
            channel.delay.write(signal + p.delayFeedback * delayed);
            signal = clean(signal * (1.0 - p.delayMix) + delayed * p.delayMix);
            double reverb = 0;
            for (std::size_t i = 0; i < channel.combs.size(); ++i)
                reverb += channel.combs[i].process(signal, impl_->rate * combSeconds[i] * size,
                                                   feedback, damping) *
                          0.25;
            for (std::size_t i = 0; i < channel.allpasses.size(); ++i) {
                auto &allpass = channel.allpasses[i];
                const double buffered = allpass.read(impl_->rate * allpassSeconds[i] * size);
                const double output = clean(buffered - 0.5 * reverb);
                allpass.write(reverb + 0.5 * output);
                reverb = output;
            }
            channels[c][frame] =
                static_cast<float>(clean(signal * (1.0 - p.reverbMix) + reverb * p.reverbMix));
        }
    }
    return true;
}
} // namespace darkvoice
