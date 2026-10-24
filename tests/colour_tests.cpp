#include "../src/ColourOversampling.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <darkvoice/Colour.hpp>
#include <limits>
#include <new>
#include <numbers>
#include <vector>
namespace {
std::atomic<std::size_t> allocations{0};
std::atomic<bool> reject{false};
int checks = 0, failures = 0;
void check(bool pass, const char *message) {
    ++checks;
    if (!pass) {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", message);
    }
}
bool render(darkvoice::Colour &fx, std::vector<float> &left, std::vector<float> *right,
            std::size_t block) {
    for (std::size_t at = 0; at < left.size(); at += block) {
        std::array<float *, 2> ptr{left.data() + at, right ? right->data() + at : nullptr};
        if (!fx.process(std::span<float *>(ptr.data(), right ? 2U : 1U),
                        std::min(block, left.size() - at)))
            return false;
    }
    return true;
}
std::vector<float> signal(std::size_t count) {
    std::vector<float> data(count);
    for (std::size_t i = 0; i < count; ++i)
        data[i] = static_cast<float>(.2 * std::sin(.103 * i) + .07 * std::cos(.037 * i));
    return data;
}
double energy(const std::vector<float> &x, std::size_t begin = 0) {
    double sum = 0;
    for (std::size_t i = begin; i < x.size(); ++i)
        sum += static_cast<double>(x[i]) * x[i];
    return sum;
}
// Independent reference: explicit zero insertion, generic causal convolution,
// and even-phase sample selection. No production resampling helper is called.
std::array<double, 65> referenceKernel() {
    std::array<double, 65> h{};
    for (int j = -32; j <= 32; ++j) {
        const double phase = 2 * std::numbers::pi * (j + 32) / 64;
        const double window = .42 - .5 * std::cos(phase) + .08 * std::cos(2 * phase);
        h[static_cast<std::size_t>(j + 32)] =
            j == 0       ? .5
            : j % 2 == 0 ? 0
                         : std::sin(.5 * std::numbers::pi * j) * window / (std::numbers::pi * j);
    }
    double total = 0;
    for (double x : h)
        total += x;
    for (auto &x : h)
        x /= total;
    return h;
}
std::vector<double> convolve(const std::vector<double> &x, const std::array<double, 65> &h) {
    std::vector<double> result(x.size());
    for (std::size_t tap = 0; tap < h.size(); ++tap)
        if (h[tap] != 0)
            for (std::size_t i = tap; i < x.size(); ++i)
                result[i] += h[tap] * x[i - tap];
    return result;
}
std::vector<double> reference(const std::vector<float> &source, double rate, int model, float drive,
                              float tone, int stages, bool emphasis = true) {
    const double a = std::exp(-2 * std::numbers::pi * 1500 / rate);
    const double r = std::exp(-2 * std::numbers::pi * 10 / rate);
    const double pole = std::exp(-2 * std::numbers::pi * tone / rate);
    const double d = 1 + 5 * static_cast<double>(drive), k = model == 0 && emphasis ? .5 : 0;
    const double normalization = std::tanh(d), offset = std::tanh(.12 * d);
    std::vector<double> x(source.size());
    double pre = 0;
    for (std::size_t i = 0; i < x.size(); ++i) {
        pre = a * pre + (1 - a) * source[i];
        x[i] = source[i] + k * (source[i] - pre);
    }
    const auto h = referenceKernel();
    for (int stage = 0; stage < stages; ++stage) {
        std::vector<double> inserted(2 * x.size());
        for (std::size_t i = 0; i < x.size(); ++i)
            inserted[2 * i] = 2 * x[i];
        x = convolve(inserted, h);
    }
    for (auto &value : x)
        value = model == 0 ? std::tanh(d * value) / normalization
                           : (std::tanh(d * (value + .12)) - offset) / normalization;
    for (int stage = 0; stage < stages; ++stage) {
        const auto filtered = convolve(x, h);
        std::vector<double> selected(x.size() / 2);
        for (std::size_t i = 0; i < selected.size(); ++i)
            selected[i] = filtered[2 * i];
        x = std::move(selected);
    }
    double previousS = 0, previousY = 0, previousDc = 0, dc = 0, filtered = 0;
    for (auto &value : x) {
        const double y = (value - a * previousS + (1 + k) * a * previousY) / (1 + k * a);
        previousS = value;
        previousY = y;
        dc = y - previousDc + r * dc;
        previousDc = y;
        filtered = pole * filtered + (1 - pole) * dc;
        value = filtered;
    }
    return x;
}
void filters() {
    const auto h = darkvoice::colour_detail::kernel();
    const auto independent = referenceKernel();
    bool symmetric = true, zeros = true;
    double sum = 0;
    for (std::size_t i = 0; i < h.size(); ++i) {
        symmetric = symmetric && std::abs(h[i] - h[64 - i]) < 1e-15 &&
                    std::abs(h[i] - independent[i]) < 1e-15;
        zeros = zeros && (i == 32 || i % 2 != 0 || h[i] == 0);
        sum += h[i];
    }
    check(symmetric && zeros && std::abs(sum - 1) < 1e-14,
          "published halfband coefficients, symmetry, exact zeros and DC");
    double pass = 0, stop = 0;
    for (int bin = 0; bin <= 4096; ++bin) {
        const double frequency = .5 * bin / 4096;
        std::complex<double> response{};
        for (std::size_t j = 0; j < h.size(); ++j)
            response +=
                h[j] * std::exp(std::complex<double>{0, -2 * std::numbers::pi * frequency * j});
        if (frequency <= .2)
            pass = std::max(pass, std::abs(std::abs(response) - 1));
        if (frequency >= .3)
            stop = std::max(stop, std::abs(response));
    }
    std::printf("Kernel passband %.9g stopband %.3fdB\n", pass, 20 * std::log10(stop));
    check(pass < .0002, "kernel per-stage passband error below .0002");
    check(20 * std::log10(stop) < -75, "kernel per-stage stopband below -75dB");
    darkvoice::colour_detail::Oversampler8 resampler;
    resampler.prepare(h);
    std::vector<double> impulse(256);
    for (std::size_t i = 0; i < impulse.size(); ++i)
        impulse[i] = resampler.process(i == 0 ? 1 : 0, [](double x) { return x; });
    const auto peak = static_cast<std::size_t>(std::max_element(impulse.begin(), impulse.end()) -
                                               impulse.begin());
    bool linearPhase = true;
    for (std::size_t i = 0; i <= 112; ++i)
        linearPhase = linearPhase && std::abs(impulse[i] - impulse[112 - i]) < 1e-14;
    check(peak == 56 && linearPhase, "identity resampler symmetry and 56-sample group delay");
    resampler.reset();
    double settled = 0;
    for (int i = 0; i < 512; ++i)
        settled = resampler.process(1, [](double x) { return x; });
    check(std::abs(settled - 1) < 1e-7, "identity resampler settled DC unity");
}
void aliasMatrix() {
    // PREDECLARED before first run: N4096, single bins719/1597, two-tone683+1129;
    // aggregate amplitudes.15/.5, Tape+Tube drive1 tone16000, all4rates. Warm4096.
    constexpr std::size_t period = 4096, warm = 4096, samples = warm + period + 128;
    for (double rate : {44100., 48000., 88200., 96000.})
        for (int model : {0, 1})
            for (float amplitude : {.15f, .5f})
                for (int fixture = 0; fixture < 3; ++fixture) {
                    std::vector<float> source(samples);
                    for (std::size_t i = 0; i < source.size(); ++i) {
                        const double phase = 2 * std::numbers::pi * i / period;
                        source[i] = static_cast<float>(
                            amplitude * (fixture == 0   ? std::sin(719 * phase)
                                         : fixture == 1 ? std::sin(1597 * phase)
                                                        : .5 * (std::sin(683 * phase) +
                                                                std::sin(1129 * phase))));
                    }
                    const auto high = reference(source, rate, model, 1, 16000, 5);
                    const auto native = reference(source, rate, model, 1, 16000, 0);
                    auto actual = source;
                    darkvoice::Colour fx;
                    check(fx.prepare(rate, 257, 1) && fx.setParameters({model, 1, 16000, 1}),
                          "alias matrix setup");
                    fx.reset();
                    check(render(fx, actual, nullptr, 257), "alias matrix render");
                    double error8 = 0, error1 = 0, inputPower = 0;
                    for (std::size_t j = 0; j < period; ++j) {
                        const std::size_t at = warm + 56 + j;
                        const double e8 = actual[at] - high[at + 6];
                        const double e1 = native[at - 56] - high[at + 6];
                        error8 += e8 * e8;
                        error1 += e1 * e1;
                        inputPower += static_cast<double>(source[warm + j]) * source[warm + j];
                    }
                    const double residual = 10 * std::log10(error8 / inputPower);
                    const double improvement = 10 * std::log10(error1 / error8);
                    std::printf("Alias %.0fHz model%d amp%.2f fixture%d residual %.3fdB "
                                "improvement %.3fdB\n",
                                rate, model, amplitude, fixture, residual, improvement);
                    check(residual < -55, "8x residual below -55dB relative input");
                    check(improvement >= 20, "8x residual improves at least20dB against1x");
                }
}
double harmonic(const std::vector<float> &x, std::size_t begin, int bin) {
    std::complex<double> sum{};
    constexpr std::size_t period = 4096;
    for (std::size_t i = 0; i < period; ++i)
        sum += static_cast<double>(x[begin + i]) *
               std::exp(std::complex<double>{0, -2 * std::numbers::pi * bin * i / period});
    return 2 * std::abs(sum) / period;
}
std::vector<float> processedTone(int model, float drive, double amplitude, int bin) {
    constexpr std::size_t period = 4096, warm = 16384;
    std::vector<float> data(warm + period);
    for (std::size_t i = 0; i < data.size(); ++i)
        data[i] = static_cast<float>(amplitude * std::sin(2 * std::numbers::pi * bin * i / period));
    darkvoice::Colour fx;
    check(fx.prepare(48000, 257, 1) && fx.setParameters({model, drive, 20000, 1}),
          "character setup");
    fx.reset();
    check(render(fx, data, nullptr, 257), "character tone render");
    return data;
}
void character() {
    std::array<double, 2> compression{};
    for (std::size_t j = 0; j < 2; ++j) {
        const int bin = j == 0 ? 23 : 719;
        const auto low = processedTone(0, 1, .01, bin), high = processedTone(0, 1, .5, bin);
        compression[j] = (harmonic(high, 16384, bin) / .5) / (harmonic(low, 16384, bin) / .01);
    }
    std::printf("Tape amplitude compression low %.9g high %.9g\n", compression[0], compression[1]);
    check(compression[1] < .9 * compression[0],
          "Tape level-dependent HF compression stronger than low band");
    const auto tape = processedTone(0, .7f, .15, 53), tube = processedTone(1, .7f, .15, 53);
    const double oddEven = harmonic(tape, 16384, 106), even = harmonic(tube, 16384, 106);
    std::printf("Second harmonics Tape %.9g Tube %.9g\n", oddEven, even);
    check(oddEven < 1e-6 && even > .001,
          "Tube asymmetry creates actual even harmonics unlike odd Tape shaper");
    double delta = 0;
    for (std::size_t i = 16384; i < tape.size(); ++i)
        delta += std::pow(static_cast<double>(tape[i]) - tube[i], 2);
    check(delta / 4096 > .0001, "styles measurably differ on same material");
    darkvoice::Colour fx;
    check(fx.prepare(48000, 257, 1) && fx.setParameters({1, 1, 16000, 1}), "DC setup");
    fx.reset();
    std::vector<float> dc(96000, .3f);
    check(render(fx, dc, nullptr, 257), "DC settling render");
    const double remaining = energy(dc, dc.size() - 4096) / 4096;
    check(remaining < 1e-12 && energy(dc) > .01,
          "fixed DC blocker removes settled asymmetric DC output");
}
void alignmentOracle(double rate) {
    auto source = signal(2048);
    source[0] = .5f;
    const auto wet = reference(source, rate, 1, .7f, 8000, 3);
    for (float mix : {0.f, .25f, .75f, 1.f}) {
        darkvoice::Colour fx;
        check(fx.prepare(rate, 257, 1) && fx.setParameters({1, .7f, 8000, mix}),
              "parallel oracle setup");
        fx.reset();
        auto actual = source;
        check(render(fx, actual, nullptr, 113), "parallel oracle render");
        bool exact = true;
        for (std::size_t i = 0; i < source.size(); ++i) {
            const double dry = i < 56 ? 0 : source[i - 56];
            const double expected = (1 - static_cast<double>(mix)) * dry + mix * wet[i];
            exact = exact && std::abs(actual[i] - expected) < 1e-7;
        }
        check(exact, "independent convolution oracle verifies partial-mix delayed dry alignment");
    }
    // The published native recursive emphasis/de-emphasis is a memory process,
    // independently reconstructed here and distinguishable from its removal.
    const auto tape = reference(source, rate, 0, .8f, 16000, 3);
    const auto noMemory = reference(source, rate, 0, .8f, 16000, 3, false);
    darkvoice::Colour fx;
    check(fx.prepare(rate, 257, 1) && fx.setParameters({0, .8f, 16000, 1}), "memory oracle setup");
    fx.reset();
    auto actual = source;
    check(render(fx, actual, nullptr, 257), "memory oracle render");
    double error = 0, difference = 0;
    for (std::size_t i = 0; i < source.size(); ++i) {
        error = std::max(error, std::abs(actual[i] - tape[i]));
        difference += (tape[i] - noMemory[i]) * (tape[i] - noMemory[i]);
    }
    check(error < 1e-7 && difference / source.size() > 1e-7,
          "Tape native filter memory matches independent recurrence and affects nonlinear audio");
}
void benchmark() {
    for (double rate : {48000., 96000.})
        for (int model : {0, 1}) {
            darkvoice::Colour fx;
            check(fx.prepare(rate, 257, 2) && fx.setParameters({model, 1, 16000, 1}),
                  "cost measurement setup");
            fx.reset();
            auto left = signal(static_cast<std::size_t>(rate)), right = left;
            const auto start = std::chrono::steady_clock::now();
            const bool rendered = render(fx, left, &right, 257);
            const double seconds =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            check(rendered, "cost measurement render");
            std::printf("Cost %.0fHz stereo model%d one audio second: %.6f wall seconds (no "
                        "deadline claim)\n",
                        rate, model, seconds);
        }
}
void validation() {
    darkvoice::Colour fx;
    std::array<float, 8> left{}, right{};
    std::array<float *, 2> ptr{left.data(), right.data()};
    check(fx.latencySamples() == 0 && !fx.process(ptr, 8),
          "unprepared reports zero latency/rejects");
    for (double rate : {0., 22050., std::numeric_limits<double>::quiet_NaN()})
        check(!fx.prepare(rate, 8, 2), "unsupported rates rejected");
    check(!fx.prepare(48000, 0, 2) && !fx.prepare(48000, 65537, 2), "bounded capacity");
    check(!fx.prepare(48000, 8, 0) && !fx.prepare(48000, 8, 3), "mono/stereo only");
    reject = true;
    const bool prepared = fx.prepare(48000, 8, 2);
    reject = false;
    check(!prepared && !fx.process(ptr, 8), "allocation failure leaves unprepared");
    check(fx.prepare(48000, 8, 2) && fx.latencySamples() == 56, "prepare recovers fixed latency");
    const auto copy = left;
    std::array<float *, 2> null{left.data(), nullptr}, alias{left.data(), left.data()};
    check(!fx.process(null, 8) && left == copy, "null rejects atomically");
    check(!fx.process(alias, 8) && left == copy, "alias rejects atomically");
    check(!fx.process(ptr, 9) && left == copy, "oversized rejects atomically");
    check(!fx.process(std::span<float *>(ptr.data(), 1), 8), "wrong layout rejected");
    check(fx.process(null, 0), "zero frames no-op");
    for (auto member : {&darkvoice::ColourParameters::drive, &darkvoice::ColourParameters::toneHz,
                        &darkvoice::ColourParameters::mix})
        for (float bad :
             {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -1.f,
              std::numeric_limits<float>::max()}) {
            darkvoice::ColourParameters p;
            p.*member = bad;
            check(!fx.setParameters(p), "invalid scalar snapshot rejected");
        }
    check(!fx.setParameters({-1, .2f, 16000, 0}) && !fx.setParameters({2, .2f, 16000, 0}),
          "invalid style rejected");
    check(!fx.prepare(12345, 8, 2) && fx.latencySamples() == 0 && !fx.process(ptr, 8),
          "invalid prepare unprepares");
}
void behavior(double rate, std::size_t channels) {
    darkvoice::Colour fx, other;
    check(fx.prepare(rate, 257, channels) && other.prepare(rate, 257, channels),
          "behavior prepare");
    auto left = signal(4096), right = left;
    left[0] = 1e-21f;
    left[1] = -std::numeric_limits<float>::min();
    const auto dry = left, dryRight = right;
    check(render(fx, left, channels == 2 ? &right : nullptr, 257), "neutral render");
    bool exact = true;
    for (std::size_t i = 0; i < left.size(); ++i) {
        exact = exact && left[i] == (i < 56 ? 0 : dry[i - 56]);
        if (channels == 2)
            exact = exact && right[i] == (i < 56 ? 0 : dryRight[i - 56]);
    }
    check(exact, "neutral exact sanitized normal dry delayed56 samples");
    fx.reset();
    left.assign(256, 0);
    left[0] = std::numeric_limits<float>::quiet_NaN();
    left[1] = std::numeric_limits<float>::infinity();
    left[2] = -std::numeric_limits<float>::infinity();
    left[3] = std::numeric_limits<float>::denorm_min();
    left[4] = -std::numeric_limits<float>::denorm_min();
    left[5] = std::numeric_limits<float>::max();
    left[6] = std::numeric_limits<float>::min();
    left[7] = -std::numeric_limits<float>::min();
    left[8] = -std::numeric_limits<float>::max();
    right = left;
    auto sanitizedInput = left;
    std::fill(sanitizedInput.begin(), sanitizedInput.begin() + 5, 0);
    bool sanitized = render(fx, left, channels == 2 ? &right : nullptr, 113);
    for (std::size_t i = 0; i < left.size(); ++i) {
        const float expected = i < 56 ? 0 : sanitizedInput[i - 56];
        sanitized = sanitized && left[i] == expected && (channels == 1 || right[i] == expected);
    }
    check(sanitized, "neutral delayed path sanitizes only nonfinite/true subnormal values");
    check(fx.setParameters({1, .9f, 6000, .7f}) && other.setParameters({1, .9f, 6000, .7f}),
          "active set");
    fx.reset();
    other.reset();
    left = signal(8192);
    right = left;
    auto split = left, splitRight = right;
    check(render(fx, left, channels == 2 ? &right : nullptr, 257) &&
              render(other, split, channels == 2 ? &splitRight : nullptr, 37) && left == split &&
              (channels == 1 || right == splitRight),
          "active reset/partition equality");
    check(energy(left) > .001, "real processed energy");
    for (int stage = 0; stage < 4; ++stage) {
        const darkvoice::ColourParameters p{stage % 2, .2f * stage, 2000.f + 3000 * stage,
                                            .2f * stage};
        check(fx.setParameters(p) && other.setParameters(p), "automation set");
        left = signal(1024);
        right = left;
        split = left;
        splitRight = right;
        check(render(fx, left, channels == 2 ? &right : nullptr, 257) &&
                  render(other, split, channels == 2 ? &splitRight : nullptr, 11) &&
                  left == split && (channels == 1 || right == splitRight),
              "automated four controls partition invariant");
    }
    check(!fx.setParameters({3, .5f, 8000, .5f}) && !fx.process(std::span<float *>{}, 1),
          "invalid warmed calls reject");
    left = signal(1024);
    right = left;
    split = left;
    splitRight = right;
    check(render(fx, left, channels == 2 ? &right : nullptr, 257) &&
              render(other, split, channels == 2 ? &splitRight : nullptr, 13) && left == split &&
              (channels == 1 || right == splitRight),
          "invalid calls preserve warmed histories");
    fx.reset();
    std::array<float, 257> block{};
    std::array<float *, 2> ptr{block.data(), nullptr};
    std::array<float, 257> second{};
    ptr[1] = second.data();
    const auto before = allocations.load();
    reject = true;
    bool safe = fx.setParameters({0, 1, 20000, 1});
    for (int i = 0; i < 8; ++i)
        safe = safe && fx.process(std::span<float *>(ptr.data(), channels), 257);
    fx.reset();
    reject = false;
    check(safe && allocations.load() == before, "process/set/reset allocate nothing");
    check(fx.setParameters({1, 1, 1000, 1}), "extreme setup");
    fx.reset();
    left.assign(2048, std::numeric_limits<float>::max());
    right = left;
    left[0] = std::numeric_limits<float>::quiet_NaN();
    left[1] = std::numeric_limits<float>::infinity();
    left[2] = std::numeric_limits<float>::denorm_min();
    check(render(fx, left, channels == 2 ? &right : nullptr, 257) &&
              std::all_of(left.begin(), left.end(), [](float x) { return std::isfinite(x); }) &&
              std::all_of(right.begin(), right.end(), [](float x) { return std::isfinite(x); }),
          "hostile floats remain finite");
    if (channels == 2) {
        fx.reset();
        left = signal(2048);
        right.assign(left.size(), 0);
        check(render(fx, left, &right, 113) &&
                  std::all_of(right.begin(), right.end(), [](float value) { return value == 0; }),
              "independent channels preserve silent opposite channel");
        fx.reset();
        left = signal(2048);
        right = left;
        check(render(fx, left, &right, 257) && left == right,
              "identical dualmono has coherent channel processing");
    }
}
} // namespace
#if defined(__GNUC__) || defined(__clang__)
#define DV_NOINLINE __attribute__((noinline))
#else
#define DV_NOINLINE
#endif
DV_NOINLINE void *operator new(std::size_t size) {
    ++allocations;
    if (reject)
        throw std::bad_alloc();
    if (void *value = std::malloc(size == 0 ? 1 : size))
        return value;
    throw std::bad_alloc();
}
DV_NOINLINE void *operator new[](std::size_t size) {
    return ::operator new(size);
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
#undef DV_NOINLINE
int main() {
    validation();
    filters();
    for (double rate : {44100., 48000., 88200., 96000.})
        for (std::size_t channels : {1U, 2U})
            behavior(rate, channels);
    for (double rate : {44100., 48000., 88200., 96000.})
        alignmentOracle(rate);
    character();
    aliasMatrix();
    benchmark();
    std::printf("Colour checks: %d, failures: %d\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
