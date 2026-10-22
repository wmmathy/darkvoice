#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <darkvoice/ModulationEffects.hpp>
#include <limits>
#include <new>
#include <numbers>
#include <vector>
namespace {
std::atomic<std::size_t> allocations{0};
std::atomic<bool> rejectAllocation{false};
int checks = 0, failures = 0;
void check(bool pass, const char *message) {
    ++checks;
    if (!pass) {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", message);
    }
}
bool render(darkvoice::ModulationEffects &fx, std::vector<float> &left, std::vector<float> *right,
            std::size_t block) {
    for (std::size_t offset = 0; offset < left.size(); offset += block) {
        std::array<float *, 2> ptr{left.data() + offset, right ? right->data() + offset : nullptr};
        if (!fx.process(std::span<float *>(ptr.data(), right ? 2 : 1),
                        std::min(block, left.size() - offset)))
            return false;
    }
    return true;
}
void validation() {
    darkvoice::ModulationEffects fx;
    std::array<float, 8> x{1};
    std::array<float *, 2> ptr{x.data(), nullptr};
    check(!fx.process(std::span<float *>(ptr.data(), 1), 8), "unprepared rejects");
    for (double rate : {0., 32000., std::numeric_limits<double>::quiet_NaN()})
        check(!fx.prepare(rate, 8, 1), "invalid rate");
    check(!fx.prepare(48000, 0, 1) && !fx.prepare(48000, 65537, 1), "capacity bounds");
    check(!fx.prepare(48000, 8, 0) && !fx.prepare(48000, 8, 3), "layout bounds");
    check(fx.prepare(48000, 8, 2), "prepare stereo");
    check(!fx.process(ptr, 8) && x[0] == 1, "null channel no partial write");
    check(fx.process(ptr, 0), "zero frames permit null");
    ptr[1] = x.data();
    check(!fx.process(ptr, 8) && x[0] == 1, "alias rejects");
    check(!fx.process(ptr, 9), "oversized rejects");
    check(!fx.process(std::span<float *>(ptr.data(), 1), 8), "wrong channel count");
    for (auto field : {&darkvoice::ModulationParameters::chorusRateHz,
                       &darkvoice::ModulationParameters::chorusDepthMs,
                       &darkvoice::ModulationParameters::chorusMix,
                       &darkvoice::ModulationParameters::stereoWidth})
        for (float bad : {std::numeric_limits<float>::quiet_NaN(),
                          std::numeric_limits<float>::infinity(), -1.f, 100.f}) {
            darkvoice::ModulationParameters p;
            p.*field = bad;
            check(!fx.setParameters(p), "invalid parameters reject");
        }
    check(!fx.prepare(12345, 8, 2) && !fx.process(ptr, 4), "invalid prepare unprepares");
    darkvoice::ModulationEffects fresh;
    rejectAllocation = true;
    const bool prepared = fresh.prepare(48000, 8, 2);
    rejectAllocation = false;
    check(!prepared && !fresh.process(ptr, 4), "allocation failure leaves unprepared");
}
void width(double rate) {
    for (float value : {0.f, .5f, 1.f, 2.f}) {
        darkvoice::ModulationEffects fx;
        check(fx.prepare(rate, 64, 2) && fx.setParameters({.8f, 6, 0, value}), "width prepare");
        fx.reset();
        std::array<float, 64> l{}, r{};
        for (std::size_t i = 0; i < l.size(); ++i) {
            l[i] = static_cast<float>(.2 * std::sin(i * .17));
            r[i] = static_cast<float>(.1 * std::cos(i * .13));
        }
        const auto a = l, b = r;
        std::array<float *, 2> ptr{l.data(), r.data()};
        check(fx.process(ptr, l.size()), "width process");
        bool algebra = true, mono = true;
        for (std::size_t i = 0; i < l.size(); ++i) {
            const double mid = .5 * (static_cast<double>(a[i]) + b[i]);
            const double side = .5 * (static_cast<double>(a[i]) - b[i]) * value;
            algebra = algebra && std::abs(l[i] - (mid + side)) < 1e-7 &&
                      std::abs(r[i] - (mid - side)) < 1e-7;
            mono = mono && std::abs((static_cast<double>(l[i]) + r[i]) -
                                    (static_cast<double>(a[i]) + b[i])) < 1e-7;
        }
        check(algebra && mono, "width algebra preserves mono sum");
        if (value == 1)
            check(l == a && r == b, "width one exact identity");
        if (value == 0)
            check(l == r, "width zero exact mono fold-down");
    }
    darkvoice::ModulationEffects wide;
    check(wide.prepare(rate, 2, 2) && wide.setParameters({.8f, 6, 0, 2}), "extreme width prepare");
    wide.reset();
    std::array<float, 2> sideLeft{.1f, std::numeric_limits<float>::max()},
        sideRight{-.1f, -std::numeric_limits<float>::max()};
    std::array<float *, 2> sidePtr{sideLeft.data(), sideRight.data()};
    check(wide.process(sidePtr, 2) && sideLeft[0] == .2f && sideRight[0] == -.2f &&
              sideLeft[1] == std::numeric_limits<float>::max() &&
              sideRight[1] == -std::numeric_limits<float>::max(),
          "side-only width doubles signal and safely bounds extreme peaks");
    darkvoice::ModulationEffects mono;
    check(mono.prepare(rate, 4, 1) && mono.setParameters({.8f, 6, 0, 2}), "mono prepare");
    mono.reset();
    std::array<float, 4> x{.2f, -.1f, 1e-21f, std::numeric_limits<float>::min()};
    const auto original = x;
    std::array<float *, 1> ptr{x.data()};
    check(mono.process(ptr, 4) && x == original, "mono width no-op and tiny-normal identity");
}
void chorus(double rate) {
    darkvoice::ModulationEffects impulse;
    check(impulse.prepare(rate, 4096, 1) && impulse.setParameters({.8f, 0, .5f, 1}),
          "fixed delay prepare");
    impulse.reset();
    std::vector<float> x(4096);
    x[0] = 1;
    check(render(impulse, x, nullptr, 37), "impulse render");
    const double delay = .015 * rate;
    const auto first = static_cast<std::size_t>(delay);
    const double fraction = delay - first;
    check(x[0] == .5f && std::abs(x[first] - .5 * (1 - fraction)) < 1e-6 &&
              std::abs(x[first + 1] - .5 * fraction) < 1e-6,
          "15ms fractional chorus echo");
    double sum = 0;
    for (float v : x)
        sum += v;
    check(std::abs(sum - 1) < 1e-6 &&
              std::all_of(x.begin() + 1, x.begin() + first, [](float v) { return v == 0; }),
          "no early chorus echo or feedback");
    for (auto p : {darkvoice::ModulationParameters{.2f, 3, .25f, 1},
                   darkvoice::ModulationParameters{5, 12, .5f, 1}}) {
        darkvoice::ModulationEffects fx;
        check(fx.prepare(rate, 1024, 2) && fx.setParameters(p), "modulation prepare");
        fx.reset();
        std::vector<float> l(static_cast<std::size_t>(rate)), r;
        for (std::size_t i = 0; i < l.size(); ++i)
            l[i] = static_cast<float>(i / rate);
        r = l;
        check(render(fx, l, &r, 257), "ramp render");
        bool oracle = true, stereoDifferent = false;
        for (std::size_t i = static_cast<std::size_t>(rate * .05); i < l.size(); i += 997) {
            for (std::size_t c = 0; c < 2; ++c) {
                const double phase = 2 * std::numbers::pi * (p.chorusRateHz * i / rate + .25 * c);
                const double expected =
                    i / rate - p.chorusMix * (.015 + .001 * p.chorusDepthMs * std::sin(phase));
                oracle = oracle && std::abs((c == 0 ? l[i] : r[i]) - expected) < 1e-6;
            }
            stereoDifferent = stereoDifferent || std::abs(l[i] - r[i]) > 1e-4;
        }
        check(oracle && stereoDifferent, "rate depth mix and stereo phase match delay oracle");
    }
    darkvoice::ModulationEffects one, split;
    check(one.prepare(rate, 65536, 2) && split.prepare(rate, 65536, 2), "partition prepare");
    const darkvoice::ModulationParameters p{2, 12, .5f, 1.6f};
    check(one.setParameters(p) && split.setParameters(p), "partition parameters");
    std::vector<float> a(6000), b(6000);
    for (std::size_t i = 0; i < a.size(); ++i) {
        a[i] = static_cast<float>(.2 * std::sin(i * .1));
        b[i] = a[i] * .3f;
    }
    auto c = a, d = b;
    check(render(one, a, &b, 6000) && render(split, c, &d, 37) && a == c && b == d,
          "variable buffer partition invariance");
    std::array<float *, 2> bad{nullptr, nullptr};
    check(!one.process(bad, 1), "warm invalid block reject");
    auto invalid = p;
    invalid.stereoWidth = 3;
    check(!one.setParameters(invalid), "warm invalid snapshot reject");
    check(render(one, a, &b, 257) && render(split, c, &d, 257) && a == c && b == d,
          "invalid operations preserve targets and history");
    check(one.setParameters({.8f, 6, 0, 1}), "disable chorus and width");
    a.assign(static_cast<std::size_t>(rate), .1f);
    b = a;
    check(render(one, a, &b, 37) && a.back() == .1f && b.back() == .1f,
          "automation settles to neutral");
    std::array<float, 8> left{}, right{};
    std::array<float *, 2> ptr{left.data(), right.data()};
    const auto before = allocations.load();
    rejectAllocation = true;
    const bool processed = one.setParameters(p) && one.process(ptr, 8);
    one.reset();
    rejectAllocation = false;
    check(processed && allocations.load() == before, "no callback allocation");
    left = {std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(),
            std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()};
    right = left;
    check(one.process(ptr, 8) &&
              std::all_of(left.begin(), left.end(), [](float v) { return std::isfinite(v); }) &&
              std::all_of(right.begin(), right.end(), [](float v) { return std::isfinite(v); }),
          "hostile samples remain finite");
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
    if (void *p = std::malloc(size == 0 ? 1 : size))
        return p;
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
    for (double rate : {44100., 48000., 88200., 96000.}) {
        width(rate);
        chorus(rate);
    }
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
