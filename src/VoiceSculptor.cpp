#include <darkvoice/VoiceSculptor.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace darkvoice {
namespace {
double cleanState(double x) noexcept {
    return !std::isfinite(x) || std::fpclassify(x) == FP_SUBNORMAL ? 0.0 : x;
}
double cleanInput(float x) noexcept {
    return !std::isfinite(x) || std::fpclassify(x) == FP_SUBNORMAL ? 0.0 : x;
}
struct Coefficients {
    double b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
};
struct Filter {
    double z1 = 0, z2 = 0;
    double process(double x, const Coefficients &c) noexcept {
        const double y = cleanState(c.b0 * x + z1);
        z1 = cleanState(c.b1 * x - c.a1 * y + z2);
        z2 = cleanState(c.b2 * x - c.a2 * y);
        return y;
    }
};
Coefficients bandpass(double rate, double frequency, double q) noexcept {
    const double w = 2 * std::numbers::pi * frequency / rate;
    const double alpha = std::sin(w) / (2 * q), denominator = 1 + alpha;
    return {alpha / denominator, 0, -alpha / denominator, -2 * std::cos(w) / denominator,
            (1 - alpha) / denominator};
}
} // namespace

struct VoiceSculptor::Impl {
    std::size_t maxFrames = 0, channelCount = 0;
    double smoothing = 0;
    std::array<Coefficients, 5> coefficients;
    std::array<std::array<Filter, 5>, 2> channels{};
    std::array<double, 6> weights{0, 1, 1, 0, 0, 0};
};

VoiceSculptor::VoiceSculptor() noexcept = default;
VoiceSculptor::~VoiceSculptor() = default;

bool VoiceSculptor::valid(const VoiceSculptorParameters &p) noexcept {
    const auto validGain = [](float x) { return std::isfinite(x) && x >= -6 && x <= 6; };
    return validGain(p.bodyDb) && validGain(p.warmthDb) && validGain(p.presenceDb) &&
           validGain(p.depthDb) && validGain(p.resonanceDb);
}

bool VoiceSculptor::prepare(double rate, std::size_t maxFrames, std::size_t channels) noexcept {
    impl_.reset();
    if ((rate != 44100 && rate != 48000 && rate != 88200 && rate != 96000) || maxFrames == 0 ||
        maxFrames > 65536 || (channels != 1 && channels != 2))
        return false;
    try {
        auto next = std::make_unique<Impl>();
        next->maxFrames = maxFrames;
        next->channelCount = channels;
        next->smoothing = 1 - std::exp(-1 / (.010 * rate));
        const double k = std::tan(std::numbers::pi * 450 / rate);
        const double depth = std::tan(std::numbers::pi * 90 / rate);
        next->coefficients = {
            bandpass(rate, 180, .85),
            Coefficients{k / (1 + k), k / (1 + k), 0, (k - 1) / (k + 1), 0},
            bandpass(rate, 3000, .7),
            Coefficients{depth / (1 + depth), depth / (1 + depth), 0, (depth - 1) / (depth + 1), 0},
            bandpass(rate, 700, 1.6)};
        impl_ = std::move(next);
        targets_ = {0, 1, 1, 0, 0, 0};
        reset();
        return true;
    } catch (...) {
        return false;
    }
}

bool VoiceSculptor::setParameters(const VoiceSculptorParameters &p) noexcept {
    if (!valid(p))
        return false;
    targets_ = {std::pow(10.0, p.bodyDb / 20.0) - 1,  std::pow(10.0, p.warmthDb / 40.0),
                std::pow(10.0, -p.warmthDb / 40.0),   std::pow(10.0, p.presenceDb / 20.0) - 1,
                std::pow(10.0, p.depthDb / 20.0) - 1, std::pow(10.0, p.resonanceDb / 20.0) - 1};
    return true;
}

void VoiceSculptor::reset() noexcept {
    if (!impl_)
        return;
    impl_->channels = {};
    impl_->weights = targets_;
}

bool VoiceSculptor::process(std::span<float *> channels, std::size_t frames) noexcept {
    if (!impl_ || channels.size() != impl_->channelCount || frames > impl_->maxFrames)
        return false;
    for (std::size_t c = 0; c < channels.size(); ++c) {
        if (frames != 0 && channels[c] == nullptr)
            return false;
        for (std::size_t other = 0; other < c; ++other)
            if (channels[c] == channels[other])
                return false;
    }
    constexpr double limit = std::numeric_limits<float>::max();
    auto &p = *impl_;
    for (std::size_t i = 0; i < frames; ++i) {
        for (std::size_t w = 0; w < p.weights.size(); ++w)
            p.weights[w] = cleanState(p.weights[w] + p.smoothing * (targets_[w] - p.weights[w]));
        for (std::size_t c = 0; c < channels.size(); ++c) {
            auto &filters = p.channels[c];
            double x = cleanInput(channels[c][i]);
            const double depth = filters[3].process(x, p.coefficients[3]);
            if (p.weights[4] != 0)
                x += p.weights[4] * depth;
            const double body = filters[0].process(x, p.coefficients[0]);
            if (p.weights[0] != 0)
                x += p.weights[0] * body;
            const double low = filters[1].process(x, p.coefficients[1]);
            // Explicit neutral paths preserve the sign of input zero while
            // every fixed filter continues tracking its own stage input.
            if (p.weights[1] != 1 || p.weights[2] != 1)
                x = p.weights[2] * x + (p.weights[1] - p.weights[2]) * low;
            const double resonance = filters[4].process(x, p.coefficients[4]);
            if (p.weights[5] != 0)
                x += p.weights[5] * resonance;
            const double presence = filters[2].process(x, p.coefficients[2]);
            if (p.weights[3] != 0)
                x += p.weights[3] * presence;
            const float output =
                std::isfinite(x) ? static_cast<float>(std::clamp(x, -limit, limit)) : 0.0f;
            // A cut can turn a normal input into a float subnormal even though
            // all double histories are normal. Preserve signed zero and the
            // normal boundary while flushing only actual emitted subnormals.
            channels[c][i] = std::fpclassify(output) == FP_SUBNORMAL ? 0.0f : output;
        }
    }
    return true;
}
} // namespace darkvoice
