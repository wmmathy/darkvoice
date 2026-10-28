#include <darkvoice/VoiceSculptor.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cfenv>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <numbers>
#include <vector>

namespace {
using darkvoice::VoiceSculptor;
using Parameters = darkvoice::VoiceSculptorParameters;
std::atomic<std::size_t> allocations{0};
std::atomic<bool> rejectAllocation{false};
int checks = 0, failures = 0;
void check(bool condition, const char *message) {
    ++checks;
    if (!condition) {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", message);
    }
}
bool render(VoiceSculptor &fx, std::vector<float> &left, std::vector<float> *right,
            std::size_t block, bool irregular = false) {
    std::size_t partition = 0;
    constexpr std::array<std::size_t, 7> sizes{1, 13, 257, 2, 71, 19, 128};
    for (std::size_t at = 0; at < left.size();) {
        const auto count =
            std::min(left.size() - at, irregular ? sizes[partition++ % sizes.size()] : block);
        std::array<float *, 2> pointers{left.data() + at, right ? right->data() + at : nullptr};
        if (!fx.process(std::span<float *>(pointers.data(), right ? 2U : 1U), count))
            return false;
        at += count;
    }
    return true;
}
std::vector<float> source(std::size_t count) {
    std::vector<float> result(count);
    for (std::size_t i = 0; i < count; ++i)
        result[i] = static_cast<float>(.17 * std::sin(.037 * i) + .11 * std::cos(.211 * i));
    return result;
}
bool close(const std::vector<float> &a, const std::vector<float> &b, double tolerance) {
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (!std::isfinite(a[i]) || std::abs(static_cast<double>(a[i]) - b[i]) > tolerance)
            return false;
    return true;
}

// Independent frequency oracle: analog transfer functions evaluated through the
// bilinear frequency mapping, not the production RBJ coefficient recurrence.
std::complex<double> transfer(double rate, double frequency, Parameters p) {
    const auto band = [rate, frequency](double center, double q) {
        const double u = std::tan(std::numbers::pi * frequency / rate) /
                         std::tan(std::numbers::pi * center / rate);
        const std::complex<double> v(0, u / q);
        return v / (1 - u * u + v);
    };
    const double u =
        std::tan(std::numbers::pi * frequency / rate) / std::tan(std::numbers::pi * 450 / rate);
    const auto low = 1.0 / std::complex<double>(1, u);
    const double a = std::pow(10., p.warmthDb / 40.), b = std::pow(10., -p.warmthDb / 40.);
    const double depthU =
        std::tan(std::numbers::pi * frequency / rate) / std::tan(std::numbers::pi * 90 / rate);
    const auto depthLow = 1.0 / std::complex<double>(1, depthU);
    return (1.0 + (std::pow(10., p.depthDb / 20.) - 1) * depthLow) *
           (1.0 + (std::pow(10., p.bodyDb / 20.) - 1) * band(180, .85)) * (b + (a - b) * low) *
           (1.0 + (std::pow(10., p.resonanceDb / 20.) - 1) * band(700, 1.6)) *
           (1.0 + (std::pow(10., p.presenceDb / 20.) - 1) * band(3000, .7));
}
std::complex<double> fit(const std::vector<float> &samples, double rate, double frequency) {
    double ss = 0, cc = 0, sc = 0, ys = 0, yc = 0;
    for (std::size_t i = samples.size() / 2; i < samples.size(); ++i) {
        const double phase = 2 * std::numbers::pi * frequency * i / rate;
        const double s = std::sin(phase), c = std::cos(phase);
        ss += s * s;
        cc += c * c;
        sc += s * c;
        ys += samples[i] * s;
        yc += samples[i] * c;
    }
    const double determinant = ss * cc - sc * sc;
    return {(ys * cc - yc * sc) / determinant / .1, (yc * ss - ys * sc) / determinant / .1};
}

void spectral(double rate, std::size_t channels) {
    VoiceSculptor fx;
    check(fx.prepare(rate, 257, channels), "spectral preparation");
    std::vector<Parameters> settings{{}};
    for (float gain : {-6.f, -3.f, 3.f, 6.f}) {
        settings.push_back({gain, 0, 0});
        settings.push_back({0, gain, 0});
        settings.push_back({0, 0, gain});
    }
    for (float body : {-6.f, 6.f})
        for (float warmth : {-6.f, 6.f})
            for (float presence : {-6.f, 6.f})
                settings.push_back({body, warmth, presence});
    for (const auto p : settings) {
        check(fx.setParameters(p), "accepted spectral setting");
        for (double frequency : {70., 180., 450., 1000., 3000., 8000., 16000.}) {
            fx.reset();
            std::vector<float> left(8192), right;
            for (std::size_t i = 0; i < left.size(); ++i)
                left[i] =
                    static_cast<float>(.1 * std::sin(2 * std::numbers::pi * frequency * i / rate));
            if (channels == 2)
                right = left;
            check(render(fx, left, channels == 2 ? &right : nullptr, 257), "spectral render");
            const auto measured = fit(left, rate, frequency),
                       expected = transfer(rate, frequency, p);
            const double errorDb = 20 * std::log10(std::abs(measured) / std::abs(expected));
            check(std::abs(errorDb) < .02 && std::abs(measured - expected) < 2e-5,
                  "independent complex transfer and .02dB magnitude gate");
            if (channels == 2)
                check(left == right, "spectral dualmono coherence");
        }
    }
    for (float gain : {-6.f, -3.f, 3.f, 6.f}) {
        check(std::abs(20 * std::log10(std::abs(transfer(rate, 180, {gain, 0, 0}))) - gain) < 1e-10,
              "analytic body center gain");
        check(std::abs(20 * std::log10(std::abs(transfer(rate, 3000, {0, 0, gain}))) - gain) <
                  1e-10,
              "analytic presence center gain");
        for (bool nyquist : {false, true}) {
            check(fx.setParameters({0, gain, 0}), "tilt endpoint setting");
            fx.reset();
            std::vector<float> left(4096, .1f), right;
            if (nyquist)
                for (std::size_t i = 1; i < left.size(); i += 2)
                    left[i] = -.1f;
            if (channels == 2)
                right = left;
            check(render(fx, left, channels == 2 ? &right : nullptr, 257), "DC/Nyquist render");
            const double measured = 20 * std::log10(std::abs(left.back()) / .1);
            check(std::abs(measured - (nyquist ? -.5 : .5) * gain) < .002,
                  "measured tilt exact asymptotic endpoints");
        }
    }
    bool bound = true;
    for (float body : {-6.f, 6.f})
        for (float warmth : {-6.f, 6.f})
            for (float presence : {-6.f, 6.f})
                for (int i = 1; i < 2048; ++i)
                    bound = bound &&
                            std::abs(transfer(rate, rate * i / 4096, {body, warmth, presence})) <=
                                std::pow(10., 15. / 20.) + 1e-12;
    check(bound, "static cascade magnitude remains below analytic +15dB bound");
    check(std::abs(transfer(rate, 180, {6, 0, 0})) > std::abs(transfer(rate, 180, {0, 6, 0})) &&
              std::abs(transfer(rate, 3000, {0, 0, 6})) >
                  std::abs(transfer(rate, 3000, {6, 0, 0})) &&
              std::abs(transfer(rate, 16000, {0, 6, 0})) < .8,
          "body warmth and presence have distinct spectral operations");
}

// V2 response proof uses the same independent analog/bilinear oracle, not
// production coefficient formulas. All32 five-control endpoint combinations are
// included, plus isolated endpoint/intermediate settings for both new controls.
void spectralV2(double rate, std::size_t channels) {
    VoiceSculptor fx;
    check(fx.prepare(rate, 257, channels), "v2 spectral preparation");
    std::vector<Parameters> settings;
    for (float gain : {-6.f, -3.f, 3.f, 6.f}) {
        settings.push_back({0, 0, 0, gain, 0});
        settings.push_back({0, 0, 0, 0, gain});
    }
    for (unsigned bits = 0; bits < 32; ++bits) {
        const auto gain = [bits](unsigned bit) { return bits & (1U << bit) ? 6.f : -6.f; };
        settings.push_back({gain(0), gain(1), gain(2), gain(3), gain(4)});
    }
    for (const auto p : settings) {
        check(fx.setParameters(p), "v2 spectral target accepted");
        for (double frequency : {70., 90., 180., 450., 700., 3000., 16000.}) {
            fx.reset();
            std::vector<float> left(8192), right;
            for (std::size_t i = 0; i < left.size(); ++i)
                left[i] =
                    static_cast<float>(.1 * std::sin(2 * std::numbers::pi * frequency * i / rate));
            if (channels == 2)
                right = left;
            check(render(fx, left, channels == 2 ? &right : nullptr, 257), "v2 spectral render");
            const auto measured = fit(left, rate, frequency),
                       expected = transfer(rate, frequency, p);
            check(std::abs(20 * std::log10(std::abs(measured) / std::abs(expected))) < .02 &&
                      std::abs(measured - expected) < 2e-5,
                  "v2 independent complex transfer and unchanged .02dB/2e-5 gates");
            if (channels == 2)
                check(left == right, "v2 dualmono spectral identity");
        }
    }
    for (float gain : {-6.f, -3.f, 3.f, 6.f}) {
        check(std::abs(20 * std::log10(std::abs(transfer(rate, 700, {0, 0, 0, 0, gain}))) - gain) <
                  1e-10,
              "analytic resonance exact700Hz gain");
        for (bool nyquist : {false, true}) {
            check(fx.setParameters({0, 0, 0, gain, 0}), "depth endpoint target");
            fx.reset();
            std::vector<float> left(4096, .1f), right;
            if (nyquist)
                for (std::size_t i = 1; i < left.size(); i += 2)
                    left[i] = -.1f;
            if (channels == 2)
                right = left;
            check(render(fx, left, channels == 2 ? &right : nullptr, 257),
                  "depth DC/Nyquist render");
            check(std::abs(20 * std::log10(std::abs(left.back()) / .1) - (nyquist ? 0 : gain)) <
                      .002,
                  "depth requested low-frequency endpoint and unity high endpoint");
        }
    }
    bool bound = true;
    for (unsigned bits = 0; bits < 32; ++bits) {
        const auto gain = [bits](unsigned bit) { return bits & (1U << bit) ? 6.f : -6.f; };
        const Parameters p{gain(0), gain(1), gain(2), gain(3), gain(4)};
        for (int i = 1; i < 2048; ++i)
            bound &=
                std::abs(transfer(rate, rate * i / 4096, p)) <= std::pow(10., 27. / 20.) + 1e-12;
    }
    check(bound, "v2 five-stage static magnitude bounded by conservative27dB");
    check(std::abs(transfer(rate, 70, {0, 0, 0, 6, 0})) > std::abs(transfer(rate, 70, {6, 0, 0})) &&
              std::abs(transfer(rate, 700, {0, 0, 0, 0, 6})) >
                  std::abs(transfer(rate, 700, {6, 0, 0})) &&
              std::abs(transfer(rate, 16000, {0, 6, 0})) <
                  std::abs(transfer(rate, 16000, {0, 0, 0, 6, 0})),
          "depth resonance and existing tone stages have distinct spectral responses");
}

// Independent time-domain oracle: trapezoidal-integrator state-variable filters,
// unlike production transposed direct form II. Smoothed weights use a closed
// exponential envelope rather than the production iterative update.
struct Reference {
    struct Band {
        long double s1 = 0, s2 = 0;
        long double process(long double x, long double g, long double q) {
            const auto r = 1 / (2 * q);
            const auto high = (x - (2 * r + g) * s1 - s2) / (1 + 2 * r * g + g * g);
            const auto band = g * high + s1, low = g * band + s2;
            s1 = g * high + band;
            s2 = g * band + low;
            return 2 * r * band;
        }
    };
    std::array<Band, 3> bands;
    long double lowState = 0;
    double rate;
    std::array<long double, 6> start{0, 1, 1, 0, 0, 0}, target{0, 1, 1, 0, 0, 0};
    std::size_t age = 0;
    long double depthState = 0;
    std::array<long double, 6> weights() const {
        auto values = target;
        const auto decay = std::exp(-static_cast<long double>(age) / (.010L * rate));
        for (std::size_t i = 0; i < values.size(); ++i)
            values[i] += (start[i] - target[i]) * decay;
        return values;
    }
    void set(Parameters p, bool settle = false) {
        start = weights();
        target = {std::pow(10.L, p.bodyDb / 20.L) - 1,  std::pow(10.L, p.warmthDb / 40.L),
                  std::pow(10.L, -p.warmthDb / 40.L),   std::pow(10.L, p.presenceDb / 20.L) - 1,
                  std::pow(10.L, p.depthDb / 20.L) - 1, std::pow(10.L, p.resonanceDb / 20.L) - 1};
        if (settle)
            start = target;
        age = 0;
    }
    float process(float input) {
        ++age;
        const auto w = weights();
        long double x = input;
        const auto kd = std::tan(std::numbers::pi_v<long double> * 90 / rate);
        const auto depth = (kd * x + depthState) / (1 + kd);
        depthState = 2 * depth - depthState;
        x += w[4] * depth;
        const auto body =
            bands[0].process(x, std::tan(std::numbers::pi_v<long double> * 180 / rate), .85L);
        x += w[0] * body;
        const auto k = std::tan(std::numbers::pi_v<long double> * 450 / rate);
        const auto low = (k * x + lowState) / (1 + k);
        lowState = 2 * low - lowState;
        x = w[2] * x + (w[1] - w[2]) * low;
        const auto resonance =
            bands[2].process(x, std::tan(std::numbers::pi_v<long double> * 700 / rate), 1.6L);
        x += w[5] * resonance;
        const auto presence =
            bands[1].process(x, std::tan(std::numbers::pi_v<long double> * 3000 / rate), .7L);
        return static_cast<float>(x + w[3] * presence);
    }
};

void automation(double rate, std::size_t channels) {
    for (const auto setting :
         {Parameters{6, 0, 0}, Parameters{0, -6, 0}, Parameters{0, 0, 6}, Parameters{6, -3, -6},
          Parameters{0, 0, 0, 6, 0}, Parameters{0, 0, 0, -6, 0}, Parameters{0, 0, 0, 0, 6},
          Parameters{0, 0, 0, 0, -6}, Parameters{3, -4, 6, -5, 2},
          Parameters{-3, 6, -5, 3.7f, -2.3f}}) {
        VoiceSculptor fx, split;
        check(fx.prepare(rate, 257, channels) && split.prepare(rate, 257, channels),
              "automation preparation");
        Reference oracle{{}, 0, rate};
        for (const auto target : {Parameters{}, setting, Parameters{-3, 6, 3}, Parameters{}}) {
            check(fx.setParameters(target) && split.setParameters(target),
                  "automation accepted target");
            oracle.set(target);
            auto left = source(2048), original = left, right = left, divided = left,
                 dividedRight = right;
            std::vector<float> expected(left.size());
            for (std::size_t i = 0; i < original.size(); ++i)
                expected[i] = oracle.process(original[i]);
            check(render(fx, left, channels == 2 ? &right : nullptr, 257) &&
                      render(split, divided, channels == 2 ? &dividedRight : nullptr, 257, true),
                  "automation irregular renders");
            check(close(left, expected, 2e-7),
                  "combined trapezoidal oracle with closed 10ms weight envelope");
            check(left == divided && (channels == 1 || (right == dividedRight && right == left)),
                  "automation exact partition invariance and shared weight timing");
        }
    }
}

void behavior(double rate, std::size_t channels) {
    VoiceSculptor fx, paired;
    check(fx.prepare(rate, 257, channels) && paired.prepare(rate, 257, channels),
          "behavior preparation");
    check(fx.latencySamples() == 0, "zero algorithmic latency");
    std::vector<float> left{0.f,
                            -0.f,
                            std::numeric_limits<float>::min(),
                            -std::numeric_limits<float>::min(),
                            std::numeric_limits<float>::max(),
                            -std::numeric_limits<float>::max(),
                            .125f,
                            -.9f,
                            9e-22f};
    const auto original = left;
    auto right = left;
    check(render(fx, left, channels == 2 ? &right : nullptr, 257), "neutral edges render");
    bool exact = true;
    for (std::size_t i = 0; i < left.size(); ++i)
        exact = exact &&
                std::bit_cast<std::uint32_t>(left[i]) == std::bit_cast<std::uint32_t>(original[i]);
    check(exact && (channels == 1 || left == right),
          "neutral preserves normal float bits and signedzero");
    fx.reset();
    left = {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
            -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::denorm_min(),
            -std::numeric_limits<float>::denorm_min()};
    right = left;
    check(render(fx, left, channels == 2 ? &right : nullptr, 257) &&
              std::all_of(left.begin(), left.end(),
                          [](float x) { return x == 0 && std::isfinite(x); }),
          "nonfinite and true float subnormal sanitized");
    check(fx.setParameters({0, -6, 0}), "normal-boundary warmth cut target");
    for (float sign : {-1.f, 1.f}) {
        fx.reset();
        left.assign(4096, sign * std::numeric_limits<float>::min());
        right = left;
        check(render(fx, left, channels == 2 ? &right : nullptr, 257) &&
                  std::all_of(left.end() - 512, left.end(), [](float x) { return x == 0; }) &&
                  (channels == 1 ||
                   std::all_of(right.end() - 512, right.end(), [](float x) { return x == 0; })) &&
                  std::fpclassify(left.front()) == FP_NORMAL,
              "active warmth cut flushes only emitted float subnormals after settling");
    }
    check(fx.setParameters({6, 6, 6}) && paired.setParameters({6, 6, 6}), "behavior target");
    fx.reset();
    paired.reset();
    left = source(4096);
    auto other = left;
    right.assign(left.size(), 0);
    auto otherRight = right;
    check(render(fx, left, channels == 2 ? &right : nullptr, 257) &&
              render(paired, other, channels == 2 ? &otherRight : nullptr, 257),
          "warm histories");
    check(channels == 1 || std::all_of(right.begin(), right.end(), [](float x) { return x == 0; }),
          "silent channel isolation");
    std::array<float, 258> block{}, second{};
    block.fill(.3f);
    auto saved = block;
    std::array<float *, 2> pointers{block.data(), second.data()};
    check(!fx.process(std::span<float *>(pointers.data(), channels), 258) && block == saved,
          "oversized call is atomic");
    check(!fx.process(std::span<float *>(pointers.data(), channels == 1 ? 2U : 1U), 17),
          "wrong channel count rejected");
    pointers[channels - 1] = nullptr;
    check(!fx.process(std::span<float *>(pointers.data(), channels), 17) && block == saved,
          "null nonempty channel rejected atomically");
    pointers = {block.data(), block.data()};
    check(channels == 1 || !fx.process(pointers, 17), "duplicate channels rejected");
    pointers = {block.data(), second.data()};
    check(fx.process(std::span<float *>(pointers.data(), channels), 0),
          "empty valid call accepted");
    for (float bad : {-6.001f, 6.001f, std::numeric_limits<float>::infinity(),
                      std::numeric_limits<float>::quiet_NaN()}) {
        check(!fx.setParameters({bad, 0, 0}) && !fx.setParameters({0, bad, 0}) &&
                  !fx.setParameters({0, 0, bad}),
              "invalid entire snapshots rejected");
    }
    left = source(1024);
    other = left;
    right = left;
    otherRight = right;
    check(render(fx, left, channels == 2 ? &right : nullptr, 257) &&
              render(paired, other, channels == 2 ? &otherRight : nullptr, 17) && left == other &&
              (channels == 1 || right == otherRight),
          "invalid and empty calls preserve targets histories and smoothing");
    fx.reset();
    left = source(1024);
    right = left;
    check(render(fx, left, channels == 2 ? &right : nullptr, 257), "reset first render");
    fx.reset();
    other = source(1024);
    otherRight = other;
    check(render(fx, other, channels == 2 ? &otherRight : nullptr, 13) && left == other,
          "reset reproducibility at accepted target");
    fx.reset();
    left.assign(257, 0);
    left[0] = .1f;
    right = left;
    check(render(fx, left, channels == 2 ? &right : nullptr, 257) && left[0] != 0,
          "active filters have immediate impulse response without added delay");
    fx.reset();
    pointers = {block.data(), second.data()};
    const auto before = allocations.load();
    rejectAllocation = true;
    bool safe = fx.setParameters({-6, -6, -6});
    for (int i = 0; i < 8; ++i)
        safe = safe && fx.process(std::span<float *>(pointers.data(), channels), 257);
    fx.reset();
    rejectAllocation = false;
    check(safe && allocations == before, "process setParameters reset allocate nothing");
    for (float gain : {-6.f, 6.f}) {
        check(fx.setParameters({gain, gain, gain}), "extreme target");
        fx.reset();
        left.assign(2048, std::numeric_limits<float>::max());
        for (std::size_t i = 1; i < left.size(); i += 2)
            left[i] = -left[i];
        right = left;
        check(render(fx, left, channels == 2 ? &right : nullptr, 257) &&
                  std::all_of(left.begin(), left.end(), [](float x) { return std::isfinite(x); }),
              "extreme finite input remains finite");
    }
    // A linear fixed-control module must obey superposition below float clipping.
    check(fx.setParameters({3, -6, 6}), "superposition target");
    auto a = source(2048), b = a, sum = a;
    for (std::size_t i = 0; i < b.size(); ++i) {
        b[i] = static_cast<float>(.08 * std::sin(.071 * i));
        sum[i] = a[i] + b[i];
    }
    for (auto *data : {&a, &b, &sum}) {
        fx.reset();
        auto r = *data;
        check(render(fx, *data, channels == 2 ? &r : nullptr, 257), "superposition render");
    }
    for (std::size_t i = 0; i < sum.size(); ++i)
        a[i] += b[i];
    check(close(a, sum, 3e-7), "static module superposition");
}

void behaviorV2(double rate, std::size_t channels) {
    VoiceSculptor actual, expected;
    const Parameters p{3, -2, 5, 4, -3};
    check(actual.prepare(rate, 257, channels) && expected.prepare(rate, 257, channels) &&
              actual.setParameters(p) && expected.setParameters(p),
          "v2 active lifecycle preparation");
    actual.reset();
    expected.reset();
    auto left = source(2048), right = left, baseline = left, baselineRight = right;
    check(render(actual, left, channels == 2 ? &right : nullptr, 257) &&
              render(expected, baseline, channels == 2 ? &baselineRight : nullptr, 17) &&
              left == baseline && (channels == 1 || right == baselineRight),
          "v2 active controls exact irregular partition reference");
    for (float bad :
         {-6.001f, 6.001f, std::numeric_limits<float>::quiet_NaN(),
          std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()}) {
        auto invalid = p;
        invalid.bodyDb = -6;
        invalid.depthDb = bad;
        check(!actual.setParameters(invalid),
              "v2 invalid depth atomically rejects other target changes");
        invalid = p;
        invalid.warmthDb = 6;
        invalid.resonanceDb = bad;
        check(!actual.setParameters(invalid),
              "v2 invalid resonance atomically rejects other target changes");
    }
    std::array<float, 257> empty{}, second{};
    std::array<float *, 2> ptr{empty.data(), second.data()};
    check(actual.process(std::span<float *>(ptr.data(), channels), 0) &&
              !actual.process(std::span<float *>(ptr.data(), channels), 258),
          "v2 empty and invalid calls cannot advance new filter histories");
    left = source(2048);
    right = left;
    baseline = left;
    baselineRight = right;
    check(render(actual, left, channels == 2 ? &right : nullptr, 257) &&
              render(expected, baseline, channels == 2 ? &baselineRight : nullptr, 13) &&
              left == baseline && (channels == 1 || right == baselineRight),
          "v2 rejected snapshots and calls preserve all accepted states");
    actual.reset();
    expected.reset();
    left = source(2048);
    right = left;
    baseline = left;
    baselineRight = right;
    check(render(actual, left, channels == 2 ? &right : nullptr, 257) &&
              render(expected, baseline, channels == 2 ? &baselineRight : nullptr, 13) &&
              left == baseline && actual.latencySamples() == 0,
          "v2 reset clears new histories and settles all target weights without added latency");
    const auto before = allocations.load();
    rejectAllocation = true;
    bool safe = actual.setParameters({-6, 6, -6, 6, -6});
    for (int block = 0; block < 8; ++block)
        safe &= actual.process(std::span<float *>(ptr.data(), channels), 257);
    actual.reset();
    rejectAllocation = false;
    check(safe && allocations == before, "v2 active process/set/reset allocate nothing");
    for (float gain : {-6.f, 6.f}) {
        check(actual.setParameters({gain, gain, gain, gain, gain}), "v2 finite extreme target");
        actual.reset();
        left.assign(2048, std::numeric_limits<float>::max());
        for (std::size_t i = 1; i < left.size(); i += 2)
            left[i] = -left[i];
        right = left;
        check(render(actual, left, channels == 2 ? &right : nullptr, 257) &&
                  std::all_of(left.begin(), left.end(), [](float x) { return std::isfinite(x); }),
              "v2 combined extremes keep finite input/output finite");
    }
}

#ifdef DARKVOICE_TEST_LEGACY_V1
// Build this optional proof with the pinned49ec v1 header/source renamed into
// darkvoice_v1 in a temporary directory. Both versions share one compiler/process;
// portable fixed audio hashes are deliberately not used.
void legacyCompatibility() {
    std::size_t compared = 0;
    for (double rate : {44100., 48000., 88200., 96000.})
        for (std::size_t channels : {1U, 2U}) {
            VoiceSculptor current;
            darkvoice_v1::VoiceSculptor prior;
            check(current.prepare(rate, 257, channels) && prior.prepare(rate, 257, channels),
                  "v1/v2 temporary-reference preparation");
            std::size_t absolute = 0;
            for (std::size_t segment = 0; segment < 12; ++segment) {
                const auto p = std::array<Parameters, 4>{
                    {{}, {6, -6, 6}, {-3.7f, 2.3f, -5.1f}, {-6, 6, -6}}}[segment % 4];
                if (segment == 7)
                    check(current.prepare(rate, 257, channels) &&
                              prior.prepare(rate, 257, channels),
                          "v1/v2 reprepare lifecycle");
                check(current.setParameters(p) &&
                          prior.setParameters({p.bodyDb, p.warmthDb, p.presenceDb}),
                      "v1/v2 old-control automation accepted");
                if (segment % 3 == 0) {
                    current.reset();
                    prior.reset();
                }
                bool identical = true;
                for (std::size_t at = 0; at < 773;) {
                    const auto n = std::min<std::size_t>(
                        773 - at, std::array<std::size_t, 5>{1, 13, 257, 71, 19}[at % 5]);
                    std::array<std::array<float, 257>, 2> a{}, b{};
                    for (std::size_t c = 0; c < channels; ++c)
                        for (std::size_t i = 0; i < n; ++i)
                            a[c][i] = b[c][i] =
                                static_cast<float>(.17 * std::sin(.037 * (absolute + i) + c) +
                                                   .11 * std::cos(.211 * (absolute + i) + .7 * c));
                    std::array<float *, 2> pa{a[0].data(), a[1].data()},
                        pb{b[0].data(), b[1].data()};
                    identical &= current.process(std::span<float *>(pa.data(), channels), n) &&
                                 prior.process(std::span<float *>(pb.data(), channels), n);
                    for (std::size_t c = 0; c < channels; ++c)
                        for (std::size_t i = 0; i < n; ++i) {
                            identical &= std::bit_cast<std::uint32_t>(a[c][i]) ==
                                         std::bit_cast<std::uint32_t>(b[c][i]);
                            ++compared;
                        }
                    at += n;
                    absolute += n;
                }
                check(identical, "v2 new-zero stages retain bitwise v1 output through "
                                 "automation/reset/reprepare");
            }
        }
    std::printf("Legacy same-platform v1/v2 samples compared: %zu\n", compared);
}
#endif

void longDecay(double rate, std::size_t channels, Parameters seed = {6, 0, 0}) {
    VoiceSculptor fx;
    check(fx.prepare(rate, 257, channels) && fx.setParameters(seed), "long-decay preparation");
    fx.reset();
    check(fx.setParameters({}), "decay target neutral");
    // Fixed, continuously excited filters keep their histories normal. Only the
    // fading contribution weights approach the double-subnormal range after ~7 seconds.
    // An unflushed weight eventually stalls there and continues raising floating
    // underflow on each update. After true-subnormal flushing it stays exactzero.
    const auto input = source(257);
    auto left = input, right = input;
    std::array<float *, 2> pointers{left.data(), right.data()};
    bool rendered = true;
    const auto blocks = static_cast<std::size_t>(std::ceil(9 * rate / 257));
    for (std::size_t i = 0; i < blocks; ++i) {
        std::copy(input.begin(), input.end(), left.begin());
        std::copy(input.begin(), input.end(), right.begin());
        rendered = rendered && fx.process(std::span<float *>(pointers.data(), channels), 257);
    }
    std::feclearexcept(FE_UNDERFLOW);
    for (int i = 0; i < 16; ++i) {
        std::copy(input.begin(), input.end(), left.begin());
        std::copy(input.begin(), input.end(), right.begin());
        rendered = rendered && fx.process(std::span<float *>(pointers.data(), channels), 257);
    }
    const bool underflow = std::fetestexcept(FE_UNDERFLOW) != 0;
    check(rendered && !underflow && left == input && (channels == 1 || right == input),
          "long decay flushes true double subnormal weights without persistent underflow");
}

void lifecycle() {
    VoiceSculptor fx;
    std::array<float, 8> samples{};
    std::array<float *, 1> ptr{samples.data()};
    check(!fx.process(ptr, 8) && fx.latencySamples() == 0, "unprepared rejects process");
    for (double rate : {0., -1., 32000., 48001., std::numeric_limits<double>::infinity(),
                        std::numeric_limits<double>::quiet_NaN()}) {
        check(fx.prepare(48000, 8, 1) && !fx.prepare(rate, 8, 1) && !fx.process(ptr, 8),
              "invalid preparation unprepares");
    }
    check(!fx.prepare(48000, 0, 1) && !fx.prepare(48000, 65537, 1) && !fx.prepare(48000, 8, 0) &&
              !fx.prepare(48000, 8, 3),
          "invalid capacities/layouts");
    rejectAllocation = true;
    const bool failed = !fx.prepare(48000, 8, 1);
    rejectAllocation = false;
    check(failed && !fx.process(ptr, 8), "pimpl allocation failure safely unprepares");
    check(fx.prepare(48000, 65536, 1), "maximum capacity supported");
    check(fx.setParameters({6, 6, 6}), "set nonneutral before reprepare");
    check(fx.prepare(48000, 8, 1), "reprepare restores defaults");
    samples.fill(.25f);
    check(fx.process(ptr, 8) && samples[0] == .25f && samples[7] == .25f,
          "successful preparation resets default neutral targets");
}
} // namespace

#if defined(__GNUC__) || defined(__clang__)
#define DV_NOINLINE __attribute__((noinline))
#else
#define DV_NOINLINE
#endif
DV_NOINLINE void *operator new(std::size_t size) {
    ++allocations;
    if (rejectAllocation)
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
    lifecycle();
#ifdef DARKVOICE_TEST_LEGACY_V1
    legacyCompatibility();
#endif
    for (double rate : {44100., 48000., 88200., 96000.})
        for (std::size_t channels : {1U, 2U}) {
            spectral(rate, channels);
            spectralV2(rate, channels);
            automation(rate, channels);
            behavior(rate, channels);
            behaviorV2(rate, channels);
            longDecay(rate, channels);
            longDecay(rate, channels, {0, 0, 0, 6, 6});
        }
    std::printf("VoiceSculptor checks: %d, failures: %d\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
