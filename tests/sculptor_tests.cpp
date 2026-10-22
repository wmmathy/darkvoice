#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <darkvoice/PitchFormantProcessor.hpp>
#include <limits>
#include <new>
#include <numbers>
#include <string>
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
bool render(darkvoice::PitchFormantProcessor &fx, std::vector<float> &left,
            std::vector<float> *right, std::size_t block) {
    for (std::size_t o = 0; o < left.size(); o += block) {
        std::array<float *, 2> ptr{left.data() + o, right ? right->data() + o : nullptr};
        if (!fx.process(std::span<float *>(ptr.data(), right ? 2 : 1),
                        std::min(block, left.size() - o)))
            return false;
    }
    return true;
}
std::vector<float> tone(double rate, double hz, double seconds = 1.5) {
    std::vector<float> x(static_cast<std::size_t>(seconds * rate));
    for (std::size_t i = 0; i < x.size(); ++i)
        x[i] = static_cast<float>(.1 * std::sin(2 * std::numbers::pi * hz * i / rate));
    return x;
}
double frequency(const std::vector<float> &x, double rate) {
    double first = -1, last = 0;
    int crossings = 0;
    for (std::size_t i = x.size() / 2 + 1; i < x.size(); ++i)
        if (x[i - 1] <= 0 && x[i] > 0) {
            const double t = i - 1 - static_cast<double>(x[i - 1]) / (x[i] - x[i - 1]);
            if (first < 0)
                first = t;
            last = t;
            ++crossings;
        }
    return crossings > 1 ? (crossings - 1) * rate / (last - first) : 0;
}
double amplitude(const std::vector<float> &x, double rate, double hz) {
    double s = 0, c = 0;
    for (std::size_t i = x.size() / 2; i < x.size(); ++i) {
        const double phase = 2 * std::numbers::pi * hz * i / rate;
        s += x[i] * std::sin(phase);
        c += x[i] * std::cos(phase);
    }
    return 2 * std::hypot(s, c) / (x.size() - x.size() / 2);
}
std::vector<float> vowel(double rate, double center = 0) {
    std::vector<float> x(static_cast<std::size_t>(rate * 2));
    for (int h = 1; h <= 80; ++h) {
        const double f = 100 * h;
        const double envelope =
            center > 0 ? std::exp(-.5 * std::pow((f - center) / 100, 2)) + .002 / h
                       : std::exp(-.5 * std::pow((f - 600) / 90, 2)) +
                             .8 * std::exp(-.5 * std::pow((f - 1200) / 140, 2)) +
                             .5 * std::exp(-.5 * std::pow((f - 2500) / 200, 2)) + .002 / h;
        for (std::size_t i = 0; i < x.size(); ++i)
            x[i] +=
                static_cast<float>(.025 * envelope * std::sin(2 * std::numbers::pi * f * i / rate));
    }
    return x;
}
double peak(const std::vector<float> &x, double rate, double f0, double low, double high) {
    double best = 0, result = 0;
    for (int h = 1; h * f0 <= high; ++h) {
        const double f = h * f0;
        if (f < low)
            continue;
        const auto value = amplitude(x, rate, f);
        if (value > best) {
            best = value;
            result = f;
        }
    }
    return result;
}
double correlation(const std::vector<float> &x, double rate, double f0) {
    const auto lag = static_cast<std::size_t>(std::round(rate / f0));
    double ab = 0, aa = 0, bb = 0;
    for (std::size_t i = x.size() / 2 + lag; i < x.size(); ++i) {
        ab += static_cast<double>(x[i]) * x[i - lag];
        aa += static_cast<double>(x[i]) * x[i];
        bb += static_cast<double>(x[i - lag]) * x[i - lag];
    }
    return aa * bb > 0 ? ab / std::sqrt(aa * bb) : 0;
}
void validation() {
    darkvoice::PitchFormantProcessor fx;
    std::array<float, 8> x{1};
    std::array<float *, 2> ptr{x.data(), nullptr};
    check(!fx.process(std::span<float *>(ptr.data(), 1), 8) && fx.latencySamples() == 0,
          "unprepared rejects");
    for (double bad : {0., 32000., std::numeric_limits<double>::infinity()})
        check(!fx.prepare({bad, 8, 1, darkvoice::PitchQuality::Live}), "invalid rate");
    check(!fx.prepare({48000, 0, 1}) && !fx.prepare({48000, 65537, 1}) &&
              !fx.prepare({48000, 8, 3}),
          "capacity and layout validation");
    check(!fx.prepare({48000, 8, 1, static_cast<darkvoice::PitchQuality>(99)}), "invalid quality");
    check(fx.prepare({48000, 8, 2, darkvoice::PitchQuality::Live}), "prepare stereo");
    check(!fx.process(ptr, 8) && x[0] == 1, "null channel atomic rejection");
    check(fx.process(ptr, 0), "empty block");
    ptr[1] = x.data();
    check(!fx.process(ptr, 8) && x[0] == 1, "alias rejection");
    check(!fx.process(ptr, 9), "oversize rejection");
    for (auto field : {&darkvoice::PitchFormantParameters::pitchSemitones,
                       &darkvoice::PitchFormantParameters::formantSemitones,
                       &darkvoice::PitchFormantParameters::formantBaseHz})
        for (float bad : {std::numeric_limits<float>::quiet_NaN(),
                          std::numeric_limits<float>::infinity(), -100.f, 1100.f}) {
            darkvoice::PitchFormantParameters p;
            p.*field = bad;
            check(!fx.setParameters(p), "invalid snapshot");
        }
    darkvoice::PitchFormantProcessor fresh;
    reject = true;
    const bool ok = fresh.prepare({48000, 8, 1});
    reject = false;
    check(!ok && fresh.latencySamples() == 0, "allocation failure unprepares");
}
void neutral(double rate, darkvoice::PitchQuality quality, std::size_t channels) {
    darkvoice::PitchFormantProcessor fx;
    check(fx.prepare({rate, 1024, channels, quality}), "profile prepare");
    const auto delay = fx.latencySamples();
    const auto block = (quality == darkvoice::PitchQuality::Live     ? 1024U
                        : quality == darkvoice::PitchQuality::Studio ? 4096U
                                                                     : 8192U) *
                       (rate > 48000 ? 2U : 1U);
    check(delay == block + block / 8 && fx.tailSamples() == delay + block,
          "profile reported latency and drain bound");
    std::vector<float> l(delay + block + 100, 0), r = l;
    l[0] = .5f;
    l[1] = 1e-21f;
    l[2] = -std::numeric_limits<float>::min();
    r[0] = -.25f;
    r[1] = -1e-21f;
    const auto a = l, b = r;
    check(render(fx, l, channels == 2 ? &r : nullptr, 37), "neutral render");
    bool identity = true;
    for (std::size_t i = 0; i < l.size(); ++i) {
        identity = identity && l[i] == (i < delay ? 0 : a[i - delay]);
        if (channels == 2)
            identity = identity && r[i] == (i < delay ? 0 : b[i - delay]);
    }
    check(identity, "neutral exact delayed dry including tiny normals");
    std::array<float, 64> safeLeft{}, safeRight{};
    std::array<float *, 2> safePtr{safeLeft.data(), safeRight.data()};
    const auto before = allocations.load();
    reject = true;
    const bool safe = fx.setParameters({7, 3, true, 100}) &&
                      fx.process(std::span<float *>(safePtr.data(), channels), 64);
    fx.reset();
    reject = false;
    check(safe && allocations.load() == before,
          "all profiles callback operations allocate nothing");
    check(fx.setParameters({}), "restore neutral after allocation probe");
    fx.reset();
    l.assign(delay + block + 100, 0);
    r = l;
    check(render(fx, l, channels == 2 ? &r : nullptr, 257) &&
              std::all_of(l.begin(), l.end(), [](float v) { return v == 0; }),
          "reset clears latency rings");
}
void accuracy(double rate) {
    for (float shift : {-12.f, -7.f, 7.f, 12.f}) {
        darkvoice::PitchFormantProcessor fx;
        check(fx.prepare({rate, 1024, 1, darkvoice::PitchQuality::Studio}) &&
                  fx.setParameters({shift, 0, false, 100}),
              "pitch setup");
        fx.reset();
        auto x = tone(rate, 110);
        check(render(fx, x, nullptr, 257), "pitch render");
        const double expected = 110 * std::pow(2., shift / 12.);
        const double measured = frequency(x, rate);
        const double cents = measured > 0 ? 1200 * std::log2(measured / expected) : 1e9;
        std::printf("pitch rate %.0f shift %.0f measured %.4f cents %.3f\n", rate, shift, measured,
                    cents);
        check(std::abs(cents) < 20, "stable sine pitch accuracy within20cents");
    }
}
void formants() {
    const double rate = 48000;
    const auto input = vowel(rate);
    for (auto p : {darkvoice::PitchFormantParameters{7, 0, true, 100},
                   darkvoice::PitchFormantParameters{0, 7, true, 100},
                   darkvoice::PitchFormantParameters{0, -7, true, 100}}) {
        darkvoice::PitchFormantProcessor fx;
        check(fx.prepare({rate, 1024, 1, darkvoice::PitchQuality::Offline}) && fx.setParameters(p),
              "vowel setup");
        fx.reset();
        auto x = input;
        check(render(fx, x, nullptr, 512), "vowel render");
        const double f0 = 100 * std::pow(2., p.pitchSemitones / 12.);
        const double scale = std::pow(2., p.formantSemitones / 12.);
        // Geometric boundaries keep adjacent resonances distinct without excluding
        // downshifted targets or restricting searches to their acceptance intervals.
        const double boundary = std::sqrt(600.0 * 1200.0) * scale;
        const double upper = std::sqrt(1200.0 * 2500.0) * scale;
        const double f1 = peak(x, rate, f0, 200, boundary), f2 = peak(x, rate, f0, boundary, upper);
        std::printf("vowel pitch %.0f formant %.0f peaks %.2f %.2f periodicity %.4f\n",
                    p.pitchSemitones, p.formantSemitones, f1, f2, correlation(x, rate, f0));
        check(std::abs(f1 / (600 * scale) - 1) < .2 && std::abs(f2 / (1200 * scale) - 1) < .2,
              "synthetic vowel envelope peaks follow independent formant target");
        check(correlation(x, rate, f0) > .8, "vowel F0 remains on intended pitch grid");
    }
}
void isolatedFormants() {
    for (double center : {600., 1200.})
        for (float shift : {-7.f, 7.f}) {
            darkvoice::PitchFormantProcessor fx;
            check(fx.prepare({48000, 1024, 1, darkvoice::PitchQuality::Offline}) &&
                      fx.setParameters({0, shift, true, 100}),
                  "isolated formant setup");
            fx.reset();
            auto x = vowel(48000, center);
            check(render(fx, x, nullptr, 512), "isolated formant render");
            const double expected = center * std::pow(2., shift / 12.);
            const double measured = peak(x, 48000, 100, 200, 3000);
            std::printf("isolated formant center %.0f shift %.0f measured %.2f expected %.2f\n",
                        center, shift, measured, expected);
            check(std::abs(measured / expected - 1) < .2,
                  "isolated envelope peak within20percent of formant target");
            check(correlation(x, 48000, 100) > .8, "isolated formant change retains F0 grid");
        }
}
void safety() {
    darkvoice::PitchFormantProcessor one, split;
    check(one.prepare({48000, 1024, 2, darkvoice::PitchQuality::Live}) &&
              split.prepare({48000, 1024, 2, darkvoice::PitchQuality::Live}),
          "partition prepare");
    check(one.setParameters({7, 3, true, 100}) && split.setParameters({7, 3, true, 100}),
          "partition params");
    auto a = tone(48000, 100, .5), b = a, c = a, d = a;
    check(render(one, a, &b, 512) && render(split, c, &d, 37), "partition render");
    double error = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
        error = std::max(error, static_cast<double>(std::abs(a[i] - c[i])));
    std::printf("partition maximum error %.9g\n", error);
    check(error < 1e-5, "fixed-grid control and streaming partition invariance");
    double stereoError = 0, stereoPower = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        stereoError += std::pow(static_cast<double>(a[i]) - b[i], 2);
        stereoPower += static_cast<double>(a[i]) * a[i];
    }
    check(stereoPower > 0 && std::sqrt(stereoError / stereoPower) < 1e-4,
          "identical stereo inputs remain equal within measured float tolerance");
    std::array<float *, 2> invalidPtr{nullptr, nullptr};
    check(!one.process(invalidPtr, 1), "warmed invalid block rejected");
    check(!one.setParameters({13, 0, true, 100}), "warmed invalid snapshot rejected");
    check(render(one, a, &b, 257) && render(split, c, &d, 257) && a == c && b == d,
          "invalid operations preserve warmed history and targets");
    std::array<float, 512> l{}, r{};
    std::array<float *, 2> ptr{l.data(), r.data()};
    const auto before = allocations.load();
    reject = true;
    const bool processed = one.setParameters({-12, -12, true, 100}) && one.process(ptr, 512);
    one.reset();
    reject = false;
    check(processed && allocations.load() == before, "callback set process reset allocate nothing");
    l[0] = std::numeric_limits<float>::infinity();
    l[1] = std::numeric_limits<float>::quiet_NaN();
    l[2] = std::numeric_limits<float>::max();
    r = l;
    check(one.process(ptr, 512) &&
              std::all_of(l.begin(), l.end(), [](float x) { return std::isfinite(x); }) &&
              std::all_of(r.begin(), r.end(), [](float x) { return std::isfinite(x); }),
          "hostile audio finite");
}
void drain() {
    darkvoice::PitchFormantProcessor fx;
    check(fx.prepare({48000, 1024, 2, darkvoice::PitchQuality::Studio}) &&
              fx.setParameters({7, 3, true, 100}),
          "active drain setup");
    fx.reset();
    const auto tail = fx.tailSamples();
    std::vector<float> a(tail * 3), b(tail * 3);
    a[0] = b[0] = .1f;
    check(render(fx, a, &b, 512), "active impulse drain");
    double total = 0, remaining = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        total += std::abs(a[i]) + std::abs(b[i]);
        if (i >= tail)
            remaining = std::max(remaining, std::max(static_cast<double>(std::abs(a[i])),
                                                     static_cast<double>(std::abs(b[i]))));
    }
    std::printf("active drain tail %zu remaining %.9g\n", tail, remaining);
    check(total > 1e-5 && remaining < 1e-7, "actual active impulse fits conservative drain bound");
}
void quiet() {
    darkvoice::PitchFormantProcessor fx;
    check(fx.prepare({48000, 1024, 1, darkvoice::PitchQuality::Live}) &&
              fx.setParameters({0, .0001f, true, 100}),
          "quiet active setup");
    fx.reset();
    std::vector<float> silence(8192);
    check(render(fx, silence, nullptr, 512), "quiet warmup");
    std::vector<float> x(4096);
    x[0] = 1e-9f;
    check(render(fx, x, nullptr, 512), "quiet impulse");
    const auto found = static_cast<std::size_t>(
        std::max_element(x.begin(), x.end(),
                         [](float a, float b) { return std::abs(a) < std::abs(b); }) -
        x.begin());
    std::printf("quiet peak %zu expected %zu amplitude %.9g\n", found, fx.latencySamples(),
                x[found]);
    check(found == fx.latencySamples() && x[0] == 0,
          "patched near-silence avoids immediate direct copy");
}
} // namespace
#if defined(__GNUC__) || defined(__clang__)
#define DV_NOINLINE __attribute__((noinline))
#else
#define DV_NOINLINE
#endif
DV_NOINLINE void *operator new(std::size_t n) {
    ++allocations;
    if (reject)
        throw std::bad_alloc();
    if (void *p = std::malloc(n ? n : 1))
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
#undef DV_NOINLINE
int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    const std::string suite = argv[1];
    if (suite == "streaming") {
        validation();
        for (double rate : {44100., 48000., 88200., 96000.})
            for (auto quality : {darkvoice::PitchQuality::Live, darkvoice::PitchQuality::Studio,
                                 darkvoice::PitchQuality::Offline})
                for (std::size_t channels : {1U, 2U})
                    neutral(rate, quality, channels);
        safety();
        drain();
        quiet();
    } else if (suite == "qualification") {
        for (double rate : {44100., 48000., 88200., 96000.})
            accuracy(rate);
        formants();
        isolatedFormants();
    } else
        return 2;
    std::printf("%d checks %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
