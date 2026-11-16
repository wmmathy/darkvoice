#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <darkvoice/Transducer.hpp>
#include <iostream>
#include <limits>
#include <new>
#include <numbers>
#include <vector>
namespace {
thread_local bool callback = false;
std::atomic<unsigned> allocations{0}, deallocations{0};
unsigned checks = 0;
void check(bool good, const char *message) {
    ++checks;
    if (!good) {
        std::cerr << "FAIL " << message << '\n';
        std::exit(1);
    }
}
} // namespace
void *operator new(std::size_t size) {
    if (callback)
        ++allocations;
    if (auto *memory = std::malloc(size == 0 ? 1 : size))
        return memory;
    throw std::bad_alloc();
}
void *operator new[](std::size_t size) {
    return ::operator new(size);
}
void operator delete(void *memory) noexcept {
    if (callback && memory)
        ++deallocations;
    std::free(memory);
}
void operator delete[](void *memory) noexcept {
    ::operator delete(memory);
}
void operator delete(void *memory, std::size_t) noexcept {
    ::operator delete(memory);
}
void operator delete[](void *memory, std::size_t) noexcept {
    ::operator delete(memory);
}
void *operator new(std::size_t size, std::align_val_t alignment) {
    if (callback)
        ++allocations;
    void *memory = nullptr;
    if (posix_memalign(&memory, static_cast<std::size_t>(alignment), size == 0 ? 1 : size) == 0)
        return memory;
    throw std::bad_alloc();
}
void *operator new[](std::size_t size, std::align_val_t alignment) {
    return ::operator new(size, alignment);
}
void operator delete(void *memory, std::align_val_t) noexcept {
    ::operator delete(memory);
}
void operator delete[](void *memory, std::align_val_t alignment) noexcept {
    ::operator delete(memory, alignment);
}
void operator delete(void *memory, std::size_t, std::align_val_t alignment) noexcept {
    ::operator delete(memory, alignment);
}
void operator delete[](void *memory, std::size_t, std::align_val_t alignment) noexcept {
    ::operator delete(memory, alignment);
}
// Route nothrow allocations through the same malloc/free-backed guard pair.
// Otherwise sanitizer-provided nothrow new would be paired with our free-based delete.
void *operator new(std::size_t size, const std::nothrow_t &) noexcept {
    try {
        return ::operator new(size);
    } catch (...) {
        return nullptr;
    }
}
void *operator new[](std::size_t size, const std::nothrow_t &tag) noexcept {
    return ::operator new(size, tag);
}
void operator delete(void *memory, const std::nothrow_t &) noexcept {
    ::operator delete(memory);
}
void operator delete[](void *memory, const std::nothrow_t &tag) noexcept {
    ::operator delete(memory, tag);
}
void *operator new(std::size_t size, std::align_val_t alignment, const std::nothrow_t &) noexcept {
    try {
        return ::operator new(size, alignment);
    } catch (...) {
        return nullptr;
    }
}
void *operator new[](std::size_t size, std::align_val_t alignment,
                     const std::nothrow_t &tag) noexcept {
    return ::operator new(size, alignment, tag);
}
void operator delete(void *memory, std::align_val_t alignment, const std::nothrow_t &) noexcept {
    ::operator delete(memory, alignment);
}
void operator delete[](void *memory, std::align_val_t alignment,
                       const std::nothrow_t &tag) noexcept {
    ::operator delete(memory, alignment, tag);
}
namespace {
using namespace darkvoice;
using Complex = std::complex<double>;
constexpr double pi = std::numbers::pi;
struct Coeff {
    double b0, b1, b2, a1, a2;
};
// Independently formulate the transfer using z and unnormalized cookbook terms.
Coeff coeff(double rate, double f, double q, int type, double db = 0) {
    const double cosine = std::cos(2 * pi * f / rate);
    const double alpha = std::sin(2 * pi * f / rate) / (2 * q);
    const double a = std::exp(db * std::log(10.) / 40);
    const double den = type == 2 ? 1 + alpha / a : 1 + alpha;
    if (type == 2)
        return {(1 + alpha * a) / den, -2 * cosine / den, (1 - alpha * a) / den, -2 * cosine / den,
                (1 - alpha / a) / den};
    const double first = type == 0 ? (1 - cosine) / 2 : (1 + cosine) / 2;
    return {first / den, (type == 0 ? 2 : -2) * first / den, first / den, -2 * cosine / den,
            (1 - alpha) / den};
}
std::array<Coeff, 4> model(double rate, int m) {
    constexpr double a = .541196100146197, b = 1.30656296487638;
    const double q = std::sqrt(.5);
    if (m == 0)
        return {coeff(rate, 300, a, 1), coeff(rate, 300, b, 1), coeff(rate, 3400, a, 0),
                coeff(rate, 3400, b, 0)};
    if (m == 1)
        return {coeff(rate, 450, q, 1), coeff(rate, 1600, 1.2, 2, 6), coeff(rate, 3200, 2, 2, 3),
                coeff(rate, 5500, q, 0)};
    return {coeff(rate, 120, q, 1), coeff(rate, 1200, .6, 2, 3), coeff(rate, 4500, a, 0),
            coeff(rate, 4500, b, 0)};
}
Complex response(double rate, double hz, int m, int skip = -1, bool trim = true) {
    const auto z = std::polar(1., -2 * pi * hz / rate);
    Complex result = m == 1 && trim ? std::pow(10., -.3) : 1;
    const auto sections = model(rate, m);
    for (std::size_t i = 0; i < sections.size(); ++i) {
        if (static_cast<int>(i) == skip)
            continue;
        const auto &s = sections[i];
        result *= (s.b0 + s.b1 * z + s.b2 * z * z) / (1. + s.a1 * z + s.a2 * z * z);
    }
    return result;
}
void run(Transducer &fx, std::vector<float> &a, std::vector<float> *b = nullptr,
         std::size_t partition = 257) {
    for (std::size_t start = 0; start < a.size();) {
        const auto frames = std::min(partition, a.size() - start);
        std::array<float *, 2> pointers{a.data() + start, b ? b->data() + start : nullptr};
        callback = true;
        const bool ok = fx.process(std::span<float *>(pointers.data(), b ? 2 : 1), frames);
        callback = false;
        check(ok, "valid processing");
        start += frames;
    }
}
std::vector<float> signal(std::size_t count, double rate) {
    std::vector<float> x(count);
    for (std::size_t i = 0; i < count; ++i)
        x[i] = static_cast<float>(.2 * std::sin(2 * pi * 719 * i / rate) +
                                  .1 * std::sin(2 * pi * 1597 * i / rate));
    return x;
}
void frequencyTests() {
    for (double rate : {44100., 48000., 88200., 96000.}) {
        for (std::size_t channels : {1u, 2u})
            for (int m = 0; m < 3; ++m) {
                double maximumError = 0;
                unsigned negatives = 0;
                for (const auto &s : model(rate, m)) {
                    const auto d = std::sqrt(Complex(s.a1 * s.a1 - 4 * s.a2, 0));
                    check(std::abs((-s.a1 + d) / 2.) < 1 && std::abs((-s.a1 - d) / 2.) < 1,
                          "stable analytic poles");
                }
                for (double f : {100., 120., 300., 450., 900., 1200., 1600., 2400., 3200., 3400.,
                                 4500., 5500., 10000.}) {
                    // All frequencies integral: one-second measurement is coherent.
                    const auto warm = static_cast<std::size_t>(rate);
                    auto x = std::vector<float>(2 * warm);
                    for (std::size_t n = 0; n < x.size(); ++n)
                        x[n] = static_cast<float>(.25 * std::sin(2 * pi * f * n / rate));
                    auto dry = x, right = x;
                    for (auto &sample : right)
                        sample = -sample;
                    Transducer fx;
                    check(fx.prepare(rate, 257, channels) && fx.setParameters({m, 1}),
                          "response prepare");
                    fx.reset();
                    run(fx, x, channels == 2 ? &right : nullptr);
                    if (channels == 2)
                        for (std::size_t n = 0; n < x.size(); ++n)
                            check(right[n] == -x[n], "stereo anti-phase independent response");
                    Complex in{}, out{};
                    for (std::size_t n = warm; n < x.size(); ++n) {
                        const auto basis = std::polar(1., -2 * pi * f * n / rate);
                        in += static_cast<double>(dry[n]) * basis;
                        out += static_cast<double>(x[n]) * basis;
                    }
                    const auto actual = out / in, expected = response(rate, f, m);
                    maximumError = std::max(maximumError, std::abs(actual - expected));
                    check(std::abs(actual - expected) <= 2e-5, "complex frequency response");
                    if (std::abs(expected - Complex(1)) > 2e-5)
                        ++negatives;
                    check(std::abs(expected - response(rate, f, (m + 1) % 3)) > 2e-5,
                          "swapped model negative");
                }
                check(negatives == 13, "identity negatives");
                if (m == 1) {
                    check(std::abs(response(rate, 1600, m) - response(rate, 1600, m, 1)) > 2e-5,
                          "first peak negative");
                    check(std::abs(response(rate, 3200, m) - response(rate, 3200, m, 2)) > 2e-5,
                          "second peak negative");
                    check(std::abs(response(rate, 1600, m) - response(rate, 1600, m, -1, false)) >
                              2e-5,
                          "trim negative");
                }
                if (m == 2)
                    check(std::abs(response(rate, 1200, m) - response(rate, 1200, m, 1)) > 2e-5,
                          "radio emphasis omission negative");
                std::cout << "response rate=" << rate << " channels=" << channels << " model=" << m
                          << " maxComplexError=" << maximumError << '\n';
            }
        check(std::abs(response(rate, 100, 0)) < .013, "telephone bass rejection");
        check(std::abs(response(rate, 10000, 0)) < .014, "telephone treble rejection");
    }
}
void purityTests() {
    constexpr std::size_t count = 16384;
    for (double rate : {44100., 48000., 88200., 96000.})
        for (int tones : {1, 2})
            for (int m = 0; m < 3; ++m) {
                const auto warm = ((static_cast<std::size_t>(rate) + count - 1) / count) * count;
                std::vector<float> x(warm + count), clipped(warm + count);
                for (std::size_t n = 0; n < x.size(); ++n) {
                    x[n] = static_cast<float>(
                        .3 * std::sin(2 * pi * 313 * n / count) +
                        (tones == 2 ? .3 * std::sin(2 * pi * 877 * n / count) : 0));
                    clipped[n] = std::clamp(x[n], -.18f, .18f);
                }
                const auto original = x;
                Transducer fx;
                check(fx.prepare(rate, 257, 1) && fx.setParameters({m, 1}), "purity prepare");
                fx.reset();
                run(fx, x);
                // Orthogonally project both input bins; all other residual energy is new.
                auto residual = [&](const std::vector<float> &data) {
                    std::array<Complex, 2> bins{};
                    for (std::size_t n = 0; n < count; ++n)
                        for (std::size_t j = 0; j < 2; ++j)
                            bins[j] += static_cast<double>(data[warm + n]) *
                                       std::polar(1., -2 * pi * (j ? 877 : 313) * n / count);
                    double energy = 0;
                    for (std::size_t n = 0; n < count; ++n) {
                        double fit = 0;
                        for (std::size_t j = 0; j < 2; ++j)
                            fit += 2 *
                                   std::real(bins[j] *
                                             std::polar(1., 2 * pi * (j ? 877 : 313) * n / count)) /
                                   count;
                        const double delta = data[warm + n] - fit;
                        energy += delta * delta;
                    }
                    return energy / count;
                };
                double energy = 0;
                for (std::size_t n = warm; n < original.size(); ++n)
                    energy += original[n] * original[n];
                energy /= count;
                const double db = 10 * std::log10(residual(x) / energy);
                check(db <= -100, "linear harmonic/intermod purity");
                check(10 * std::log10(residual(clipped) / energy) > -100,
                      "clipping purity negative");
                std::cout << "purity rate=" << rate << " tones=" << tones << " model=" << m
                          << " residualDb=" << db << '\n';
            }
}
struct Oracle {
    std::array<std::array<Coeff, 4>, 3> coefficients;
    struct State {
        double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    };
    std::array<std::array<State, 4>, 3> states{};
    std::array<double, 3> weights{1, 0, 0}, from{1, 0, 0};
    int targetModel = 0;
    float targetMix = 0;
    double mix = 0, mixFrom = 0;
    std::size_t modelPosition = 0, mixPosition = 0, modelLength, mixLength;
    explicit Oracle(double rate)
        : modelLength(static_cast<std::size_t>(std::llround(rate * .020))),
          mixLength(static_cast<std::size_t>(std::llround(rate * .010))) {
        for (int m = 0; m < 3; ++m)
            coefficients[m] = model(rate, m);
    }
    void set(int m, float value) {
        if (m != targetModel) {
            from = weights;
            targetModel = m;
            modelPosition = 0;
        }
        if (value != targetMix) {
            mixFrom = mix;
            targetMix = value;
            mixPosition = 0;
        }
    }
    double tick(double x) {
        if (modelPosition < modelLength) {
            ++modelPosition;
            for (int m = 0; m < 3; ++m)
                weights[m] = from[m] + ((m == targetModel ? 1. : 0.) - from[m]) *
                                           static_cast<double>(modelPosition) / modelLength;
        }
        if (mixPosition < mixLength) {
            ++mixPosition;
            mix = mixFrom + (targetMix - mixFrom) * static_cast<double>(mixPosition) / mixLength;
        }
        double wet = 0;
        for (int m = 0; m < 3; ++m) {
            double y = x;
            for (int j = 0; j < 4; ++j) {
                auto &s = states[m][j];
                const auto &c = coefficients[m][j];
                const double next =
                    c.b0 * y + c.b1 * s.x1 + c.b2 * s.x2 - c.a1 * s.y1 - c.a2 * s.y2;
                s.x2 = s.x1;
                s.x1 = y;
                s.y2 = s.y1;
                s.y1 = next;
                y = next;
            }
            wet += weights[m] * y * (m == 1 ? std::pow(10., -.3) : 1);
        }
        return x + (wet - x) * mix;
    }
};
void lifecycleTests() {
    for (double rate : {44100., 48000., 88200., 96000.})
        for (std::size_t channels : {1u, 2u}) {
            Transducer a, b;
            check(a.setParameters({2, .7f}), "preprepare set");
            check(a.prepare(rate, 257, channels) && b.prepare(rate, 257, channels),
                  "lifecycle prepare");
            check(a.latencySamples() == 0, "latency");
            auto left = signal(4096, rate), right = left, neutral = left;
            if (channels == 2)
                run(a, left, &right);
            else
                run(a, left);
            check(left == neutral, "prepare resets target / exact neutral");
            check(a.setParameters({1, .8f}) && b.setParameters({1, .8f}), "targets");
            a.reset();
            b.reset();
            left = signal(9000, rate);
            right = left;
            auto expected = left, expectedR = right;
            if (channels == 2) {
                run(a, left, &right, 257);
                run(b, expected, &expectedR, 73);
            } else {
                run(a, left, nullptr, 257);
                run(b, expected, nullptr, 73);
            }
            check(left == expected && right == expectedR, "exact partition equivalence");
            for (auto p : {TransducerParameters{-1, .5f}, TransducerParameters{3, .5f},
                           TransducerParameters{1, -.1f}, TransducerParameters{1, 1.1f},
                           TransducerParameters{1, std::numeric_limits<float>::quiet_NaN()}})
                check(!a.setParameters(p), "invalid snapshot rejection");
            std::array<float, 600> buf{};
            std::array<float *, 2> ptr{buf.data(), buf.data() + 1};
            check(!a.process(std::span<float *>(ptr.data(), channels), 258), "oversize reject");
            if (channels == 2)
                check(!a.process(ptr, 100), "partial overlap reject");
            check(!a.process(std::span<float *>(ptr.data(), channels == 1 ? 2 : 1), 1),
                  "wrong channel count rejection");
            ptr[0] = nullptr;
            check(!a.process(std::span<float *>(ptr.data(), channels), 10), "null reject");
            check(a.process(std::span<float *>(ptr.data(), channels), 0), "empty null freezes");
            left = signal(2048, rate);
            right = left;
            expected = left;
            expectedR = right;
            if (channels == 2) {
                run(a, left, &right);
                run(b, expected, &expectedR);
            } else {
                run(a, left);
                run(b, expected);
            }
            check(left == expected && right == expectedR,
                  "invalid and zero calls warmed state atomicity");
            a.reset();
            b.reset();
            left = signal(2048, rate);
            right = left;
            expected = left;
            expectedR = right;
            if (channels == 2) {
                run(a, left, &right);
                run(b, expected, &expectedR);
            } else {
                run(a, left);
                run(b, expected);
            }
            check(left == expected && right == expectedR, "reset determinism");
            if (channels == 2) {
                a.reset();
                left = signal(2048, rate);
                right.assign(left.size(), 0);
                run(a, left, &right);
                check(std::all_of(right.begin(), right.end(), [](float x) { return x == 0; }),
                      "stereo isolation");
            }
            for (float special :
                 {std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(),
                  std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN(),
                  std::numeric_limits<float>::denorm_min()}) {
                left.assign(4096, special);
                right = left;
                if (channels == 2)
                    run(a, left, &right);
                else
                    run(a, left);
                check(std::all_of(left.begin(), left.end(),
                                  [](float x) {
                                      return std::isfinite(x) && std::fpclassify(x) != FP_SUBNORMAL;
                                  }),
                      "extreme finite output");
            }
            check(!a.prepare(123, 257, channels), "bad rate reject");
            ptr[0] = buf.data();
            check(!a.process(std::span<float *>(ptr.data(), channels), 1),
                  "failed prepare unprepared");
            check(!a.prepare(rate, 0, channels) && !a.prepare(rate, 65537, channels) &&
                      !a.prepare(rate, 257, 3),
                  "invalid prepare limits");
        }
    for (double rate : {44100., 48000., 88200., 96000.}) {
        Transducer fx;
        check(fx.prepare(rate, 1, 1), "automation prepare");
        Oracle oracle(rate);
        double maximum = 0;
        for (std::size_t n = 0; n < 12000; ++n) {
            if (n % 719 == 0) {
                const int m = static_cast<int>((n / 719) % 3);
                const float mix = static_cast<float>((n / 719) % 5) / 4;
                check(fx.setParameters({m, mix}), "automation targets");
                oracle.set(m, mix);
            }
            float sample = static_cast<float>(.2 * std::sin(2 * pi * 719 * n / rate) +
                                              .05 * std::sin(2 * pi * 2123 * n / rate));
            const double expected = oracle.tick(sample);
            std::array<float *, 1> ptr{&sample};
            callback = true;
            const bool ok = fx.process(ptr, 1);
            callback = false;
            check(ok, "automation process");
            maximum = std::max(maximum, std::abs(sample - expected));
        }
        check(maximum < 2e-7, "independent scalar model/mix ramp oracle");
        std::cout << "automation rate=" << rate << " maxError=" << maximum << '\n';
    }
    Transducer fx;
    check(fx.prepare(48000, 4, 1), "signed zero prepare");
    std::array<float, 4> bits{-0.f, 0.f, -.25f, .25f};
    const auto original = bits;
    std::array<float *, 1> ptr{bits.data()};
    check(fx.process(ptr, 4), "signed zero run");
    for (std::size_t i = 0; i < 4; ++i)
        check(std::bit_cast<unsigned>(bits[i]) == std::bit_cast<unsigned>(original[i]),
              "neutral bits");
}
void distinctions() {
    for (double rate : {44100., 48000., 88200., 96000.}) {
        std::array<std::vector<float>, 3> outputs;
        auto dry = signal(8192, rate);
        unsigned random = 0x9137;
        for (auto &sample : dry) {
            random ^= random << 13;
            random ^= random >> 17;
            random ^= random << 5;
            sample = static_cast<float>(static_cast<double>(random) / 4294967296. - .5);
        }
        for (int m = 0; m < 3; ++m) {
            Transducer fx;
            check(fx.prepare(rate, 257, 1) && fx.setParameters({m, 1}), "distinct prepare");
            fx.reset();
            outputs[m] = dry;
            run(fx, outputs[m]);
            double difference = 0;
            for (std::size_t i = 0; i < dry.size(); ++i)
                difference += std::pow(outputs[m][i] - dry[i], 2);
            check(difference > 1, "audible fixture difference from dry");
        }
        for (int m = 0; m < 3; ++m) {
            double difference = 0;
            for (std::size_t i = 0; i < dry.size(); ++i)
                difference += std::pow(outputs[m][i] - outputs[(m + 1) % 3][i], 2);
            check(difference > 1, "model distinctions");
        }
    }
}
void allocationProbeControls() {
    callback = true;
    void *a = ::operator new(8);
    ::operator delete(a);
    a = ::operator new[](8);
    ::operator delete[](a);
    a = ::operator new(64, std::align_val_t{64});
    ::operator delete(a, std::align_val_t{64});
    a = ::operator new(8, std::nothrow);
    ::operator delete(a, std::nothrow);
    a = ::operator new[](8, std::nothrow);
    ::operator delete[](a, std::nothrow);
    a = ::operator new(64, std::align_val_t{64}, std::nothrow);
    ::operator delete(a, std::align_val_t{64}, std::nothrow);
    a = ::operator new[](64, std::align_val_t{64}, std::nothrow);
    ::operator delete[](a, std::align_val_t{64}, std::nothrow);
    callback = false;
    check(allocations.load() == 7 && deallocations.load() == 7,
          "allocation guard positive controls");
    allocations = 0;
    deallocations = 0;
}
} // namespace
int main() {
    allocationProbeControls();
    frequencyTests();
    purityTests();
    lifecycleTests();
    distinctions();
    check(allocations.load() == 0 && deallocations.load() == 0,
          "no callback allocation/deallocation");
    std::cout << checks << " checks, 0 failures\n";
}
