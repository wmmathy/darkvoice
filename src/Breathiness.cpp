#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <darkvoice/Breathiness.hpp>
#include <limits>
#include <numbers>
#include <vector>
namespace darkvoice {
namespace {
double clean(double x) noexcept {
    return !std::isfinite(x) || std::fpclassify(x) == FP_SUBNORMAL ? 0 : x;
}
double input(float x) noexcept {
    return !std::isfinite(x) || std::fpclassify(x) == FP_SUBNORMAL ? 0 : x;
}
float output(double x) noexcept {
    if (!std::isfinite(x))
        return 0;
    constexpr double limit = std::numeric_limits<float>::max();
    const float y = static_cast<float>(std::clamp(x, -limit, limit));
    return std::fpclassify(y) == FP_SUBNORMAL ? 0 : y;
}
} // namespace
struct Breathiness::Impl {
    std::size_t frames = 0, channels = 0, window = 0, leaves = 0, position = 0, noisePosition = 0;
    double coefficient = 0, amount = 0;
    std::uint32_t random = 0x4e4f4953;
    std::vector<double> tree;
    std::array<double, 129> kernel{}, noise{};
};
Breathiness::Breathiness() noexcept = default;
Breathiness::~Breathiness() = default;
bool Breathiness::valid(const BreathinessParameters &p) noexcept {
    return std::isfinite(p.amount) && p.amount >= 0 && p.amount <= 1;
}
bool Breathiness::prepare(double rate, std::size_t frames, std::size_t channels) noexcept {
    impl_.reset();
    if ((rate != 44100 && rate != 48000 && rate != 88200 && rate != 96000) || frames == 0 ||
        frames > 65536 || (channels != 1 && channels != 2))
        return false;
    try {
        auto next = std::make_unique<Impl>();
        next->frames = frames;
        next->channels = channels;
        next->window = static_cast<std::size_t>(std::llround(rate * .020));
        next->leaves = 1;
        while (next->leaves < next->window)
            next->leaves *= 2;
        next->tree.resize(next->leaves * 2);
        next->coefficient = 1 - std::exp(-1 / (.010 * rate));
        double sum = 0, energy = 0;
        for (std::size_t n = 0; n < 129; ++n) {
            const double k = static_cast<double>(n) - 64;
            const double ideal = k == 0 ? 16000 / rate
                                        : (std::sin(2 * std::numbers::pi * 10000 * k / rate) -
                                           std::sin(2 * std::numbers::pi * 2000 * k / rate)) /
                                              (std::numbers::pi * k);
            const double w = .42 - .5 * std::cos(2 * std::numbers::pi * n / 128) +
                             .08 * std::cos(4 * std::numbers::pi * n / 128);
            next->kernel[n] = ideal * w;
            sum += next->kernel[n];
        }
        next->kernel[64] -= sum;
        for (double h : next->kernel)
            energy += h * h;
        const double divisor = std::sqrt(((1 - std::ldexp(1., -48)) / 3) * energy);
        for (double &h : next->kernel)
            h /= divisor;
        impl_ = std::move(next);
        target_ = 0;
        reset();
        return true;
    } catch (...) {
        return false;
    }
}
bool Breathiness::setParameters(const BreathinessParameters &p) noexcept {
    if (!valid(p))
        return false;
    target_ = p.amount;
    return true;
}
void Breathiness::reset() noexcept {
    if (!impl_)
        return;
    auto &p = *impl_;
    std::fill(p.tree.begin(), p.tree.end(), 0);
    p.noise = {};
    p.position = p.noisePosition = 0;
    p.random = 0x4e4f4953;
    p.amount = target_;
}
bool Breathiness::process(std::span<float *> channels, std::size_t frames) noexcept {
    if (!impl_ || channels.size() != impl_->channels || frames > impl_->frames)
        return false;
    for (std::size_t c = 0; c < channels.size(); ++c) {
        if (frames && !channels[c])
            return false;
        for (std::size_t prior = 0; prior < c; ++prior)
            if (channels[c] == channels[prior])
                return false;
    }
    auto &p = *impl_;
    constexpr double low = .00003162277660168379, high = .00031622776601683794;
    for (std::size_t i = 0; i < frames; ++i) {
        const double left = input(channels[0][i]);
        const double right = p.channels == 2 ? input(channels[1][i]) : left;
        const double mid = p.channels == 2 ? .5 * left + .5 * right : left;
        // Recompute only the leaf's ancestors. No subtraction of old energy:
        // quiet leaves survive even when a previous near-FLT_MAX sample leaves.
        auto node = p.leaves + p.position;
        p.tree[node] = mid * mid;
        while (node > 1) {
            node /= 2;
            p.tree[node] = p.tree[2 * node] + p.tree[2 * node + 1];
        }
        p.position = (p.position + 1) % p.window;
        const double envelope = std::sqrt(p.tree[1] / p.window);
        const double t = std::clamp((envelope - low) / (high - low), 0., 1.);
        p.amount = clean(p.amount + p.coefficient * (target_ - p.amount));
        // Advance once per frame, including neutral and silent processing.
        p.random ^= p.random << 13;
        p.random ^= p.random >> 17;
        p.random ^= p.random << 5;
        const double u = (static_cast<double>(p.random >> 8) + .5) / 8388608 - 1;
        p.noise[p.noisePosition] = clean(.1 * p.amount * envelope * t * t * (3 - 2 * t) * u);
        double addition = 0;
        auto index = p.noisePosition;
        for (std::size_t tap = 0; tap < 129; ++tap) {
            addition += p.kernel[tap] * p.noise[index];
            index = index == 0 ? 128 : index - 1;
        }
        p.noisePosition = (p.noisePosition + 1) % 129;
        addition = clean(addition);
        // Avoid adding +0 to -0 on the exact neutral/silent path.
        channels[0][i] = output(addition == 0 ? left : left + addition);
        if (p.channels == 2)
            channels[1][i] = output(addition == 0 ? right : right + addition);
    }
    return true;
}
} // namespace darkvoice
