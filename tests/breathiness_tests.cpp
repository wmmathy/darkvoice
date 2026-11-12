#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cfenv>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <darkvoice/Breathiness.hpp>
#include <limits>
#include <new>
#include <numbers>
#include <vector>
namespace {
std::atomic<bool> forbidden{false};
std::atomic<int> failuresToAllocate{0};
std::atomic<std::size_t> allocations{0};
int checks = 0, failures = 0;
void check(bool ok, const char *message) {
    ++checks;
    if (!ok) {
        ++failures;
        std::fprintf(stderr, "FAIL %s\n", message);
    }
}
using Samples = std::array<std::vector<float>, 2>;
bool render(darkvoice::Breathiness &fx, Samples &samples, std::size_t channels,
            bool irregular = true) {
    const std::array<std::size_t, 6> chunks{1, 17, 257, 3, 56, 91};
    std::size_t at = 0, step = 0;
    while (at < samples[0].size()) {
        auto n = std::min(samples[0].size() - at, irregular ? chunks[step++ % chunks.size()] : 257);
        std::array<float *, 2> pointers{samples[0].data() + at, samples[1].data() + at};
        if (!fx.process(std::span<float *>(pointers.data(), channels), n))
            return false;
        at += n;
    }
    return true;
}
std::vector<long double> coefficients(double rate) {
    std::vector<long double> h(129);
    const long double pi = std::numbers::pi_v<long double>;
    auto sinc = [pi](long double x) { return x == 0 ? 1.L : std::sin(pi * x) / (pi * x); };
    long double sum = 0, norm = 0;
    for (int n = 0; n <= 128; ++n) {
        const long double k = n - 64;
        const long double window =
            .42L - .5L * std::cos(pi * n / 64) + .08L * std::cos(pi * n / 32);
        h[n] = window * (20000.L / rate * sinc(20000.L / rate * k) -
                         4000.L / rate * sinc(4000.L / rate * k));
        sum += h[n];
    }
    h[64] -= sum;
    for (auto x : h)
        norm += x * x;
    norm = std::sqrt(norm * ((1 - std::ldexp(1.L, -48)) / 3));
    for (auto &x : h)
        x /= norm;
    return h;
}
std::uint32_t randomStep(std::uint32_t x) {
    x ^= x << 13;
    x ^= x >> 17;
    return x ^ (x << 5);
}
void kernelGates(double rate) {
    const auto h = coefficients(rate);
    long double sum = 0, l1 = 0, energy = 0;
    for (auto x : h) {
        sum += x;
        l1 += std::abs(x);
        energy += x * x;
    }
    check(std::abs(sum) <= 1e-10L && l1 <= 8.20L, "independent FIR DC and peak bound");
    long double all = 0, inside = 0, low = 0;
    for (int i = 0; i <= 4096; ++i) {
        const long double frequency = (rate / 2) * i / 4096;
        std::complex<long double> response{};
        for (std::size_t n = 0; n < h.size(); ++n)
            response +=
                h[n] * std::polar(1.L, -2 * std::numbers::pi_v<long double> * frequency * n / rate);
        const auto power = std::norm(response) * ((i == 0 || i == 4096) ? .5L : 1.L);
        all += power;
        if (frequency >= 1000 && frequency <= 12000)
            inside += power;
        if (frequency < 1000)
            low += power;
    }
    check(inside / all >= .999L && low / all <= .0002L, "independent dense spectrum noise colour");
    check(std::abs(energy * ((1 - std::ldexp(1.L, -48)) / 3) - 1) < 1e-14L,
          "unit stationary white-noise RMS kernel");
    std::printf("rate %.0f l1 %.9Lf in-band %.9Lf below1k %.9Lf\n", rate, l1, inside / all,
                low / all);
}
void reference(double rate, std::size_t channels) {
    constexpr std::size_t count = 4300;
    Samples source, actual;
    for (auto &s : source)
        s.resize(count);
    for (std::size_t i = 0; i < count; ++i) {
        const double amplitude = i < 650 ? .00012 : .2;
        source[0][i] = static_cast<float>(amplitude * std::cos(.071 * i));
        source[1][i] = static_cast<float>(amplitude * (.4 * std::sin(.031 * i) + .2));
    }
    actual = source;
    darkvoice::Breathiness fx;
    check(fx.prepare(rate, 257, channels) && fx.setParameters({.3f}), "oracle preparation");
    fx.reset();
    const auto h = coefficients(rate);
    const auto window = static_cast<std::size_t>(std::llround(rate * .020));
    std::vector<long double> excitations(count), mid(count);
    std::uint32_t random = 0x4e4f4953;
    const std::array<std::size_t, 4> boundaries{0, 1250, 2900, count};
    const std::array<float, 3> targets{.3f, .9f, 0};
    long double startAmount = .3f;
    double worst = 0;
    for (std::size_t segment = 0; segment < 3; ++segment) {
        check(fx.setParameters({targets[segment]}), "oracle control snapshot");
        const auto begin = boundaries[segment], end = boundaries[segment + 1];
        std::size_t position = begin;
        while (position < end) {
            auto n = std::min<std::size_t>(end - position, 137);
            std::array<float *, 2> ptr{actual[0].data() + position, actual[1].data() + position};
            check(fx.process(std::span<float *>(ptr.data(), channels), n), "oracle process");
            position += n;
        }
        for (std::size_t i = begin; i < end; ++i) {
            mid[i] = channels == 1 ? source[0][i] : .5L * source[0][i] + .5L * source[1][i];
            long double sum = 0;
            for (std::size_t j = i + 1 > window ? i + 1 - window : 0; j <= i; ++j)
                sum += mid[j] * mid[j];
            const long double envelope = std::sqrt(sum / window);
            const long double lo = std::pow(10.L, -90.L / 20), hi = std::pow(10.L, -70.L / 20);
            const long double t = std::clamp((envelope - lo) / (hi - lo), 0.L, 1.L);
            const long double amount =
                targets[segment] +
                (startAmount - targets[segment]) *
                    std::exp(-static_cast<long double>(i - begin + 1) / (.010L * rate));
            random = randomStep(random);
            const long double uniform = (static_cast<long double>(random >> 8) + .5L) / 8388608 - 1;
            excitations[i] = .1L * amount * envelope * t * t * (3 - 2 * t) * uniform;
            long double addition = 0;
            for (std::size_t k = 0; k <= std::min<std::size_t>(128, i); ++k)
                addition += h[k] * excitations[i - k];
            for (std::size_t c = 0; c < channels; ++c) {
                const auto expected = static_cast<float>(source[c][i] + addition);
                worst = std::max(worst, std::abs(static_cast<double>(actual[c][i]) - expected));
            }
        }
        startAmount = targets[segment] +
                      (startAmount - targets[segment]) *
                          std::exp(-static_cast<long double>(end - begin) / (.010L * rate));
    }
    check(worst < 3e-8, "independent brute RMS, closed smoothing and direct convolution oracle");
    std::printf("oracle %.0f/%zu max %.12g\n", rate, channels, worst);
}
void behavior(double rate, std::size_t channels) {
    darkvoice::Breathiness a, b;
    check(a.prepare(rate, 257, channels) && b.prepare(rate, 257, channels), "behavior prepare");
    Samples x, y;
    const std::array<float, 10> edges{0.f,
                                      -0.f,
                                      .1f,
                                      -.3f,
                                      std::numeric_limits<float>::min(),
                                      -std::numeric_limits<float>::min(),
                                      std::numeric_limits<float>::max(),
                                      -std::numeric_limits<float>::max(),
                                      9e-22f,
                                      -9e-22f};
    x[0].assign(edges.begin(), edges.end());
    x[1] = x[0];
    y = x;
    check(render(a, x, channels), "neutral edge processing");
    bool exact = true;
    for (std::size_t c = 0; c < channels; ++c)
        for (std::size_t i = 0; i < x[c].size(); ++i)
            exact &= std::bit_cast<std::uint32_t>(x[c][i]) == std::bit_cast<std::uint32_t>(y[c][i]);
    check(exact, "neutral normal bits and signed zero exact");
    check(a.setParameters({1}) && b.setParameters({1}), "active behavior targets");
    a.reset();
    b.reset();
    for (auto &s : x)
        s.assign(5000, 0);
    check(render(a, x, channels) &&
              std::all_of(x[0].begin(), x[0].end(), [](float v) { return v == 0; }),
          "initial silence never produces noise");
    a.reset();
    for (std::size_t i = 0; i < 5000; ++i) {
        x[0][i] = static_cast<float>(.2 * std::sin(.06 * i));
        x[1][i] = channels == 2 ? -x[0][i] : x[0][i];
    }
    y = x;
    check(render(a, x, channels), "antiphase processing");
    check(channels == 1 || x == y, "fresh pure antiphase produces no center excitation");
    a.reset();
    b.reset();
    for (std::size_t i = 0; i < 5000; ++i) {
        x[0][i] = static_cast<float>(.2 * std::cos(.03 * i));
        x[1][i] = static_cast<float>(.1 * std::sin(.07 * i));
    }
    y = x;
    const auto original = x;
    check(render(a, x, channels) && render(b, y, channels, false) && x == y,
          "partition invariance exact");
    if (channels == 2) {
        double sideError = 0;
        for (std::size_t i = 0; i < 5000; ++i)
            sideError = std::max(sideError,
                                 std::abs((static_cast<double>(x[0][i]) - x[1][i]) -
                                          (static_cast<double>(original[0][i]) - original[1][i])));
        check(sideError < 4e-8, "dry Side preserved within output rounding");
    }
    a.reset();
    y = original;
    check(render(a, y, channels) && y == x, "reset reproducibility");
    const std::size_t tail = static_cast<std::size_t>(std::llround(rate * .020)) + 128;
    for (auto &s : x)
        s.assign(tail + 257, 0);
    check(render(a, x, channels), "tail rendering");
    check(std::all_of(x[0].begin() + static_cast<std::ptrdiff_t>(tail), x[0].end(),
                      [](float v) { return v == 0; }),
          "finite N+128 silence tail");
    a.reset();
    b.reset();
    for (auto &s : x) {
        s.assign(tail + 700, .0002f);
        s[0] = std::numeric_limits<float>::max();
    }
    y = x;
    for (auto &s : y)
        s[0] = .0002f;
    check(render(a, x, channels) && render(b, y, channels), "huge to quiet processing");
    check(std::equal(x[0].begin() + static_cast<std::ptrdiff_t>(tail), x[0].end(),
                     y[0].begin() + static_cast<std::ptrdiff_t>(tail)),
          "sum tree recovers quiet signal exactly after huge history leaves");
    a.reset();
    for (auto &s : x)
        s.assign(tail + 200, 0);
    x[0][0] = std::numeric_limits<float>::max();
    if (channels == 2)
        x[1][0] = x[0][0];
    check(render(a, x, channels) &&
              std::all_of(x[0].begin(), x[0].end(), [](float v) { return std::isfinite(v); }),
          "finite extremes clamp safely");
    check(std::all_of(x[0].begin() + static_cast<std::ptrdiff_t>(tail), x[0].end(),
                      [](float v) { return v == 0; }),
          "huge history silence clears exactly");
    a.reset();
    for (auto &s : x)
        s = {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
             std::numeric_limits<float>::denorm_min()};
    check(render(a, x, channels) && x[0] == std::vector<float>(3, 0),
          "nonfinite and subnormal inputs sanitized");
}
void statistics(double rate, std::size_t channels) {
    darkvoice::Breathiness full, half;
    check(full.prepare(rate, 257, channels) && half.prepare(rate, 257, channels),
          "statistical prepare");
    check(full.setParameters({1}) && half.setParameters({.5f}), "statistical targets");
    full.reset();
    half.reset();
    Samples x, y;
    const auto warmup = static_cast<std::size_t>(rate * .1),
               count = warmup + static_cast<std::size_t>(2 * rate);
    for (auto &s : x)
        s.assign(count, .1f);
    y = x;
    check(render(full, x, channels) && render(half, y, channels), "stationary rendering");
    long double power = 0, halfPower = 0;
    double maxPeak = 0;
    for (std::size_t i = warmup; i < count; ++i) {
        const double v = x[0][i] - .1f, w = y[0][i] - .1f;
        power += v * v;
        halfPower += w * w;
        maxPeak = std::max(maxPeak, std::abs(v));
    }
    const double rms = std::sqrt(static_cast<double>(power / (count - warmup)));
    check(std::abs(rms / .01 - 1) < .05, "stationary addition RMS within five percent");
    check(std::abs(std::sqrt(static_cast<double>(halfPower / power)) - .5) < 1e-6,
          "half amount halves addition RMS");
    check(maxPeak <= .082001, "declared added peak bound");
    check(channels == 1 || x[0] == x[1], "duplicated stereo input produces identical output");
    std::printf("stationary %.0f/%zu RMS %.9g\n", rate, channels, rms);
}
void stereo(double rate) {
    darkvoice::Breathiness stereoFx, monoFx;
    check(stereoFx.prepare(rate, 257, 2) && monoFx.prepare(rate, 257, 1) &&
              stereoFx.setParameters({1}) && monoFx.setParameters({1}),
          "stereo fold preparation");
    stereoFx.reset();
    monoFx.reset();
    Samples x, mono;
    for (auto &v : x)
        v.resize(6000);
    for (auto &v : mono)
        v.resize(6000);
    for (std::size_t i = 0; i < 6000; ++i) {
        x[0][i] = static_cast<float>(static_cast<int>(i % 117) - 58) / 1024;
        x[1][i] = static_cast<float>(static_cast<int>(i % 71) - 35) / 2048;
        mono[0][i] = .5f * x[0][i] + .5f * x[1][i];
    }
    check(render(stereoFx, x, 2) && render(monoFx, mono, 1), "stereo and folded-mono rendering");
    double worst = 0;
    for (std::size_t i = 0; i < 6000; ++i)
        worst = std::max(worst,
                         std::abs(.5 * static_cast<double>(x[0][i]) + .5 * x[1][i] - mono[0][i]));
    check(worst < 8e-9, "stereo fold equals processed mid within float rounding");
    stereoFx.reset();
    x[0].assign(6000, .2f);
    x[1].assign(6000, 0);
    check(render(stereoFx, x, 2), "one-sided center excitation render");
    bool nonzero = false;
    for (std::size_t i = 0; i < 6000; ++i) {
        nonzero |= x[1][i] != 0;
        worst = std::max(worst, std::abs((static_cast<double>(x[0][i]) - .2f) - x[1][i]));
    }
    check(nonzero && worst < 2e-8, "intentional center crossfeed shares addition within rounding");
    const auto tail = static_cast<std::size_t>(std::llround(rate * .020)) + 128;
    x[0].assign(tail + 300, .125f);
    x[1].assign(tail + 300, -.125f);
    check(render(stereoFx, x, 2) &&
              std::all_of(x[0].begin() + static_cast<std::ptrdiff_t>(tail), x[0].end(),
                          [](float v) { return v == .125f; }) &&
              std::all_of(x[1].begin() + static_cast<std::ptrdiff_t>(tail), x[1].end(),
                          [](float v) { return v == -.125f; }),
          "old texture clears by finite tail under continuing antiphase input");
}
void longDecay(double rate) {
    darkvoice::Breathiness fx;
    check(fx.prepare(rate, 257, 1) && fx.setParameters({1}), "long decay prepare");
    fx.reset();
    std::array<float, 257> samples{};
    std::array<float *, 1> pointers{samples.data()};
    samples.fill(.1f);
    check(fx.process(pointers, samples.size()) && fx.setParameters({0}), "long decay target");
    bool ok = true;
    for (std::size_t at = 0; at < static_cast<std::size_t>(9 * rate); at += samples.size()) {
        samples.fill(.1f);
        ok &= fx.process(pointers, samples.size());
    }
    std::feclearexcept(FE_UNDERFLOW);
    for (int i = 0; i < 16; ++i) {
        samples.fill(.1f);
        ok &= fx.process(pointers, samples.size());
        ok &= std::all_of(samples.begin(), samples.end(), [](float v) { return v == .1f; });
    }
    check(ok && !(std::fetestexcept(FE_UNDERFLOW) & FE_UNDERFLOW),
          "amount and excitation histories avoid persistent underflow after decay");
}
void outputSubnormals(double rate, std::size_t channels) {
    darkvoice::Breathiness fx;
    check(fx.prepare(rate, 257, channels) && fx.setParameters({1}),
          "output subnormal regression prepare");
    fx.reset();
    std::array<float, 257> left{}, right{};
    left.fill(.1f);
    right.fill(.1f);
    std::array<float *, 2> pointers{left.data(), right.data()};
    bool ok = fx.process(std::span<float *>(pointers.data(), channels), left.size());
    const bool audibleBeforeDecay = std::any_of(left.begin(), left.end(), [](float x) {
        return x != .1f && std::fpclassify(x - .1f) == FP_NORMAL;
    });
    check(ok && audibleBeforeDecay && fx.setParameters({0}),
          "normal generated texture precedes output subnormal decay");
    const auto decay = static_cast<std::size_t>(std::ceil(.9 * rate));
    for (std::size_t at = 0; at < decay;) {
        const auto frames = std::min(left.size(), decay - at);
        left.fill(.1f);
        right.fill(.1f);
        ok &= fx.process(std::span<float *>(pointers.data(), channels), frames);
        at += frames;
    }
    // e^-90 amount is nonzero in double but its generated noise is in the
    // float-subnormal range. Zero dry input exposes that addition directly.
    Samples tail;
    for (auto &s : tail)
        s.assign(static_cast<std::size_t>(std::llround(.020 * rate)) + 160, 0);
    ok &= render(fx, tail, channels);
    for (std::size_t c = 0; c < channels; ++c)
        ok &= std::all_of(tail[c].begin(), tail[c].end(), [](float x) { return x == 0; });
    check(ok, "generated float-subnormal tail is cleared after output conversion");
}
void lifecycle() {
    darkvoice::Breathiness fx, paired;
    std::array<float, 257> a{}, b{};
    std::array<float *, 2> ptr{a.data(), b.data()};
    check(!fx.process(std::span<float *>(ptr.data(), 1), 1) && fx.latencySamples() == 0,
          "unprepared lifecycle");
    for (double rate : {0., 32000., 48001., std::numeric_limits<double>::quiet_NaN()})
        check(!fx.prepare(rate, 257, 1), "bad sample rate");
    check(!fx.prepare(48000, 0, 1) && !fx.prepare(48000, 65537, 1) && !fx.prepare(48000, 1, 3),
          "invalid preparation limits");
    check(fx.prepare(48000, 257, 2) && paired.prepare(48000, 257, 2), "atomicity prepare");
    check(fx.setParameters({.7f}) && paired.setParameters({.7f}), "atomicity targets");
    fx.reset();
    paired.reset();
    for (float value : {-.001f, 1.001f, std::numeric_limits<float>::quiet_NaN(),
                        std::numeric_limits<float>::infinity()})
        check(!fx.setParameters({value}), "invalid target rejected atomically");
    check(!fx.process(ptr, 258), "oversized rejected");
    std::array<float *, 2> bad{a.data(), nullptr};
    check(!fx.process(bad, 1), "null channel rejected");
    bad = {a.data(), a.data()};
    check(!fx.process(bad, 1), "duplicate channel rejected");
    bad = {nullptr, b.data()};
    check(fx.process(bad, 0), "empty call does not advance");
    Samples x, y;
    for (auto &s : x)
        s.assign(1000, .2f);
    y = x;
    check(render(fx, x, 2) && render(paired, y, 2) && x == y,
          "invalid and empty calls preserve all history and target");
    fx.reset();
    a.fill(.1f);
    b.fill(.2f);
    const auto before = allocations.load();
    forbidden = true;
    bool safe = fx.setParameters({.2f}) && fx.process(ptr, 257);
    fx.reset();
    forbidden = false;
    check(safe && allocations.load() == before, "callback set process reset do not allocate");
    for (int failAt : {1, 2}) {
        failuresToAllocate = failAt;
        check(!fx.prepare(48000, 257, 1), "prepare allocation failure handled");
        failuresToAllocate = 0;
        check(!fx.process(std::span<float *>(ptr.data(), 1), 1),
              "failed prepare leaves unprepared");
    }
}
} // namespace
#if defined(__GNUC__) || defined(__clang__)
#define DV_NOINLINE __attribute__((noinline))
#else
#define DV_NOINLINE
#endif
DV_NOINLINE void *operator new(std::size_t n) {
    ++allocations;
    if (forbidden || (failuresToAllocate > 0 && --failuresToAllocate == 0))
        throw std::bad_alloc();
    if (void *p = std::malloc(std::max<std::size_t>(n, 1)))
        return p;
    throw std::bad_alloc();
}
DV_NOINLINE void *operator new[](std::size_t n) {
    return ::operator new(n);
}
DV_NOINLINE void operator delete(void *p) noexcept {
    std::free(p);
}
DV_NOINLINE void operator delete[](void *p) noexcept {
    std::free(p);
}
DV_NOINLINE void operator delete(void *p, std::size_t) noexcept {
    std::free(p);
}
DV_NOINLINE void operator delete[](void *p, std::size_t) noexcept {
    std::free(p);
}
int main() {
    std::uint32_t word = 0x4e4f4953;
    for (auto expected : {0x4bca26c1U, 0xed522848U, 0xa334f0c5U, 0x98a58433U}) {
        word = randomStep(word);
        check(word == expected, "PRNG independently calculated known vectors");
    }
    lifecycle();
    for (double rate : {44100., 48000., 88200., 96000.}) {
        kernelGates(rate);
        stereo(rate);
        longDecay(rate);
        for (std::size_t channels : {1U, 2U}) {
            outputSubnormals(rate, channels);
            reference(rate, channels);
            behavior(rate, channels);
            statistics(rate, channels);
        }
    }
    std::printf("Breathiness checks: %d, failures: %d\n", checks, failures);
    return failures != 0;
}
