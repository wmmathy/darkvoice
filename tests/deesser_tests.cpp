#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <darkvoice/DeEsser.hpp>
#include <limits>
#include <new>
#include <numbers>
#include <vector>

namespace {
std::atomic<std::size_t> allocations{0};
int checks = 0, failures = 0;
void check(bool pass, const char *message) {
    ++checks;
    if (!pass) {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", message);
    }
}
bool render(darkvoice::DeEsser &fx, std::vector<float> &left, std::vector<float> *right,
            std::size_t block) {
    for (std::size_t offset = 0; offset < left.size(); offset += block) {
        std::array<float *, 2> channels{left.data() + offset,
                                        right ? right->data() + offset : nullptr};
        if (!fx.process(std::span<float *>(channels.data(), right ? 2 : 1),
                        std::min(block, left.size() - offset)))
            return false;
    }
    return true;
}
std::vector<float> tone(double rate, double frequency) {
    std::vector<float> result(static_cast<std::size_t>(rate));
    for (std::size_t i = 0; i < result.size(); ++i)
        result[i] = static_cast<float>(.1 * std::sin(2 * std::numbers::pi * frequency * i / rate));
    return result;
}
double energy(const std::vector<float> &data) {
    double sum = 0;
    for (std::size_t i = data.size() / 2; i < data.size(); ++i)
        sum += static_cast<double>(data[i]) * data[i];
    return sum;
}
double amplitude(const std::vector<float> &data, double rate, double frequency) {
    double sine = 0, cosine = 0;
    for (std::size_t i = data.size() / 2; i < data.size(); ++i) {
        const auto phase = 2 * std::numbers::pi * frequency * i / rate;
        sine += data[i] * std::sin(phase);
        cosine += data[i] * std::cos(phase);
    }
    return 2 * std::hypot(sine, cosine) / (data.size() - data.size() / 2);
}
void validation() {
    darkvoice::DeEsser fx;
    std::array<float, 8> data{1};
    std::array<float *, 2> ptr{data.data(), nullptr};
    check(!fx.process(std::span<float *>(ptr.data(), 1), 8), "unprepared rejects");
    for (double rate : {0.0, 32000.0, std::numeric_limits<double>::quiet_NaN()})
        check(!fx.prepare(rate, 8, 1), "invalid rate");
    check(!fx.prepare(48000, 0, 1) && !fx.prepare(48000, 65537, 1), "bounded capacity");
    check(!fx.prepare(48000, 8, 0) && !fx.prepare(48000, 8, 3), "layout bounds");
    check(fx.prepare(48000, 8, 2), "prepare stereo");
    check(!fx.process(ptr, 8) && data[0] == 1, "null second rejects without writes");
    check(fx.process(ptr, 0), "empty null channel allowed");
    ptr[1] = data.data();
    check(!fx.process(ptr, 8) && data[0] == 1, "alias rejects without writes");
    check(!fx.process(std::span<float *>(ptr.data(), 1), 8), "wrong channel count");
    check(!fx.process(ptr, 9) && data[0] == 1, "oversized block");
    for (auto field :
         {&darkvoice::DeEsserParameters::frequencyHz, &darkvoice::DeEsserParameters::thresholdDb,
          &darkvoice::DeEsserParameters::maxReductionDb})
        for (float invalid : {std::numeric_limits<float>::quiet_NaN(),
                              std::numeric_limits<float>::infinity(), -1000.0f, 13000.0f}) {
            darkvoice::DeEsserParameters p;
            p.*field = invalid;
            check(!fx.setParameters(p), "invalid snapshot rejected");
        }
    check(!fx.prepare(12345, 8, 2) && !fx.process(ptr, 4), "invalid prepare unprepares");
}
void signals(double rate) {
    for (std::size_t count : {1U, 2U}) {
        darkvoice::DeEsser fx;
        check(fx.prepare(rate, 1024, count), "supported preparation");
        auto signal = tone(rate, 8000), original = signal;
        auto right = signal;
        for (auto &x : right)
            x *= -.5f;
        check(render(fx, signal, count == 2 ? &right : nullptr, 37) && signal == original,
              "disabled exact identity");
        check(fx.setParameters({2000, -60, 12}), "active setup");
        fx.reset();
        signal = original;
        right = original;
        for (auto &x : right)
            x *= -.5f;
        check(render(fx, signal, count == 2 ? &right : nullptr, 257), "high tone render");
        const auto ratio = std::sqrt(energy(signal) / energy(original));
        check(ratio > .2 && ratio < .65, "measured high frequency reduction");
        if (count == 2) {
            bool linked = true;
            for (std::size_t i = signal.size() / 2; i < signal.size(); ++i)
                linked = linked && std::abs(right[i] + .5f * signal[i]) < 1.0e-7f;
            check(linked, "shared high-band gain preserves stereo balance");
        }
        check(fx.setParameters({2000, -30, 12}), "low tone setup");
        fx.reset();
        auto low = tone(rate, 100), lowOriginal = low;
        right = low;
        check(render(fx, low, count == 2 ? &right : nullptr, 512) && low == lowOriginal,
              "low tone below detector threshold preserved");
        check(fx.setParameters({2000, -6, 12}), "high threshold setup");
        fx.reset();
        signal = original;
        right = signal;
        check(render(fx, signal, count == 2 ? &right : nullptr, 128) && signal == original,
              "threshold control prevents attenuation below threshold");
        check(fx.setParameters({2000, -60, 18}), "maximum depth setup");
        fx.reset();
        signal = original;
        right = signal;
        check(render(fx, signal, count == 2 ? &right : nullptr, 128), "deep render");
        check(std::sqrt(energy(signal) / energy(original)) < ratio,
              "maximum reduction control changes actual audio");
        check(fx.setParameters({2000, -60, 0}), "disable via automation");
        signal = original;
        right = signal;
        check(render(fx, signal, count == 2 ? &right : nullptr, 37) &&
                  std::equal(signal.begin() + signal.size() * 15 / 16, signal.end(),
                             original.begin() + original.size() * 15 / 16),
              "disable automation returns to exact neutral");
        std::array<float, 32> a{}, b{};
        std::array<float *, 2> ptr{a.data(), b.data()};
        const auto before = allocations.load();
        const bool processed = fx.setParameters({6000, -24, 6}) &&
                               fx.process(std::span<float *>(ptr.data(), count), 32);
        fx.reset();
        check(processed && allocations.load() == before, "callback operations do not allocate");
    }
    darkvoice::DeEsser one, split;
    check(one.prepare(rate, 65536, 1) && split.prepare(rate, 65536, 1), "partition prepare");
    check(one.setParameters({2000, -60, 12}) && split.setParameters({2000, -60, 12}),
          "partition setup");
    auto a = tone(rate, 8000);
    for (std::size_t i = 0; i < a.size(); ++i)
        a[i] += static_cast<float>(.1 * std::sin(2 * std::numbers::pi * 100 * i / rate));
    auto b = a;
    check(render(one, a, nullptr, 60000) && render(split, b, nullptr, 37) && a == b,
          "partition invariant processing");
    std::array<float *, 1> invalidPtr{nullptr};
    check(!one.process(invalidPtr, 1), "invalid block after warmup rejects");
    auto invalidParameters = darkvoice::DeEsserParameters{2000, -60, 12};
    invalidParameters.maxReductionDb = std::numeric_limits<float>::quiet_NaN();
    check(!one.setParameters(invalidParameters), "invalid snapshot after warmup rejects");
    auto continuation = tone(rate, 8000), continuationReference = continuation;
    check(render(one, continuation, nullptr, 257) &&
              render(split, continuationReference, nullptr, 257) &&
              continuation == continuationReference,
          "rejected block and snapshot preserve warmed history and targets");
    check(amplitude(a, rate, 100) > .09 && amplitude(a, rate, 8000) < .065,
          "mixed signal preserves low band while reducing high band");
    darkvoice::DeEsser upper;
    check(upper.prepare(rate, 1024, 1) && upper.setParameters({12000, -60, 12}), "frequency setup");
    upper.reset();
    auto high = tone(rate, 8000);
    check(render(upper, high, nullptr, 512) &&
              amplitude(high, rate, 8000) > amplitude(a, rate, 8000) * 1.4,
          "higher split retains more high-frequency signal");
    darkvoice::DeEsser recovery;
    check(recovery.prepare(rate, 1024, 1) && recovery.setParameters({2000, -40, 12}),
          "recovery setup");
    recovery.reset();
    auto burst = tone(rate, 100);
    for (std::size_t i = 0; i < burst.size() / 4; ++i)
        burst[i] += static_cast<float>(.2 * std::sin(2 * std::numbers::pi * 8000 * i / rate));
    check(render(recovery, burst, nullptr, 257) && amplitude(burst, rate, 100) > .098,
          "high burst recovers while retaining low-band signal");
    darkvoice::DeEsser neutral;
    check(neutral.prepare(rate, 4, 1), "tiny normal setup");
    std::array<float, 4> tiny{1.0e-21f, -1.0e-21f, std::numeric_limits<float>::min(),
                              std::numeric_limits<float>::denorm_min()};
    std::array<float *, 1> tinyPtr{tiny.data()};
    check(neutral.process(tinyPtr, 4) && tiny[0] == 1.0e-21f && tiny[1] == -1.0e-21f &&
              tiny[2] == std::numeric_limits<float>::min() && tiny[3] == 0,
          "neutral preserves tiny normal samples and flushes true subnormals");
    std::array<float, 4> dirty{std::numeric_limits<float>::infinity(),
                               std::numeric_limits<float>::quiet_NaN(),
                               std::numeric_limits<float>::max(), 1.0e-35f};
    darkvoice::DeEsser clean;
    check(clean.prepare(rate, 4, 1) && clean.setParameters({2000, -60, 18}), "sanitize setup");
    clean.reset();
    std::array<float *, 1> ptr{dirty.data()};
    check(clean.process(ptr, 4) &&
              std::all_of(dirty.begin(), dirty.end(), [](float x) { return std::isfinite(x); }),
          "finite output for hostile input");
}
} // namespace
#if defined(__GNUC__) || defined(__clang__)
#define DV_NOINLINE __attribute__((noinline))
#else
#define DV_NOINLINE
#endif
DV_NOINLINE void *operator new(std::size_t size) {
    ++allocations;
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
    for (double rate : {44100.0, 48000.0, 88200.0, 96000.0})
        signals(rate);
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
