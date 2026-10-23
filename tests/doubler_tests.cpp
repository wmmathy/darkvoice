#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <darkvoice/Doubler.hpp>
#include <limits>
#include <new>
#include <vector>

namespace {
std::atomic<std::size_t> allocations{0};
std::atomic<long> failAfter{-1};
int checks = 0, failures = 0;
void check(bool ok, const char *message) {
    ++checks;
    if (!ok) {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", message);
    }
}
bool render(darkvoice::Doubler &fx, std::vector<float> &left, std::vector<float> *right,
            std::size_t block) {
    for (std::size_t offset = 0; offset < left.size(); offset += block) {
        std::array<float *, 2> ptr{left.data() + offset, right ? right->data() + offset : nullptr};
        if (!fx.process(std::span<float *>(ptr.data(), right ? 2 : 1),
                        std::min(block, left.size() - offset)))
            return false;
    }
    return true;
}
std::vector<float> signal(double rate, double seconds = 2) {
    std::vector<float> result(static_cast<std::size_t>(rate * seconds));
    for (std::size_t i = 0; i < result.size(); ++i)
        result[i] = static_cast<float>(.23 * std::sin(.139 * i) + .11 * std::sin(.017 * i));
    return result;
}
double distance(const std::vector<float> &a, const std::vector<float> &b) {
    double sum = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const double delta = static_cast<double>(a[i]) - b[i];
        sum += delta * delta;
    }
    return std::sqrt(sum / a.size());
}
void validation() {
    darkvoice::Doubler fx;
    std::array<float, 8> left{1}, right{};
    std::array<float *, 2> ptr{left.data(), right.data()};
    check(!fx.process(ptr, 8), "unprepared rejection");
    for (double rate : {0., 32000., 192000., std::numeric_limits<double>::quiet_NaN(),
                        std::numeric_limits<double>::infinity()})
        check(!fx.prepare(rate, 8, 2), "unsupported rate unprepares");
    check(!fx.prepare(48000, 0, 2) && !fx.prepare(48000, 65537, 2), "frame capacity bounds");
    check(!fx.prepare(48000, 8, 0) && !fx.prepare(48000, 8, 3), "channel bounds");
    check(fx.prepare(48000, 8, 2), "valid stereo prepare");
    auto bad = ptr;
    bad[1] = nullptr;
    check(!fx.process(bad, 8) && left[0] == 1, "null second channel rejects without partial write");
    bad[1] = bad[0];
    check(!fx.process(bad, 8) && left[0] == 1, "alias rejects without write");
    check(!fx.process(ptr, 9) && left[0] == 1, "oversized block rejects before read");
    check(!fx.process(std::span<float *>(ptr.data(), 1), 8), "wrong layout rejects");
    std::array<float *, 2> empty{};
    check(fx.process(empty, 0), "zero frame null buffers permitted");
    for (auto field :
         {&darkvoice::DoublerParameters::separationMs, &darkvoice::DoublerParameters::variationMs,
          &darkvoice::DoublerParameters::rateHz, &darkvoice::DoublerParameters::mix})
        for (float value : {std::numeric_limits<float>::quiet_NaN(),
                            std::numeric_limits<float>::infinity(), -1.f, 41.f}) {
            darkvoice::DoublerParameters p;
            p.*field = value;
            check(!fx.setParameters(p), "nonfinite/out-of-range snapshot rejected");
        }
    for (auto p : {darkvoice::DoublerParameters{9.99f, 0, .1f, 0},
                   darkvoice::DoublerParameters{40.01f, 0, .1f, 0},
                   darkvoice::DoublerParameters{20, 6.01f, .1f, 0},
                   darkvoice::DoublerParameters{20, 0, .099f, 0},
                   darkvoice::DoublerParameters{20, 0, 2.001f, 0},
                   darkvoice::DoublerParameters{20, 0, 1, .501f}})
        check(!fx.setParameters(p), "exact parameter upper/lower bounds");
    check(!fx.prepare(12345, 8, 2) && !fx.process(ptr, 8), "invalid reprepare unprepares");
    for (long failure : {0L, 1L}) {
        darkvoice::Doubler failing;
        failAfter = failure;
        const bool prepared = failing.prepare(48000, 8, 2);
        failAfter = -1;
        check(!prepared && !failing.process(ptr, 8), "allocation failure leaves unprepared");
        check(failing.prepare(48000, 8, 2), "prepare recovers from allocation failure");
    }
}
double impulseAt(std::size_t index, double delay) {
    const auto first = static_cast<std::size_t>(delay);
    const double fraction = delay - first;
    return index == first ? 1 - fraction : index == first + 1 ? fraction : 0;
}
void impulses(double rate, std::size_t channels) {
    for (float separation : {10.f, 20.3f, 40.f}) {
        darkvoice::Doubler fx;
        const darkvoice::DoublerParameters p{separation, 0, .4f, .5f};
        check(fx.prepare(rate, 257, channels) && fx.setParameters(p), "impulse prepare");
        fx.reset();
        std::vector<float> left(static_cast<std::size_t>(std::ceil(.060 * rate)) + 12),
            right(left.size());
        left[0] = .7f;
        right[0] = -.3f;
        check(render(fx, left, channels == 2 ? &right : nullptr, 257), "analytic impulse render");
        bool correct = true;
        const double a = rate * .001 * static_cast<double>(p.separationMs);
        const double b = rate * .001 * (static_cast<double>(p.separationMs) + 11);
        for (std::size_t i = 0; i < left.size(); ++i) {
            const double aWeight = channels == 1 ? .5 : .75;
            const double expectedLeft =
                .7f * ((i == 0 ? .5 : 0) +
                       .5 * (aWeight * impulseAt(i, a) + (1 - aWeight) * impulseAt(i, b)));
            correct = correct && std::abs(left[i] - expectedLeft) < 2e-7;
            if (channels == 2) {
                const double expectedRight =
                    -.3f *
                    ((i == 0 ? .5 : 0) + .5 * (.25 * impulseAt(i, a) + .75 * impulseAt(i, b)));
                correct = correct && std::abs(right[i] - expectedRight) < 2e-7;
            }
        }
        check(correct, "independent fractional impulse oracle and convex mono/stereo weights");
    }
}
void neutralAndSafety(double rate, std::size_t channels) {
    darkvoice::Doubler fx;
    check(fx.prepare(rate, 1024, channels), "neutral prepare");
    auto left = signal(rate, .1), right = left;
    left[0] = 1e-21f;
    left[1] = -std::numeric_limits<float>::min();
    const auto original = left, originalRight = right;
    check(render(fx, left, channels == 2 ? &right : nullptr, 37) && left == original &&
              (channels == 1 || right == originalRight),
          "mix0 preserves exact normal dry samples");
    std::array<float, 128> l{}, r{};
    std::array<float *, 2> ptr{l.data(), r.data()};
    const auto before = allocations.load();
    failAfter = 0;
    const bool safe = fx.setParameters({40, 6, 2, .5f}) &&
                      fx.process(std::span<float *>(ptr.data(), channels), l.size());
    fx.reset();
    failAfter = -1;
    check(safe && allocations.load() == before, "set/process/reset do not allocate");
    std::vector<float> dirty(4096), other(dirty.size());
    const float maximum = std::numeric_limits<float>::max();
    for (std::size_t i = 0; i < dirty.size(); ++i) {
        dirty[i] = (i & 1) ? maximum : -maximum;
        other[i] = -dirty[i];
    }
    dirty[0] = std::numeric_limits<float>::infinity();
    dirty[1] = std::numeric_limits<float>::quiet_NaN();
    dirty[2] = std::numeric_limits<float>::denorm_min();
    check(render(fx, dirty, channels == 2 ? &other : nullptr, 128) &&
              std::all_of(dirty.begin(), dirty.end(), [](float x) { return std::isfinite(x); }) &&
              (channels == 1 ||
               std::all_of(other.begin(), other.end(), [](float x) { return std::isfinite(x); })),
          "opposite finite float extremes and nonfinite input remain finite");
    check(fx.setParameters({20, 2, .4f, 0}), "restore neutral target");
    fx.reset();
    l.fill(0);
    l[0] = std::numeric_limits<float>::denorm_min();
    l[1] = std::numeric_limits<float>::quiet_NaN();
    l[2] = std::numeric_limits<float>::infinity();
    l[3] = 1e-21f;
    check(fx.process(std::span<float *>(ptr.data(), channels), l.size()) && l[0] == 0 &&
              l[1] == 0 && l[2] == 0 && l[3] == 1e-21f,
          "neutral sanitizes invalid/true subnormal and preserves tiny normal");
}
void trajectories(double rate, std::size_t channels) {
    darkvoice::Doubler a, b;
    check(a.prepare(rate, 65536, channels) && b.prepare(rate, 65536, channels),
          "trajectory prepare");
    const darkvoice::DoublerParameters active{30, 6, 2, .5f};
    check(a.setParameters(active) && b.setParameters(active), "trajectory setup");
    a.reset();
    b.reset();
    auto left = signal(rate), right = left, split = left, splitRight = right;
    check(render(a, left, channels == 2 ? &right : nullptr, 4096) &&
              render(b, split, channels == 2 ? &splitRight : nullptr, 37) && left == split &&
              (channels == 1 || right == splitRight),
          "active variable-block partition invariance");
    const auto first = left, firstRight = right;
    a.reset();
    left = signal(rate);
    right = left;
    check(render(a, left, channels == 2 ? &right : nullptr, 257) && left == first &&
              (channels == 1 || right == firstRight),
          "seeded reset reproduces random trajectories");
    check(a.setParameters({40, 0, .1f, 0}) && b.setParameters({40, 0, .1f, 0}), "automation setup");
    left = signal(rate, .5);
    right = left;
    split = left;
    splitRight = right;
    check(render(a, left, channels == 2 ? &right : nullptr, 4096) &&
              render(b, split, channels == 2 ? &splitRight : nullptr, 37) && left == split &&
              (channels == 1 || right == splitRight),
          "all four automated controls partition invariant");
    const auto dry = signal(rate, .5);
    check(std::equal(left.end() - 256, left.end(), dry.end() - 256),
          "mix automation settles exactly");
    std::array<float *, 2> invalid{};
    check(!a.process(std::span<float *>(invalid.data(), channels), 1), "invalid warmed null block");
    check(!a.setParameters({30, 6, 2, std::numeric_limits<float>::quiet_NaN()}),
          "invalid warmed snapshot rejects");
    check(a.process(std::span<float *>(invalid.data(), channels), 0),
          "empty block does not advance");
    left = signal(rate, .1);
    right = left;
    split = left;
    splitRight = right;
    check(render(a, left, channels == 2 ? &right : nullptr, 128) &&
              render(b, split, channels == 2 ? &splitRight : nullptr, 128) && left == split &&
              (channels == 1 || right == splitRight),
          "invalid calls preserve warmed targets/history/generator phase");
    darkvoice::Doubler staticTap, slow, fast;
    check(staticTap.prepare(rate, 512, channels) && slow.prepare(rate, 512, channels) &&
              fast.prepare(rate, 512, channels),
          "variation comparison prepare");
    check(staticTap.setParameters({30, 0, .1f, .5f}) && slow.setParameters({30, 6, .1f, .5f}) &&
              fast.setParameters(active),
          "variation comparison setup");
    staticTap.reset();
    slow.reset();
    fast.reset();
    auto fixed = signal(rate), varied = fixed, rapid = fixed;
    auto rFixed = fixed, rVaried = fixed, rRapid = fixed;
    check(render(staticTap, fixed, channels == 2 ? &rFixed : nullptr, 512) &&
              render(slow, varied, channels == 2 ? &rVaried : nullptr, 512) &&
              render(fast, rapid, channels == 2 ? &rRapid : nullptr, 512),
          "variation comparison render");
    check(distance(fixed, varied) > .02 && distance(varied, rapid) > .02,
          "variation and rate measurably change rendered audio");
    if (channels == 2)
        check(distance(rapid, rRapid) > .01, "independent trajectories produce stereo distinction");
}
void boundsAndDc(double rate, std::size_t channels) {
    darkvoice::Doubler fx;
    check(fx.prepare(rate, 257, channels) && fx.setParameters({40, 6, 2, .5f}), "bounded setup");
    fx.reset();
    std::vector<float> l(static_cast<std::size_t>(rate * .2), .25f), r(l.size(), -.5f);
    check(render(fx, l, channels == 2 ? &r : nullptr, 257), "DC render");
    check(std::all_of(l.end() - 128, l.end(), [](float x) { return x == .25f; }) &&
              (channels == 1 ||
               std::all_of(r.end() - 128, r.end(), [](float x) { return x == -.5f; })),
          "convex routing retains DC unity");
    fx.reset();
    const auto count = static_cast<std::size_t>(std::ceil(.080 * rate));
    l.assign(count, 0);
    r.assign(count, 0);
    l[0] = 1;
    check(render(fx, l, channels == 2 ? &r : nullptr, 257), "bounded impulse render");
    const auto tail = static_cast<std::size_t>(std::ceil(.057 * rate));
    bool bounded = true;
    for (std::size_t i = 1; i < l.size(); ++i)
        bounded = bounded && (i <= tail || l[i] == 0) && l[i] >= 0 && l[i] <= .5f;
    check(bounded, "causal nonnegative wet impulse bounded by 57ms with no feedback");
    if (channels == 2)
        check(std::all_of(r.begin(), r.end(), [](float x) { return x == 0; }), "no channel bleed");
    fx.reset();
    l = signal(rate, .3);
    r = l;
    auto original = l;
    check(render(fx, l, channels == 2 ? &r : nullptr, 257), "historic bound render");
    double peak = 0;
    bool historic = true;
    for (std::size_t i = 0; i < l.size(); ++i) {
        peak = std::max(peak, static_cast<double>(std::abs(original[i])));
        historic = historic && std::abs(l[i]) <= peak && (channels == 1 || std::abs(r[i]) <= peak);
    }
    check(historic, "output peak never exceeds historic input peak");
}
// Independent offline oracle: fixed PRNG knot fixtures, closed-form phase and
// absolute history positions, with no circular-buffer or incremental phase state.
void wanderingOracle(double rate, std::size_t channels) {
    constexpr std::array<double, 8> aKnots{2274908837., 358294691., 1210119364., 2176035992.,
                                           1882851208., 698933837., 2311737117., 3306712617.};
    constexpr std::array<double, 8> bKnots{1359758873., 3761132862., 2075758394., 25405621.,
                                           3862129951., 4186559031., 3122997712., 4244368831.};
    const auto wander = [](const auto &knots, double phase) {
        const auto knot = static_cast<std::size_t>(phase);
        const double t = phase - knot;
        const double blend = t * t * (3 - 2 * t);
        return 2 * ((1 - blend) * knots[knot] + blend * knots[knot + 1]) / 4294967295. - 1;
    };
    const auto read = [](const auto &history, double location) {
        const auto first = static_cast<long>(std::floor(location));
        const auto value = [&](long index) {
            return index < 0 ? 0.0 : static_cast<double>(history[static_cast<std::size_t>(index)]);
        };
        const double fraction = location - first;
        return value(first) * (1 - fraction) + value(first + 1) * fraction;
    };
    darkvoice::Doubler fx;
    check(fx.prepare(rate, 512, channels) && fx.setParameters({25, 6, 2, .5f}),
          "wander oracle setup");
    fx.reset();
    auto left = signal(rate, 1), right = left;
    for (std::size_t i = 0; i < right.size(); ++i)
        right[i] = static_cast<float>(-.2 * right[i] + .07 * std::cos(.031 * i));
    const auto dryLeft = left, dryRight = right;
    check(render(fx, left, channels == 2 ? &right : nullptr, 512), "wander oracle render");
    bool correct = true;
    for (std::size_t i = 0; i < left.size(); ++i) {
        const double delayA = .001 * rate * (25 + 6 * wander(aKnots, i * 2.0 / rate));
        const double delayB = .001 * rate * (36 + 6 * wander(bKnots, i * 2.0 * 1.173 / rate));
        const double weight = channels == 1 ? .5 : .75;
        const double expected = .5 * dryLeft[i] + .5 * (weight * read(dryLeft, i - delayA) +
                                                        (1 - weight) * read(dryLeft, i - delayB));
        correct = correct && std::abs(left[i] - expected) < 2e-6;
        if (channels == 2) {
            const double expectedRight = .5 * dryRight[i] + .5 * (.25 * read(dryRight, i - delayA) +
                                                                  .75 * read(dryRight, i - delayB));
            correct = correct && std::abs(right[i] - expectedRight) < 2e-6;
        }
    }
    check(correct, "independent closed-form random-knot and absolute-history oracle");
}

void foldDown(double rate) {
    darkvoice::Doubler mono, stereo;
    check(mono.prepare(rate, 512, 1) && stereo.prepare(rate, 512, 2) &&
              mono.setParameters({25, 6, 1, .5f}) && stereo.setParameters({25, 6, 1, .5f}),
          "fold-down setup");
    mono.reset();
    stereo.reset();
    auto m = signal(rate), l = m, r = m;
    check(render(mono, m, nullptr, 512) && render(stereo, l, &r, 512), "fold-down render");
    bool same = true;
    for (std::size_t i = 0; i < m.size(); ++i)
        same = same &&
               std::abs(static_cast<double>(m[i]) - .5 * (static_cast<double>(l[i]) + r[i])) < 4e-8;
    check(same, "dual-mono stereo fold-down matches mono within float rounding");
}
} // namespace
#if defined(__GNUC__) || defined(__clang__)
#define DV_NOINLINE __attribute__((noinline))
#else
#define DV_NOINLINE
#endif
DV_NOINLINE void *operator new(std::size_t size) {
    ++allocations;
    const auto remaining = failAfter.load();
    if (remaining >= 0) {
        if (remaining == 0)
            throw std::bad_alloc();
        --failAfter;
    }
    if (void *memory = std::malloc(size == 0 ? 1 : size))
        return memory;
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
        for (std::size_t channels : {1U, 2U}) {
            impulses(rate, channels);
            neutralAndSafety(rate, channels);
            trajectories(rate, channels);
            boundsAndDc(rate, channels);
            wanderingOracle(rate, channels);
        }
        foldDown(rate);
    }
    std::printf("Doubler checks: %d, failures: %d\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
