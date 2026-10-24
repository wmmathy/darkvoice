#include <algorithm>
#include <array>
#include <cmath>
#include <darkvoice/SpaceReverb.hpp>
#include <limits>
#include <numbers>
#include <vector>
namespace darkvoice {
namespace {
bool range(float value, float low, float high) noexcept {
    return std::isfinite(value) && value >= low && value <= high;
}
double input(float value) noexcept {
    return std::isfinite(value) && std::abs(value) >= std::numeric_limits<float>::min() ? value : 0;
}
double state(double value) noexcept {
    return std::isfinite(value) && std::abs(value) >= std::numeric_limits<double>::min() ? value
                                                                                         : 0;
}
float output(double value) noexcept {
    if (!std::isfinite(value) || std::abs(value) < std::numeric_limits<float>::min())
        return 0;
    constexpr double maximum = std::numeric_limits<float>::max();
    return static_cast<float>(std::clamp(value, -maximum, maximum));
}
std::size_t length(double rate, double ms) noexcept {
    return std::max<std::size_t>(1, static_cast<std::size_t>(std::llround(rate * ms / 1000)));
}
struct Ring {
    std::vector<double> data;
    std::size_t position = 0;
    void prepare(std::size_t size) {
        data.assign(size, 0);
        position = 0;
    }
    void reset() noexcept {
        std::fill(data.begin(), data.end(), 0);
        position = 0;
    }
    void advance() noexcept {
        position = position + 1 == data.size() ? 0 : position + 1;
    }
    void push(double value) noexcept {
        data[position] = state(value);
        advance();
    }
    double read() const noexcept {
        return data[position];
    }
    double tap(double delay) const noexcept {
        const auto integer = static_cast<std::size_t>(delay);
        const double fraction = delay - integer;
        const auto first =
            position >= integer ? position - integer : position + data.size() - integer;
        const auto second = first == 0 ? data.size() - 1 : first - 1;
        return (1 - fraction) * data[first] + fraction * data[second];
    }
};
struct Allpass {
    Ring ring;
    void prepare(double rate, double ms) {
        ring.prepare(length(rate, ms));
    }
    void reset() noexcept {
        ring.reset();
    }
    double process(double x) noexcept {
        const double y = ring.read() - .5 * x;
        ring.push(x + .5 * y);
        return y;
    }
};
template <std::size_t N> struct Fdn {
    std::array<Ring, N> delays;
    std::array<double, N> lp{};
    double pole = 0;
    void prepare(double rate, const std::array<double, N> &times, double cutoff) {
        for (std::size_t i = 0; i < N; ++i)
            delays[i].prepare(length(rate, times[i]));
        pole = std::exp(-2 * std::numbers::pi * cutoff / rate);
    }
    void reset() noexcept {
        for (auto &delay : delays)
            delay.reset();
        lp = {};
    }
    double process(double x, double gain, const std::array<int, N> &signs) noexcept {
        constexpr double scale = N == 4 ? .5 : std::numbers::sqrt2 / 4;
        std::array<double, N> d{}, q{};
        double late = 0;
        for (std::size_t i = 0; i < N; ++i) {
            d[i] = delays[i].read();
            lp[i] = state(pole * lp[i] + (1 - pole) * d[i]);
            q[i] = lp[i];
            late += signs[i] * scale * d[i];
        }
        // Walsh-Hadamard butterflies implement (-1)^popcount(i & j)
        // using N*log2(N) additions instead of a dense N*N product.
        for (std::size_t width = 1; width < N; width *= 2)
            for (std::size_t block = 0; block < N; block += 2 * width)
                for (std::size_t j = 0; j < width; ++j) {
                    const double a = q[block + j], b = q[block + j + width];
                    q[block + j] = a + b;
                    q[block + j + width] = a - b;
                }
        for (std::size_t i = 0; i < N; ++i)
            delays[i].push((1 - gain) * scale * x + gain * scale * q[i]);
        return late;
    }
};
struct Channel {
    Ring predelay, earlyRoom, earlyHall;
    std::array<std::size_t, 6> roomTimes{};
    std::array<std::size_t, 4> hallTimes{};
    Fdn<4> room;
    Fdn<8> hall;
    std::array<Allpass, 2> hallInput, plateInput, plateFeedback;
    std::array<Ring, 2> tanks;
    std::array<double, 2> plateLp{};
    double platePole = 0, tone = 0;
    void prepare(double rate) {
        predelay.prepare(static_cast<std::size_t>(std::ceil(.100 * rate)) + 2);
        constexpr std::array<double, 6> roomMs{3, 5, 8, 12, 17, 23};
        constexpr std::array<double, 4> hallMs{12, 19, 27, 37};
        for (std::size_t i = 0; i < roomTimes.size(); ++i)
            roomTimes[i] = length(rate, roomMs[i]);
        for (std::size_t i = 0; i < hallTimes.size(); ++i)
            hallTimes[i] = length(rate, hallMs[i]);
        earlyRoom.prepare(roomTimes.back() + 1);
        earlyHall.prepare(hallTimes.back() + 1);
        room.prepare(rate, {29, 37, 43, 53}, 10000);
        hall.prepare(rate, {43, 53, 61, 71, 79, 89, 101, 113}, 6000);
        hallInput[0].prepare(rate, 4.7);
        hallInput[1].prepare(rate, 7.3);
        plateInput[0].prepare(rate, 1.7);
        plateInput[1].prepare(rate, 3.1);
        plateFeedback[0].prepare(rate, 7.7);
        plateFeedback[1].prepare(rate, 11.3);
        tanks[0].prepare(length(rate, 23.9));
        tanks[1].prepare(length(rate, 31.3));
        platePole = std::exp(-2 * std::numbers::pi * 8000 / rate);
    }
    void reset() noexcept {
        predelay.reset();
        earlyRoom.reset();
        earlyHall.reset();
        room.reset();
        hall.reset();
        for (auto *allpasses : {&hallInput, &plateInput, &plateFeedback})
            for (auto &allpass : *allpasses)
                allpass.reset();
        for (auto &tank : tanks)
            tank.reset();
        plateLp = {};
        tone = 0;
    }
    std::array<double, 3> process(double x, double delay, double gain) noexcept {
        predelay.data[predelay.position] = x;
        x = predelay.tap(delay);
        predelay.advance();
        earlyRoom.data[earlyRoom.position] = x;
        earlyHall.data[earlyHall.position] = x;
        constexpr std::array<double, 6> roomWeights{.30, .24, .18, .13, .09, .06};
        constexpr std::array<double, 4> hallWeights{.40, .30, .20, .10};
        double roomEarly = 0, hallEarly = 0;
        for (std::size_t i = 0; i < roomTimes.size(); ++i)
            roomEarly += roomWeights[i] * earlyRoom.tap(roomTimes[i]);
        for (std::size_t i = 0; i < hallTimes.size(); ++i)
            hallEarly += hallWeights[i] * earlyHall.tap(hallTimes[i]);
        earlyRoom.advance();
        earlyHall.advance();
        const double roomLate = room.process(x, gain, {1, -1, 1, -1});
        double u = hallInput[1].process(hallInput[0].process(x));
        const double hallLate = hall.process(u, gain, {1, -1, 1, -1, -1, 1, -1, 1});
        u = plateInput[1].process(plateInput[0].process(x));
        const std::array<double, 2> d{tanks[0].read(), tanks[1].read()};
        std::array<double, 2> allpass{};
        for (std::size_t i = 0; i < 2; ++i) {
            plateLp[i] = state(platePole * plateLp[i] + (1 - platePole) * d[i]);
            allpass[i] = plateFeedback[i].process(plateLp[i]);
        }
        constexpr double scale = std::numbers::sqrt2 / 2;
        tanks[0].push((1 - gain) * scale * u + gain * allpass[1]);
        tanks[1].push((1 - gain) * scale * u - gain * allpass[0]);
        return {.60 * roomEarly + .40 * roomLate, .20 * hallEarly + .80 * hallLate,
                scale * (d[0] + d[1])};
    }
};
} // namespace
struct SpaceReverb::Impl {
    double rate = 0, smoothing = 0, modelSmoothing = 0;
    std::size_t maxFrames = 0, channels = 0;
    double decay = .5, toneHz = 8000, predelayMs = 0, mix = 0;
    std::array<double, 3> weights{1, 0, 0};
    std::array<Channel, 2> channel;
};
SpaceReverb::SpaceReverb() noexcept = default;
SpaceReverb::~SpaceReverb() = default;
bool SpaceReverb::valid(const SpaceReverbParameters &p) noexcept {
    return p.model >= 0 && p.model <= 2 && range(p.decay, 0, 1) && range(p.toneHz, 1000, 16000) &&
           range(p.preDelayMs, 0, 100) && range(p.mix, 0, .5f);
}
bool SpaceReverb::prepare(double rate, std::size_t maxFrames, std::size_t channels) noexcept {
    impl_.reset();
    if ((rate != 44100 && rate != 48000 && rate != 88200 && rate != 96000) || maxFrames == 0 ||
        maxFrames > 65536 || (channels != 1 && channels != 2))
        return false;
    try {
        auto next = std::make_unique<Impl>();
        next->rate = rate;
        next->maxFrames = maxFrames;
        next->channels = channels;
        next->smoothing = 1 - std::exp(-1 / (.010 * rate));
        next->modelSmoothing = 1 - std::exp(-1 / (.020 * rate));
        for (std::size_t i = 0; i < channels; ++i)
            next->channel[i].prepare(rate);
        impl_ = std::move(next);
        target_ = {};
        reset();
        return true;
    } catch (...) {
        return false;
    }
}
bool SpaceReverb::setParameters(const SpaceReverbParameters &p) noexcept {
    if (!valid(p))
        return false;
    target_ = p;
    return true;
}
void SpaceReverb::reset() noexcept {
    if (!impl_)
        return;
    impl_->decay = target_.decay;
    impl_->toneHz = target_.toneHz;
    impl_->predelayMs = target_.preDelayMs;
    impl_->mix = target_.mix;
    impl_->weights = {};
    impl_->weights[static_cast<std::size_t>(target_.model)] = 1;
    for (auto &channel : impl_->channel)
        channel.reset();
}
bool SpaceReverb::process(std::span<float *> channels, std::size_t frames) noexcept {
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
    const auto smooth = [&p](double &value, double goal) {
        value += p.smoothing * (goal - value);
        if (std::abs(value - goal) < 1e-7)
            value = goal;
    };
    for (std::size_t i = 0; i < frames; ++i) {
        smooth(p.decay, target_.decay);
        smooth(p.toneHz, target_.toneHz);
        smooth(p.predelayMs, target_.preDelayMs);
        smooth(p.mix, target_.mix);
        // The same affine update preserves nonnegative weights summing to one;
        // normalize rounding error without snapping individual components.
        double sum = 0;
        for (std::size_t model = 0; model < p.weights.size(); ++model) {
            p.weights[model] +=
                p.modelSmoothing *
                ((target_.model == static_cast<int>(model) ? 1.0 : 0.0) - p.weights[model]);
            sum += p.weights[model];
        }
        for (auto &weight : p.weights)
            weight /= sum;
        const double gain = .25 + .70 * p.decay;
        const double tonePole = std::exp(-2 * std::numbers::pi * p.toneHz / p.rate);
        for (std::size_t c = 0; c < channels.size(); ++c) {
            const double dry = input(channels[c][i]);
            const auto models = p.channel[c].process(dry, .001 * p.rate * p.predelayMs, gain);
            double wet = 0;
            for (std::size_t model = 0; model < models.size(); ++model)
                wet += p.weights[model] * models[model];
            auto &tone = p.channel[c].tone;
            tone = state(tonePole * tone + (1 - tonePole) * wet);
            channels[c][i] =
                p.mix == 0 ? static_cast<float>(dry) : output((1 - p.mix) * dry + p.mix * tone);
        }
    }
    return true;
}
} // namespace darkvoice
