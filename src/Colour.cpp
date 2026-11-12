#include "ColourOversampling.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <darkvoice/Colour.hpp>
#include <limits>
#include <numbers>
namespace darkvoice {
namespace {
bool range(float x, float low, float high) noexcept {
    return std::isfinite(x) && x >= low && x <= high;
}
double input(float x) noexcept {
    return std::isfinite(x) && std::abs(x) >= std::numeric_limits<float>::min() ? x : 0;
}
double state(double x) noexcept {
    return std::isfinite(x) && std::abs(x) >= std::numeric_limits<double>::min() ? x : 0;
}
float sample(double x) noexcept {
    if (!std::isfinite(x) || std::abs(x) < std::numeric_limits<float>::min())
        return 0;
    constexpr double limit = std::numeric_limits<float>::max();
    return static_cast<float>(std::clamp(x, -limit, limit));
}
struct Channel {
    colour_detail::Oversampler8 resampler;
    std::array<double, 56> dry{};
    std::size_t position = 0;
    double pre = 0, sPrevious = 0, yPrevious = 0, dcPrevious = 0, dcState = 0, toneState = 0;
    void reset() noexcept {
        resampler.reset();
        dry = {};
        position = 0;
        pre = sPrevious = yPrevious = dcPrevious = dcState = toneState = 0;
    }
};
} // namespace
struct Colour::Impl {
    double rate = 0, smooth = 0, modelSmooth = 0, prePole = 0, dcPole = 0;
    std::size_t maxFrames = 0, channels = 0;
    double drive = .25, toneHz = 16000, mix = 0, style = 0;
    std::array<Channel, 2> channel;
};
Colour::Colour() noexcept = default;
Colour::~Colour() = default;
bool Colour::valid(const ColourParameters &p) noexcept {
    return p.model >= 0 && p.model <= 1 && range(p.drive, 0, 1) && range(p.toneHz, 1000, 20000) &&
           range(p.mix, 0, 1);
}
bool Colour::prepare(double rate, std::size_t maxFrames, std::size_t channels) noexcept {
    impl_.reset();
    if ((rate != 44100 && rate != 48000 && rate != 88200 && rate != 96000) || maxFrames == 0 ||
        maxFrames > 65536 || (channels != 1 && channels != 2))
        return false;
    try {
        auto next = std::make_unique<Impl>();
        next->rate = rate;
        next->maxFrames = maxFrames;
        next->channels = channels;
        next->smooth = 1 - std::exp(-1 / (.010 * rate));
        next->modelSmooth = 1 - std::exp(-1 / (.020 * rate));
        next->prePole = std::exp(-2 * std::numbers::pi * 1500 / rate);
        next->dcPole = std::exp(-2 * std::numbers::pi * 10 / rate);
        const auto h = colour_detail::kernel();
        for (std::size_t i = 0; i < channels; ++i)
            next->channel[i].resampler.prepare(h);
        impl_ = std::move(next);
        target_ = {};
        reset();
        return true;
    } catch (...) {
        return false;
    }
}
bool Colour::setParameters(const ColourParameters &p) noexcept {
    if (!valid(p))
        return false;
    target_ = p;
    return true;
}
void Colour::reset() noexcept {
    if (!impl_)
        return;
    impl_->drive = target_.drive;
    impl_->toneHz = target_.toneHz;
    impl_->mix = target_.mix;
    impl_->style = target_.model;
    for (auto &channel : impl_->channel)
        channel.reset();
}
std::size_t Colour::latencySamples() const noexcept {
    return impl_ ? 56 : 0;
}
bool Colour::process(std::span<float *> channels, std::size_t frames) noexcept {
    if (!impl_ || channels.size() != impl_->channels || frames > impl_->maxFrames)
        return false;
    if (frames == 0)
        return true;
    for (std::size_t c = 0; c < channels.size(); ++c) {
        if (!channels[c])
            return false;
        for (std::size_t other = 0; other < c; ++other)
            if (channels[c] == channels[other])
                return false;
    }
    auto &p = *impl_;
    const auto smooth = [](double &x, double target, double coefficient) {
        x += coefficient * (target - x);
        if (std::abs(x - target) < 1e-7)
            x = target;
    };
    for (std::size_t i = 0; i < frames; ++i) {
        smooth(p.drive, target_.drive, p.smooth);
        smooth(p.toneHz, target_.toneHz, p.smooth);
        smooth(p.mix, target_.mix, p.smooth);
        smooth(p.style, target_.model, p.modelSmooth);
        const double drive = 1 + 5 * p.drive, k = .5 * (1 - p.style);
        const double normalization = std::tanh(drive), tubeOffset = std::tanh(.12 * drive);
        const double tonePole = std::exp(-2 * std::numbers::pi * p.toneHz / p.rate);
        for (std::size_t c = 0; c < channels.size(); ++c) {
            auto &s = p.channel[c];
            const double dry = input(channels[c][i]);
            const double delayed = s.dry[s.position];
            s.dry[s.position] = dry;
            s.position = s.position + 1 == s.dry.size() ? 0 : s.position + 1;
            s.pre = state(p.prePole * s.pre + (1 - p.prePole) * dry);
            const double v = dry + k * (dry - s.pre);
            const double shaped = s.resampler.process(v, [&](double x) {
                const double tape = std::tanh(drive * x) / normalization;
                const double tube = (std::tanh(drive * (x + .12)) - tubeOffset) / normalization;
                return (1 - p.style) * tape + p.style * tube;
            });
            const double y =
                state((shaped - p.prePole * s.sPrevious + (1 + k) * p.prePole * s.yPrevious) /
                      (1 + k * p.prePole));
            s.sPrevious = shaped;
            s.yPrevious = y;
            const double z = state(y - s.dcPrevious + p.dcPole * s.dcState);
            s.dcPrevious = y;
            s.dcState = z;
            s.toneState = state(tonePole * s.toneState + (1 - tonePole) * z);
            channels[c][i] = p.mix == 0 ? static_cast<float>(delayed)
                                        : sample((1 - p.mix) * delayed + p.mix * s.toneState);
        }
    }
    return true;
}
} // namespace darkvoice
