#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <darkvoice/LatencyAlignedMix.hpp>
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
bool render(darkvoice::LatencyAlignedMix &mix, const std::vector<float> &dryLeft,
            const std::vector<float> &dryRight, std::vector<float> &left, std::vector<float> &right,
            std::size_t channels, std::size_t block) {
    for (std::size_t offset = 0; offset < left.size(); offset += block) {
        std::array<const float *, 2> dry{dryLeft.data() + offset, dryRight.data() + offset};
        std::array<float *, 2> wet{left.data() + offset, right.data() + offset};
        if (!mix.process(std::span<const float *>(dry.data(), channels),
                         std::span<float *>(wet.data(), channels),
                         std::min(block, left.size() - offset)))
            return false;
    }
    return true;
}
void signals(double rate, std::size_t channels, std::size_t delay) {
    darkvoice::LatencyAlignedMix mixer;
    check(mixer.prepare(rate, 512, channels, delay) && mixer.latencySamples() == delay,
          "matrix prepare and reported delay");
    std::vector<float> dry(delay + 2048), dryRight(dry.size()), wet(dry.size()), right(dry.size());
    dry[0] = .5f;
    dry[1] = 1e-21f;
    dry[2] = -std::numeric_limits<float>::min();
    dryRight[0] = -.25f;
    for (float amount : {0.f, .5f, 1.f}) {
        check(mixer.setParameters({amount, false}), "mix target");
        mixer.reset();
        std::fill(wet.begin(), wet.end(), 0);
        std::fill(right.begin(), right.end(), 0);
        // A synthetic wet backend with the same delay and twice the dry gain.
        for (std::size_t i = delay; i < wet.size(); ++i) {
            wet[i] = dry[i - delay] * 2;
            right[i] = dryRight[i - delay] * 2;
        }
        check(render(mixer, dry, dryRight, wet, right, channels, 37), "aligned matrix render");
        bool exact = true;
        for (std::size_t i = 0; i < wet.size(); ++i) {
            const float expected = i < delay ? 0 : dry[i - delay] * (1 + amount);
            exact = exact && wet[i] == expected;
            if (channels == 2)
                exact = exact && right[i] == (i < delay ? 0 : dryRight[i - delay] * (1 + amount));
        }
        check(exact, "aligned impulses blend without combing, tiny endpoints exact");
    }
    mixer.setParameters({1, true});
    mixer.reset();
    std::fill(wet.begin(), wet.end(), .9f);
    std::fill(right.begin(), right.end(), .9f);
    check(render(mixer, dry, dryRight, wet, right, channels, 512) && wet[delay] == .5f &&
              (channels == 1 || right[delay] == -.25f),
          "settled bypass retains configured dry latency");
}
void automation() {
    darkvoice::LatencyAlignedMix one, split;
    check(one.prepare(48000, 512, 2, 1152) && split.prepare(48000, 512, 2, 1152),
          "automation prepare");
    std::vector<float> dry(24000), dryRight(dry.size());
    for (std::size_t i = 0; i < dry.size(); ++i) {
        dry[i] = static_cast<float>(.1 * std::sin(.017 * i));
        dryRight[i] = -dry[i];
    }
    auto left = dry, right = dryRight, otherLeft = dry, otherRight = dryRight;
    const auto run = [&](darkvoice::LatencyAlignedMix &m, std::vector<float> &l,
                         std::vector<float> &r, std::size_t block) {
        std::size_t offset = 0;
        while (offset < l.size()) {
            if (offset == 137 && !m.setParameters({.25f, false}))
                return false;
            if (offset == 4093 && !m.setParameters({1, true}))
                return false;
            const auto next = offset < 137 ? 137U : offset < 4093 ? 4093U : l.size();
            const auto frames = std::min(block, next - offset);
            std::array<const float *, 2> d{dry.data() + offset, dryRight.data() + offset};
            std::array<float *, 2> w{l.data() + offset, r.data() + offset};
            if (!m.process(d, w, frames))
                return false;
            offset += frames;
        }
        return true;
    };
    const auto before = allocations.load();
    reject = true;
    const bool ok = run(one, left, right, 512) && run(split, otherLeft, otherRight, 37);
    reject = false;
    check(ok && allocations.load() == before, "automated callback processing allocates nothing");
    check(left == otherLeft && right == otherRight,
          "sample-positioned automation partition invariant");
    bool settled = true;
    for (std::size_t i = 16000; i < left.size(); ++i)
        settled = settled && left[i] == dry[i - 1152] && right[i] == dryRight[i - 1152];
    check(settled, "bypass automation settles exactly while dry history stays warm");
    // Rejection must preserve already warmed dry rings and parameter targets.
    std::array<const float *, 2> invalid{nullptr, nullptr};
    std::array<float *, 2> w{left.data(), right.data()};
    check(!one.process(invalid, w, 1) && !one.setParameters({2, false}), "warmed rejection");
    check(render(one, dry, dryRight, left, right, 2, 257) &&
              render(split, dry, dryRight, otherLeft, otherRight, 2, 257) && left == otherLeft &&
              right == otherRight,
          "rejection preserves warmed history");
}
void ramp() {
    darkvoice::LatencyAlignedMix mixer;
    check(mixer.prepare(48000, 512, 1, 0), "ramp prepare");
    std::vector<float> dry(12000, 0), other(12000, 0), wet(12000, 1), right(12000, 1);
    check(mixer.setParameters({1, true}) && render(mixer, dry, other, wet, right, 1, 512),
          "ramp render");
    double jump = std::abs(1. - wet[0]);
    for (std::size_t i = 1; i < wet.size(); ++i)
        jump = std::max(jump, std::abs(static_cast<double>(wet[i]) - wet[i - 1]));
    check(jump < .003 && wet[479] > .36f && wet[479] < .38f && wet.back() == 0,
          "bypass ramp bounds sample discontinuity, measures10ms decay and settles exactly");
}
void safety() {
    darkvoice::LatencyAlignedMix m;
    std::array<float, 16> x{1}, y{2};
    std::array<const float *, 2> d{x.data(), y.data()};
    std::array<float *, 2> w{x.data(), y.data()};
    check(!m.process(d, w, 1), "unprepared rejects");
    check(!m.prepare(32000, 16, 2, 1) && !m.prepare(48000, 0, 2, 1) &&
              !m.prepare(48000, 65537, 2, 1) && !m.prepare(48000, 16, 3, 1) &&
              !m.prepare(48000, 16, 2, 65537),
          "invalid prepare");
    check(m.prepare(48000, 16, 2, 2), "safety prepare");
    for (float bad : {-1.f, 2.f, std::numeric_limits<float>::quiet_NaN(),
                      std::numeric_limits<float>::infinity()})
        check(!m.setParameters({bad, false}), "invalid snapshot");
    auto alias = w;
    alias[0] = y.data();
    check(!m.process(d, alias, 1) && x[0] == 1 && y[0] == 2,
          "cross-channel alias atomic rejection");
    check(!m.process(d, w, 17), "oversize rejection");
    check(m.setParameters({0, false}), "dry alias setup");
    m.reset();
    check(m.process(d, w, 16) && x[0] == 0 && x[2] == 1 && y[2] == 2,
          "same-channel in-place dry alias supported");
    const auto before = allocations.load();
    reject = true;
    m.reset();
    reject = false;
    check(allocations.load() == before, "reset allocates nothing");
    reject = true;
    const bool prepared = m.prepare(48000, 16, 2, 65536);
    reject = false;
    check(!prepared && m.latencySamples() == 0 && !m.process(d, w, 1),
          "allocation failure unprepares");
    check(m.prepare(48000, 16, 2, 0), "zero latency safety prepare");
    x.fill(std::numeric_limits<float>::max());
    y = x;
    x[0] = std::numeric_limits<float>::infinity();
    x[1] = std::numeric_limits<float>::quiet_NaN();
    x[2] = std::numeric_limits<float>::denorm_min();
    check(m.process(d, w, 16) && x[0] == 0 && x[1] == 0 && x[2] == 0 &&
              x[3] == std::numeric_limits<float>::max() &&
              std::all_of(y.begin(), y.end(), [](float v) { return std::isfinite(v); }),
          "hostile samples finite, normal endpoints retained");
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
            for (std::size_t delay : {0U, 1152U, 9216U, 18432U, 65536U})
                signals(rate, channels, delay);
    automation();
    ramp();
    safety();
    std::printf("%d checks %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
