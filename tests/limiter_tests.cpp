#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <darkvoice/VocalLimiter.hpp>
#include <limits>
#include <new>
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
bool render(darkvoice::VocalLimiter &fx, std::vector<float> &l, std::vector<float> *r,
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
    darkvoice::VocalLimiter fx;
    check(fx.prepare(rate, 512, channels), "matrix prepare");
    std::vector<float> left(4096), right(left.size());
    for (std::size_t i = 0; i < left.size(); ++i) {
        left[i] = static_cast<float>(.01 * std::sin(.017 * i));
        right[i] = -left[i];
    }
    left[0] = 1e-21f;
    left[1] = -std::numeric_limits<float>::min();
    const auto originalLeft = left, originalRight = right;
    check(render(fx, left, channels == 2 ? &right : nullptr, 37) && left == originalLeft &&
              (channels == 1 || right == originalRight),
          "below-ceiling unity exact incl tiny normals");
    for (float ceilingDb : {-24.f, -1.f, 0.f}) {
        check(fx.setParameters({ceilingDb, 20}), "ceiling snapshot");
        fx.reset();
        for (std::size_t i = 0; i < left.size(); ++i) {
            left[i] = i % 2 ? -2.f : 2.f;
            right[i] = -.25f * left[i];
        }
        check(render(fx, left, channels == 2 ? &right : nullptr, 257), "sustained peaks render");
        const double ceiling = std::pow(10., ceilingDb / 20.);
        bool bounded = true, linked = true;
        for (std::size_t i = 0; i < left.size(); ++i) {
            bounded =
                bounded && std::abs(left[i]) <= ceiling && std::abs(left[i]) > ceiling * .999999;
            if (channels == 2) {
                bounded = bounded && std::abs(right[i]) <= ceiling;
                linked = linked && right[i] == -.25f * left[i];
            }
        }
        check(bounded, "sample ceiling honored without attack overshoot or startup latency");
        check(linked, "stereo linked gain preserves ratio and polarity");
    }
    fx.reset();
    check(fx.setParameters({-1, 20}), "release setup");
    left.assign(static_cast<std::size_t>(rate * .5), .01f);
    right.assign(left.size(), .005f);
    left[0] = 2;
    right[0] = 1;
    check(render(fx, left, channels == 2 ? &right : nullptr, 37), "impulse/release render");
    const auto atTau = static_cast<std::size_t>(rate * .020);
    const double initialGain = left[0] / 2.;
    const double expected = 1 - (1 - initialGain) * std::exp(-1.);
    check(std::abs(left[atTau] / .01 - expected) < 2e-6,
          "gain release reaches mathematically expected one-time-constant value");
    bool monotone = true;
    for (std::size_t i = 2; i < left.size(); ++i)
        monotone = monotone && left[i] >= left[i - 1] && left[i] <= .01f;
    check(monotone && left.back() == .01f,
          "release monotonic, settles to exact dry without makeup");
    std::array<float, 64> a{}, b{};
    std::array<float *, 2> ptr{a.data(), b.data()};
    const auto before = allocations.load();
    reject = true;
    const bool safe = fx.setParameters({-24, 5}) &&
                      fx.process(std::span<float *>(ptr.data(), channels), a.size());
    fx.reset();
    reject = false;
    check(safe && allocations.load() == before, "set/process/reset have no callback allocation");
}
void transitions() {
    darkvoice::VocalLimiter fx;
    check(fx.prepare(48000, 8, 1) && fx.setParameters({0, 1000}), "transition setup");
    std::array<float, 8> samples{2};
    std::array<float *, 1> ptr{samples.data()};
    check(fx.process(ptr, 1) && samples[0] == 1, "instantaneous linked attack");
    check(fx.setParameters({-24, 1000}), "lower ceiling during release");
    samples[0] = .5f;
    check(fx.process(ptr, 1) && samples[0] <= std::pow(10., -24. / 20.) && samples[0] > .06f,
          "lower ceiling applies next sample even during release");
    check(fx.setParameters({0, 1000}), "raise ceiling during reduction");
    samples.fill(.5f);
    check(fx.process(ptr, samples.size()) && samples.back() > samples.front() &&
              samples.back() < .07f,
          "raising ceiling retains gradual release");
    fx.reset();
    samples.fill(.5f);
    check(fx.process(ptr, samples.size()) && samples.front() == .5f && samples.back() == .5f,
          "reset clears gain reduction, retains parameters");
}
void partitions() {
    darkvoice::VocalLimiter one, split;
    check(one.prepare(48000, 512, 2) && split.prepare(48000, 512, 2), "partition prepare");
    std::vector<float> a(10000), b(a.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        a[i] = static_cast<float>(2 * std::sin(.037 * i));
        b[i] = static_cast<float>(1.7 * std::sin(.021 * i));
    }
    auto c = a, d = b;
    const auto run = [&](darkvoice::VocalLimiter &fx, std::vector<float> &l, std::vector<float> &r,
                         std::size_t block) {
        std::size_t offset = 0;
        while (offset < l.size()) {
            if (offset == 137 && !fx.setParameters({-24, 5}))
                return false;
            if (offset == 4093 && !fx.setParameters({0, 1000}))
                return false;
            const auto end = offset < 137 ? 137U : offset < 4093 ? 4093U : l.size();
            const auto count = std::min(block, end - offset);
            std::array<float *, 2> ptr{l.data() + offset, r.data() + offset};
            if (!fx.process(ptr, count))
                return false;
            offset += count;
        }
        return true;
    };
    check(run(one, a, b, 512) && run(split, c, d, 37) && a == c && b == d,
          "sample-positioned target transitions partition invariant");
    std::array<float *, 2> bad{a.data(), nullptr};
    const auto before = a;
    check(!one.process(bad, 1) && a == before && !one.setParameters({1, 100}),
          "warmed invalid snapshot/buffer atomic rejection");
    check(render(one, a, &b, 257) && render(split, c, &d, 257) && a == c && b == d,
          "warmed rejection preserves gain and targets");
}
void validation() {
    darkvoice::VocalLimiter fx;
    std::array<float, 512> a{1}, b{-1};
    std::array<float *, 2> ptr{a.data(), b.data()};
    check(!fx.process(ptr, 1), "unprepared rejects");
    for (double rate : {0., 32000., std::numeric_limits<double>::infinity()})
        check(!fx.prepare(rate, 512, 2), "invalid rate");
    check(!fx.prepare(48000, 0, 2) && !fx.prepare(48000, 65537, 2) && !fx.prepare(48000, 512, 3),
          "invalid capacity/layout");
    for (auto member : {&darkvoice::VocalLimiterParameters::ceilingDb,
                        &darkvoice::VocalLimiterParameters::releaseMs})
        for (float bad : {std::numeric_limits<float>::quiet_NaN(),
                          std::numeric_limits<float>::infinity(), -25.f, 1001.f}) {
            darkvoice::VocalLimiterParameters p;
            p.*member = bad;
            check(!fx.setParameters(p), "invalid complete snapshot");
        }
    const auto before = allocations.load();
    reject = true;
    const bool prepared = fx.prepare(96000, 65536, 2);
    reject = false;
    check(prepared && allocations.load() == before, "preparation itself needs no allocation");
    check(fx.prepare(48000, 512, 2), "validation prepare");
    ptr[1] = nullptr;
    check(!fx.process(ptr, 1) && a[0] == 1, "null atomic rejection");
    ptr[1] = a.data();
    check(!fx.process(ptr, 1) && a[0] == 1, "alias atomic rejection");
    ptr = {nullptr, nullptr};
    check(fx.process(ptr, 0), "zero frames permits null channels");
    ptr = {a.data(), b.data()};
    check(!fx.process(ptr, 513), "oversize rejection");
    a.fill(std::numeric_limits<float>::max());
    b.fill(-std::numeric_limits<float>::max());
    a[0] = std::numeric_limits<float>::quiet_NaN();
    a[1] = std::numeric_limits<float>::infinity();
    b[0] = std::numeric_limits<float>::denorm_min();
    check(fx.process(ptr, 512) && a[0] == 0 && a[1] == 0 && b[0] == 0 &&
              std::all_of(a.begin(), a.end(),
                          [](float v) { return std::isfinite(v) && std::abs(v) <= .9f; }) &&
              std::all_of(b.begin(), b.end(),
                          [](float v) { return std::isfinite(v) && std::abs(v) <= .9f; }),
          "hostile float-range input bounded, finite, sanitized");
    check(!fx.prepare(48000, 512, 0) && !fx.process(ptr, 1),
          "invalid reprepare unprepares warmed instance");
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
    transitions();
    partitions();
    validation();
    std::printf("%d checks %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
