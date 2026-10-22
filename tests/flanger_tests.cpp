#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <darkvoice/Flanger.hpp>
#include <limits>
#include <new>
#include <numbers>
#include <vector>
namespace {
std::atomic<std::size_t> allocations{0};
std::atomic<bool> reject{false};
int checks = 0, failures = 0;
void check(bool ok, const char *message) {
    ++checks;
    if (!ok) {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", message);
    }
}
bool render(darkvoice::Flanger &fx, std::vector<float> &l, std::vector<float> *r,
            std::size_t block) {
    for (std::size_t offset = 0; offset < l.size(); offset += block) {
        std::array<float *, 2> ptr{l.data() + offset, r ? r->data() + offset : nullptr};
        if (!fx.process(std::span<float *>(ptr.data(), r ? 2 : 1),
                        std::min(block, l.size() - offset)))
            return false;
    }
    return true;
}
void matrix(double rate, std::size_t channels) {
    darkvoice::Flanger fx;
    check(fx.prepare(rate, 512, channels), "matrix prepare");
    std::vector<float> l(4096), r(l.size());
    for (std::size_t i = 0; i < l.size(); ++i) {
        l[i] = static_cast<float>(.1 * std::sin(.017 * i));
        r[i] = -l[i];
    }
    l[0] = 1e-21f;
    l[1] = -std::numeric_limits<float>::min();
    const auto a = l, b = r;
    check(render(fx, l, channels == 2 ? &r : nullptr, 37) && l == a && (channels == 1 || r == b),
          "neutral exact including tiny normals");
    check(fx.setParameters({1, 0, 0, 1}), "matrix impulse params");
    fx.reset();
    l.assign(2048, 0);
    r = l;
    l[0] = r[0] = .5f;
    check(render(fx, l, channels == 2 ? &r : nullptr, 257), "matrix impulse render");
    double sum = 0, moment = 0;
    for (std::size_t i = 0; i < l.size(); ++i) {
        sum += l[i];
        moment += i * static_cast<double>(l[i]);
    }
    check(std::abs(sum - .5) < 1e-6 && std::abs(moment / sum - .0045 * rate) < 1e-4 && l[0] == 0,
          "4.5ms impulse delay and unity wet amplitude");
    check(channels == 1 || l == r, "same stereo phase and output");
    const auto impulse = l;
    fx.reset();
    l.assign(2048, 0);
    r = l;
    l[0] = r[0] = .5f;
    check(render(fx, l, channels == 2 ? &r : nullptr, 19) && l == impulse,
          "reset retains targets, clears feedback and LFO history");
    std::array<float, 64> safeL{}, safeR{};
    std::array<float *, 2> ptr{safeL.data(), safeR.data()};
    const auto before = allocations.load();
    reject = true;
    const bool safe = fx.setParameters({5, 4, -.85f, 1}) &&
                      fx.process(std::span<float *>(ptr.data(), channels), safeL.size());
    fx.reset();
    reject = false;
    check(safe && allocations.load() == before, "callback setters/process/reset allocate nothing");
}
void echoes() {
    for (float feedback : {-.85f, -.5f, .5f, .85f}) {
        darkvoice::Flanger fx;
        check(fx.prepare(48000, 512, 2) && fx.setParameters({1, 0, feedback, 1}), "echo setup");
        fx.reset();
        std::vector<float> l(96000), r(l.size());
        l[0] = r[0] = .5f;
        check(render(fx, l, &r, 512), "echo render");
        bool geometric = true;
        for (int echo = 0; echo < 10; ++echo) {
            const double expected = .5 * std::pow(feedback, echo);
            geometric = geometric && std::abs(l[216 * (echo + 1)] - expected) < 1e-6;
        }
        check(geometric && l == r, "signed geometric feedback echoes at216sample intervals");
        check(std::all_of(l.end() - 10000, l.end(), [](float v) { return std::abs(v) < 1e-12f; }),
              "maximum feedback decays below numerical threshold");
    }
}
void modulation() {
    darkvoice::Flanger fx;
    check(fx.prepare(48000, 512, 1) && fx.setParameters({1, 4, 0, 1}), "modulation setup");
    fx.reset();
    std::vector<float> x(48000);
    for (std::size_t offset : {0U, 12000U, 24000U, 36000U})
        x[offset] = .5f;
    check(render(fx, x, nullptr, 37), "modulation render");
    double lowDelay = 1e9, highDelay = 0;
    for (std::size_t offset : {0U, 12000U, 24000U, 36000U}) {
        auto first = x.begin() + offset + 20, last = x.begin() + offset + 420;
        const auto peak = static_cast<std::size_t>(std::max_element(first, last) - x.begin());
        const double measuredDelay = peak - offset;
        double arrival = offset + 216.;
        for (int iteration = 0; iteration < 8; ++iteration)
            arrival = offset + 216 + 192 * std::sin(2 * std::numbers::pi * arrival / 48000);
        check(std::abs(static_cast<double>(peak) - arrival) < 1.1,
              "sinusoidal variable delay follows rate and depth");
        lowDelay = std::min(lowDelay, measuredDelay);
        highDelay = std::max(highDelay, measuredDelay);
    }
    check(lowDelay < 30 && highDelay > 400, "measured depth reaches0.5..8.5ms extremes");
}
void invariance() {
    darkvoice::Flanger one, split;
    check(one.prepare(48000, 512, 2) && split.prepare(48000, 512, 2), "partition prepare");
    std::vector<float> a(32000), b(a.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        a[i] = static_cast<float>(.1 * std::sin(.03 * i));
        b[i] = static_cast<float>(.1 * std::sin(.023 * i));
    }
    auto c = a, d = b;
    const auto run = [&](darkvoice::Flanger &fx, std::vector<float> &l, std::vector<float> &r,
                         std::size_t block) {
        std::size_t offset = 0;
        while (offset < l.size()) {
            if (offset == 137 && !fx.setParameters({5, 4, .85f, 1}))
                return false;
            if (offset == 8191 && !fx.setParameters({.05f, 0, -.85f, .3f}))
                return false;
            if (offset == 16003 && !fx.setParameters({.3f, 2, 0, 0}))
                return false;
            const auto end = offset < 137     ? 137U
                             : offset < 8191  ? 8191U
                             : offset < 16003 ? 16003U
                                              : l.size();
            const auto count = std::min(block, end - offset);
            std::array<float *, 2> ptr{l.data() + offset, r.data() + offset};
            if (!fx.process(ptr, count))
                return false;
            offset += count;
        }
        return true;
    };
    check(run(one, a, b, 512) && run(split, c, d, 37) && a == c && b == d,
          "sample-positioned automation partition invariant");
    std::array<float *, 2> invalid{nullptr, b.data()};
    check(!one.process(invalid, 1) && !one.setParameters({6, 0, 0, 0}),
          "invalid warmed calls rejected");
    check(render(one, a, &b, 257) && render(split, c, &d, 257) && a == c && b == d,
          "invalid calls preserve warmed histories and targets");
    // Disabling output mix does not freeze the feedback path.
    darkvoice::Flanger warm;
    check(warm.prepare(48000, 512, 1) && warm.setParameters({1, 0, .85f, 0}),
          "warm disabled setup");
    warm.reset();
    std::vector<float> impulse(256);
    impulse[0] = .5f;
    check(render(warm, impulse, nullptr, 256) && impulse[0] == .5f,
          "disabled exact dry warms ring");
    check(warm.setParameters({1, 0, .85f, 1}), "enable warmed feedback");
    std::vector<float> silence(1000);
    check(
        render(warm, silence, nullptr, 257) &&
            std::any_of(silence.begin(), silence.end(), [](float v) { return std::abs(v) > .01f; }),
        "enabling wet reveals existing feedback history");
}
void validation() {
    darkvoice::Flanger fx;
    std::array<float, 512> a{1}, b{-1};
    std::array<float *, 2> ptr{a.data(), b.data()};
    check(!fx.process(ptr, 1), "unprepared rejects");
    for (double rate : {0., 32000., std::numeric_limits<double>::infinity()})
        check(!fx.prepare(rate, 512, 2), "invalid rate");
    check(!fx.prepare(48000, 0, 2) && !fx.prepare(48000, 65537, 2) && !fx.prepare(48000, 512, 3),
          "invalid capacity/layout");
    for (auto member : {&darkvoice::FlangerParameters::flangerRateHz,
                        &darkvoice::FlangerParameters::flangerDepthMs,
                        &darkvoice::FlangerParameters::flangerFeedback,
                        &darkvoice::FlangerParameters::flangerMix})
        for (float bad : {std::numeric_limits<float>::quiet_NaN(),
                          std::numeric_limits<float>::infinity(), -10.f, 10.f}) {
            darkvoice::FlangerParameters p;
            p.*member = bad;
            check(!fx.setParameters(p), "invalid complete snapshot");
        }
    check(fx.prepare(48000, 512, 2), "validation prepare");
    ptr[1] = nullptr;
    check(!fx.process(ptr, 1) && a[0] == 1, "null atomic rejection");
    ptr[1] = a.data();
    check(!fx.process(ptr, 1) && a[0] == 1, "direct alias atomic rejection");
    ptr[1] = b.data();
    check(!fx.process(ptr, 513), "capacity rejection");
    check(fx.setParameters({5, 4, .85f, 1}), "hostile snapshot");
    fx.reset();
    a.fill(std::numeric_limits<float>::max());
    b.fill(-std::numeric_limits<float>::max());
    a[0] = std::numeric_limits<float>::quiet_NaN();
    a[1] = std::numeric_limits<float>::infinity();
    b[0] = std::numeric_limits<float>::denorm_min();
    bool finite = true;
    for (int block = 0; block < 20; ++block)
        finite = finite && fx.process(ptr, 512) &&
                 std::all_of(a.begin(), a.end(), [](float v) { return std::isfinite(v); }) &&
                 std::all_of(b.begin(), b.end(), [](float v) { return std::isfinite(v); });
    check(finite, "hostile audio and extreme feedback remain finite");
    darkvoice::Flanger fresh;
    reject = true;
    const bool ok = fresh.prepare(96000, 512, 2);
    reject = false;
    check(!ok && !fresh.process(ptr, 1), "allocation failure leaves unprepared");
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
int main() {
    for (double rate : {44100., 48000., 88200., 96000.})
        for (std::size_t channels : {1U, 2U})
            matrix(rate, channels);
    echoes();
    modulation();
    invariance();
    validation();
    std::printf("%d checks %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
