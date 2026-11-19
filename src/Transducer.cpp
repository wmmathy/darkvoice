#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <darkvoice/Transducer.hpp>
#include <limits>
#include <numbers>

namespace darkvoice {
namespace {
double clean(double x) noexcept {
    return !std::isfinite(x) || std::fpclassify(x) == FP_SUBNORMAL ? 0 : x;
}
float bounded(double x) noexcept {
    constexpr double limit = std::numeric_limits<float>::max();
    const float result = static_cast<float>(std::clamp(clean(x), -limit, limit));
    return std::fpclassify(result) == FP_SUBNORMAL ? 0 : result;
}
struct Section {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    std::array<double, 2> z1{}, z2{};
    double tick(double x, std::size_t channel) noexcept {
        const double y = clean(b0 * x + z1[channel]);
        z1[channel] = clean(b1 * x - a1 * y + z2[channel]);
        z2[channel] = clean(b2 * x - a2 * y);
        return y;
    }
};
Section filter(double rate, double hz, double q, int kind, double db = 0) noexcept {
    const double w = 2 * std::numbers::pi * hz / rate;
    const double c = std::cos(w), alpha = std::sin(w) / (2 * q);
    Section s;
    double a0 = 1 + alpha;
    if (kind == 2) {
        const double gain = std::pow(10., db / 40);
        a0 = 1 + alpha / gain;
        s.b0 = 1 + alpha * gain;
        s.b1 = -2 * c;
        s.b2 = 1 - alpha * gain;
        s.a1 = -2 * c;
        s.a2 = 1 - alpha / gain;
    } else {
        s.b0 = (1 + (kind == 1 ? c : -c)) * .5;
        s.b1 = (kind == 1 ? -2 : 2) * s.b0;
        s.b2 = s.b0;
        s.a1 = -2 * c;
        s.a2 = 1 - alpha;
    }
    s.b0 /= a0;
    s.b1 /= a0;
    s.b2 /= a0;
    s.a1 /= a0;
    s.a2 /= a0;
    return s;
}
struct Ramp {
    double current = 0, target = 0, step = 0;
    std::size_t left = 0;
    void set(double next, std::size_t count) noexcept {
        target = next;
        left = count;
        step = (target - current) / static_cast<double>(count);
    }
    void settle(double next) noexcept {
        current = target = next;
        step = 0;
        left = 0;
    }
    double tick() noexcept {
        if (left && --left == 0)
            current = target;
        else if (left)
            current += step;
        return current;
    }
};
} // namespace
struct Transducer::Impl {
    std::array<std::array<Section, 4>, 3> networks{};
    std::array<Ramp, 3> weights{};
    Ramp mix;
    std::size_t frames = 0, channels = 0, mixSamples = 1, modelSamples = 1;
};
Transducer::Transducer() noexcept = default;
Transducer::~Transducer() = default;
bool Transducer::valid(const TransducerParameters &p) noexcept {
    return p.model >= 0 && p.model <= 2 && std::isfinite(p.mix) && p.mix >= 0 && p.mix <= 1;
}
bool Transducer::prepare(double rate, std::size_t frames, std::size_t channels) noexcept {
    impl_.reset();
    if ((rate != 44100 && rate != 48000 && rate != 88200 && rate != 96000) || frames == 0 ||
        frames > 65536 || (channels != 1 && channels != 2))
        return false;
    try {
        auto next = std::make_unique<Impl>();
        next->frames = frames;
        next->channels = channels;
        next->mixSamples = std::max<std::size_t>(1, std::llround(rate * .010));
        next->modelSamples = std::max<std::size_t>(1, std::llround(rate * .020));
        constexpr double q1 = .541196100146197, q2 = 1.30656296487638;
        const double q = std::sqrt(.5);
        next->networks[0] = {filter(rate, 300, q1, 1), filter(rate, 300, q2, 1),
                             filter(rate, 3400, q1, 0), filter(rate, 3400, q2, 0)};
        next->networks[1] = {filter(rate, 450, q, 1), filter(rate, 1600, 1.2, 2, 6),
                             filter(rate, 3200, 2, 2, 3), filter(rate, 5500, q, 0)};
        next->networks[2] = {filter(rate, 120, q, 1), filter(rate, 1200, .6, 2, 3),
                             filter(rate, 4500, q1, 0), filter(rate, 4500, q2, 0)};
        impl_ = std::move(next);
        target_ = {};
        reset();
        return true;
    } catch (...) {
        return false;
    }
}
bool Transducer::setParameters(const TransducerParameters &p) noexcept {
    if (!valid(p))
        return false;
    if (impl_) {
        if (p.mix != target_.mix)
            impl_->mix.set(p.mix, impl_->mixSamples);
        if (p.model != target_.model)
            for (std::size_t i = 0; i < 3; ++i)
                impl_->weights[i].set(i == static_cast<std::size_t>(p.model) ? 1 : 0,
                                      impl_->modelSamples);
    }
    target_ = p;
    return true;
}
void Transducer::reset() noexcept {
    if (!impl_)
        return;
    for (auto &network : impl_->networks)
        for (auto &s : network)
            s.z1 = s.z2 = {};
    impl_->mix.settle(target_.mix);
    for (std::size_t i = 0; i < 3; ++i)
        impl_->weights[i].settle(i == static_cast<std::size_t>(target_.model) ? 1 : 0);
}
bool Transducer::process(std::span<float *> channels, std::size_t frames) noexcept {
    if (!impl_ || channels.size() != impl_->channels || frames > impl_->frames)
        return false;
    if (frames == 0)
        return true;
    const auto bytes = frames * sizeof(float);
    for (std::size_t c = 0; c < channels.size(); ++c) {
        if (!channels[c])
            return false;
        const auto address = reinterpret_cast<std::uintptr_t>(channels[c]);
        if (address > std::numeric_limits<std::uintptr_t>::max() - bytes)
            return false;
        for (std::size_t j = 0; j < c; ++j) {
            const auto other = reinterpret_cast<std::uintptr_t>(channels[j]);
            if (address < other + bytes && other < address + bytes)
                return false;
        }
    }
    auto &p = *impl_;
    constexpr double megaphoneTrim = .5011872336272722;
    for (std::size_t n = 0; n < frames; ++n) {
        const double mix = p.mix.tick();
        std::array<double, 3> weights{};
        double sum = 0;
        for (std::size_t m = 0; m < 3; ++m) {
            weights[m] = std::clamp(p.weights[m].tick(), 0., 1.);
            sum += weights[m];
        }
        // Normalize rounding residuals, keeping a convex model blend during
        // interrupted ramps without ever interpolating filter coefficients.
        for (auto &weight : weights)
            weight /= sum;
        for (std::size_t c = 0; c < channels.size(); ++c) {
            const float original = channels[c][n];
            const double dry = !std::isfinite(original) || std::fpclassify(original) == FP_SUBNORMAL
                                   ? 0
                                   : original;
            double wet = 0;
            for (std::size_t m = 0; m < 3; ++m) {
                double value = dry;
                for (auto &section : p.networks[m])
                    value = section.tick(value, c);
                wet += weights[m] * value * (m == 1 ? megaphoneTrim : 1);
            }
            channels[c][n] =
                mix == 0 && std::isfinite(original) && std::fpclassify(original) != FP_SUBNORMAL
                    ? original
                    : bounded(dry + (wet - dry) * mix);
        }
    }
    return true;
}
} // namespace darkvoice
