#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <darkvoice/StereoDelay.hpp>
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
bool render(darkvoice::StereoDelay &fx, std::vector<float> &l, std::vector<float> *r,
            std::size_t block) {
    for (std::size_t offset = 0; offset < l.size(); offset += block) {
        std::array<float *, 2> ptr{l.data() + offset, r ? r->data() + offset : nullptr};
        if (!fx.process(std::span<float *>(ptr.data(), r ? 2 : 1),
                        std::min(block, l.size() - offset)))
            return false;
    }
    return true;
}
void neutral(double rate, std::size_t channels) {
    darkvoice::StereoDelay fx;
    check(fx.prepare(rate, 512, channels), "neutral prepare");
    std::vector<float> left(4096), right(left.size());
    for (std::size_t i = 0; i < left.size(); ++i) {
        left[i] = static_cast<float>(.1 * std::sin(.017 * i));
        right[i] = -left[i];
    }
    left[0] = 1e-21f;
    left[1] = -std::numeric_limits<float>::min();
    const auto a = left, b = right;
    check(render(fx, left, channels == 2 ? &right : nullptr, 37) && left == a &&
              (channels == 1 || right == b),
          "mix0 exact dry including tiny normals");
    std::array<float, 512> l{}, r{};
    std::array<float *, 2> ptr{l.data(), r.data()};
    const auto before = allocations.load();
    reject = true;
    bool safe = true;
    for (int mode : {0, 1, 2}) {
        safe = safe && fx.setParameters({2000, .8f, .5f, 20, mode, true, 5}) &&
               fx.process(std::span<float *>(ptr.data(), channels), l.size());
        fx.reset();
    }
    reject = false;
    check(safe && allocations.load() == before,
          "all modes callbacks/setters/reset allocate nothing");
}
void echoes(double rate, std::size_t channels, int mode) {
    darkvoice::StereoDelay fx;
    check(fx.prepare(rate, 512, channels) && fx.setParameters({100, .5f, .5f, 120, mode, false, 5}),
          "echo mode setup");
    fx.reset();
    const auto interval = static_cast<std::size_t>(rate / 10);
    std::vector<float> left(4 * interval + 8), right(left.size());
    left[0] = .5f;
    right[0] = -.25f;
    check(render(fx, left, channels == 2 ? &right : nullptr, 257), "echo mode render");
    bool analytic = left[0] == .25f && (channels == 1 || right[0] == -.125f);
    for (int echo = 0; echo < 3; ++echo) {
        const double gain = mode == 1 && echo > 0 ? 0 : .5 * std::pow(.5, echo);
        const bool crossed = mode == 2 && channels == 2 && echo % 2;
        const double expectedLeft = gain * (crossed ? -.25 : .5);
        const double expectedRight = gain * (crossed ? .5 : -.25);
        const auto index = static_cast<std::size_t>(echo + 1) * interval;
        analytic = analytic && std::abs(left[index] - expectedLeft) < 1e-6 &&
                   (channels == 1 || std::abs(right[index] - expectedRight) < 1e-6);
    }
    check(analytic, "analytic repeat/single/stereo alternation or mono ping-pong fallback");
    const auto original = left;
    fx.reset();
    std::fill(left.begin(), left.end(), 0);
    std::fill(right.begin(), right.end(), 0);
    left[0] = .5f;
    right[0] = -.25f;
    check(render(fx, left, channels == 2 ? &right : nullptr, 19) && left == original,
          "reset retains targets and clears complete delay history");
}
void timing(double rate, double bpm, int division) {
    constexpr std::array<double, 6> beats{.25, 1. / 3, .5, 2. / 3, .75, 1};
    darkvoice::StereoDelay fx;
    check(fx.prepare(rate, 512, 1) &&
              fx.setParameters({20, 0, .5f, static_cast<float>(bpm), 0, true, division}),
          "sync timing setup");
    fx.reset();
    const double expected = rate * 60 / bpm * beats[division];
    std::vector<float> x(static_cast<std::size_t>(std::ceil(expected)) + 64);
    x[0] = .5f;
    check(render(fx, x, nullptr, 257), "sync timing render");
    double sum = 0, moment = 0;
    for (std::size_t i = 1; i < x.size(); ++i) {
        sum += x[i];
        moment += i * static_cast<double>(x[i]);
    }
    if (!(std::abs(sum - .25) < 1e-6 && std::abs(moment / sum - expected) < .001))
        std::printf("sync rate %.0f BPM %.0f division %d sum %.12g delay %.12g expected %.12g\n",
                    rate, bpm, division, sum, moment / sum, expected);
    check(std::abs(sum - .25) < 1e-6 && std::abs(moment / sum - expected) < .001,
          "beat division/BPM measured fractional echo timing incl3sec maximum");
}
void fractionalAndDecay() {
    darkvoice::StereoDelay fx;
    check(fx.prepare(44100, 512, 1) && fx.setParameters({123.4f, 0, .5f, 120, 0, false, 5}),
          "manual fractional setup");
    fx.reset();
    std::vector<float> x(6000);
    x[0] = .5f;
    check(render(fx, x, nullptr, 37), "manual fractional render");
    double sum = 0, moment = 0;
    for (std::size_t i = 1; i < x.size(); ++i) {
        sum += x[i];
        moment += i * static_cast<double>(x[i]);
    }
    check(std::abs(sum - .25) < 1e-6 && std::abs(moment / sum - 44.1 * 123.4f) < .001,
          "manual fractional delay centroid preserves echo amplitude");
    check(fx.prepare(48000, 512, 2) && fx.setParameters({20, .8f, .5f, 120, 2, false, 5}),
          "decay setup");
    fx.reset();
    x.assign(48000 * 4, 0);
    std::vector<float> right(x.size());
    x[0] = .5f;
    check(render(fx, x, &right, 512), "maximum crossfeedback render");
    bool geometric = true;
    for (int echo = 0; echo < 20; ++echo) {
        const auto index = static_cast<std::size_t>(echo + 1) * 960;
        const auto expected = .25 * std::pow(.8f, echo);
        geometric = geometric && std::abs((echo % 2 ? right[index] : x[index]) - expected) < 1e-6 &&
                    (echo % 2 ? x[index] : right[index]) == 0;
    }
    check(geometric,
          "maximum ping-pong feedback has bounded geometric decay without double writes");
    check(std::all_of(x.end() - 4800, x.end(), [](float v) { return std::abs(v) < 1e-15f; }) &&
              std::all_of(right.end() - 4800, right.end(),
                          [](float v) { return std::abs(v) < 1e-15f; }),
          "maximum feedback drains to tiny stable tail");
}
void partitions() {
    darkvoice::StereoDelay one, split;
    check(one.prepare(48000, 512, 2) && split.prepare(48000, 512, 2), "automation prepare");
    std::vector<float> a(30000), b(a.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        a[i] = static_cast<float>(.1 * std::sin(.017 * i));
        b[i] = static_cast<float>(.1 * std::sin(.023 * i));
    }
    auto c = a, d = b;
    const auto run = [&](darkvoice::StereoDelay &fx, std::vector<float> &l, std::vector<float> &r,
                         std::size_t block) {
        std::size_t offset = 0;
        while (offset < l.size()) {
            if (offset == 137 && !fx.setParameters({20, .8f, .5f, 300, 2, false, 0}))
                return false;
            if (offset == 8191 && !fx.setParameters({2000, .8f, .5f, 20, 1, true, 5}))
                return false;
            if (offset == 16003 && !fx.setParameters({100, .25f, 0, 120, 0, false, 5}))
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
          "time/sync/mode/route/mix automation partition invariant");
    std::array<float *, 2> invalid{a.data(), nullptr};
    check(!one.process(invalid, 1) && !one.setParameters({19, .8f, .5f, 20, 0, false, 5}),
          "warmed invalid calls rejected");
    std::array<float *, 2> empty{nullptr, nullptr};
    check(one.process(empty, 0), "zero frames accepted without state advance");
    check(render(one, a, &b, 257) && render(split, c, &d, 257) && a == c && b == d,
          "invalid/empty calls preserve warmed history and target");
    // Establish asymmetric steady feedback, then measure the route-change
    // discontinuity when the newly cross-fed writes reach the output tap.
    darkvoice::StereoDelay smooth;
    check(smooth.prepare(48000, 512, 2) && smooth.setParameters({20, .5f, .5f, 120, 0, false, 5}),
          "route smoothing setup");
    smooth.reset();
    std::vector<float> left(48000, .1f), right(left.size(), 0);
    check(render(smooth, left, &right, 512), "route feedback warmup");
    double previousLeft = left.back(), previousRight = right.back();
    check(smooth.setParameters({20, .5f, .5f, 120, 2, false, 5}), "smooth route transition");
    left.assign(9600, .1f);
    right.assign(left.size(), 0);
    check(render(smooth, left, &right, 37), "route transition render");
    double jump = 0, rightPeak = 0;
    for (std::size_t i = 0; i < left.size(); ++i) {
        jump =
            std::max({jump, std::abs(left[i] - previousLeft), std::abs(right[i] - previousRight)});
        rightPeak = std::max(rightPeak, static_cast<double>(right[i]));
        previousLeft = left[i];
        previousRight = right[i];
    }
    check(jump < .001 && rightPeak > .02,
          "feedback routing changes smoothly and reaches opposite channel");
    check(smooth.setParameters({20, .8f, .5f, 120, 1, false, 5}), "single transition setup");
    left.assign(9600, .1f);
    right.assign(left.size(), 0);
    check(render(smooth, left, &right, 257) && std::abs(left.back() - .1f) < 1e-6 &&
              std::abs(right.back()) < 1e-6,
          "single mode settles feedback to zero and drains previous routing history");
}
void validation() {
    darkvoice::StereoDelay fx;
    std::array<float, 512> a{1}, b{-1};
    std::array<float *, 2> ptr{a.data(), b.data()};
    check(!fx.process(ptr, 1), "unprepared rejects");
    for (double rate : {0., 32000., std::numeric_limits<double>::infinity()})
        check(!fx.prepare(rate, 512, 2), "invalid rate");
    check(!fx.prepare(48000, 0, 2) && !fx.prepare(48000, 65537, 2) && !fx.prepare(48000, 512, 3),
          "invalid capacity/layout");
    for (auto member :
         {&darkvoice::StereoDelayParameters::timeMs, &darkvoice::StereoDelayParameters::feedback,
          &darkvoice::StereoDelayParameters::mix, &darkvoice::StereoDelayParameters::bpm})
        for (float bad : {std::numeric_limits<float>::quiet_NaN(),
                          std::numeric_limits<float>::infinity(), -1.f, 2001.f}) {
            darkvoice::StereoDelayParameters p;
            p.*member = bad;
            check(!fx.setParameters(p), "invalid finite snapshot bounds");
        }
    for (int bad : {-1, 6}) {
        darkvoice::StereoDelayParameters p;
        p.mode = bad;
        check(!fx.setParameters(p), "invalid mode");
        p = {};
        p.division = bad;
        check(!fx.setParameters(p), "invalid division");
    }
    check(fx.prepare(48000, 512, 2), "validation prepare");
    ptr[1] = nullptr;
    check(!fx.process(ptr, 1) && a[0] == 1, "null atomic rejection");
    ptr[1] = a.data();
    check(!fx.process(ptr, 1) && a[0] == 1, "alias atomic rejection");
    ptr[1] = b.data();
    check(!fx.process(ptr, 513), "capacity rejection");
    check(fx.setParameters({20, .8f, .5f, 120, 2, false, 5}), "hostile setup");
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
    check(finite, "hostile float-range input and crossfeedback remain finite");
    darkvoice::StereoDelay fresh;
    reject = true;
    const bool prepared = fresh.prepare(96000, 512, 2);
    reject = false;
    check(!prepared && !fresh.process(ptr, 1), "allocation failure leaves unprepared");
    check(!fx.prepare(48000, 512, 0) && !fx.process(ptr, 1),
          "invalid reprepare unprepares warmed object");
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
    for (double rate : {44100., 48000., 88200., 96000.}) {
        for (std::size_t channels : {1U, 2U}) {
            neutral(rate, channels);
            for (int mode : {0, 1, 2})
                echoes(rate, channels, mode);
        }
        for (double bpm : {20., 120., 300.})
            for (int division = 0; division < 6; ++division)
                timing(rate, bpm, division);
    }
    fractionalAndDecay();
    partitions();
    validation();
    std::printf("%d checks %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
