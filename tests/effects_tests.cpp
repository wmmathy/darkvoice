#include <darkvoice/ToneEffects.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <numbers>
#include <vector>

namespace {
std::atomic<std::size_t> allocations{0};
std::atomic<long> failAfter{-1};
int checks = 0, failures = 0;
void check(bool pass, const char *name) {
    ++checks;
    if (!pass) {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", name);
    }
}
bool run(darkvoice::ToneEffects &effects, std::vector<float> &data, std::size_t block = 257) {
    for (std::size_t offset = 0; offset < data.size(); offset += block) {
        std::array<float *, 1> channels{data.data() + offset};
        if (!effects.process(channels, std::min(block, data.size() - offset)))
            return false;
    }
    return true;
}
std::vector<float> tone(double rate, double frequency) {
    std::vector<float> data(static_cast<std::size_t>(rate / 2));
    for (std::size_t i = 0; i < data.size(); ++i)
        data[i] = static_cast<float>(0.1 * std::sin(2 * std::numbers::pi * frequency * i / rate));
    return data;
}
double energy(const std::vector<float> &x, std::size_t begin, std::size_t end) {
    double result = 0;
    for (std::size_t i = begin; i < end; ++i)
        result += static_cast<double>(x[i]) * x[i];
    return result;
}
void validation() {
    darkvoice::ToneEffects fx;
    std::array<float, 4> left{1, 0, 0, 0}, right{};
    std::array<float *, 2> stereo{left.data(), right.data()};
    check(!fx.process(stereo, 4), "unprepared rejects block");
    for (double rate : {0.0, 22050.0, std::numeric_limits<double>::quiet_NaN()})
        check(!fx.prepare(rate, 4, 2), "unsupported sample rate");
    check(!fx.prepare(48000, 65537, 1) && !fx.prepare(48000, 0, 1), "bounded frame capacity");
    check(!fx.prepare(48000, 4, 0) && !fx.prepare(48000, 4, 3), "mono stereo only");
    check(fx.prepare(48000, 4, 2), "stereo preparation");
    std::array<float *, 2> nullSecond{left.data(), nullptr}, alias{left.data(), left.data()};
    check(!fx.process(nullSecond, 4) && left[0] == 1, "null second channel no partial write");
    check(!fx.process(alias, 4) && left[0] == 1, "alias rejection");
    check(!fx.process(stereo, 5) && left[0] == 1, "oversized block rejection");
    check(!fx.process(std::span<float *>(stereo.data(), 1), 4), "wrong channel count");
    check(fx.process(nullSecond, 0), "zero frames allow null nonaliased pointers");
    check(!fx.prepare(12345, 4, 2) && !fx.process(stereo, 4), "invalid prepare unprepares");
    const std::array<float darkvoice::ToneEffectsParameters::*, 13> fields{
        &darkvoice::ToneEffectsParameters::eqLowHz,
        &darkvoice::ToneEffectsParameters::eqLowGainDb,
        &darkvoice::ToneEffectsParameters::eqMidHz,
        &darkvoice::ToneEffectsParameters::eqMidGainDb,
        &darkvoice::ToneEffectsParameters::eqMidQ,
        &darkvoice::ToneEffectsParameters::eqHighHz,
        &darkvoice::ToneEffectsParameters::eqHighGainDb,
        &darkvoice::ToneEffectsParameters::delayMs,
        &darkvoice::ToneEffectsParameters::delayFeedback,
        &darkvoice::ToneEffectsParameters::delayMix,
        &darkvoice::ToneEffectsParameters::reverbSize,
        &darkvoice::ToneEffectsParameters::reverbDamping,
        &darkvoice::ToneEffectsParameters::reverbMix};
    for (auto field : fields) {
        darkvoice::ToneEffectsParameters p;
        p.*field = std::numeric_limits<float>::infinity();
        check(!fx.setParameters(p) && !darkvoice::ToneEffects::valid(p),
              "nonfinite snapshot rejected");
        p.*field = 100000;
        check(!fx.setParameters(p), "out of range snapshot rejected");
    }
    for (long failure = 0; failure < 15; ++failure) {
        failAfter.store(failure);
        const bool prepared = fx.prepare(48000, 4, 2);
        failAfter.store(-1);
        check(!prepared && !fx.process(stereo, 4), "allocation failure caught and unprepares");
    }
    check(fx.prepare(48000, 4, 2), "recovery after allocation failure");
}
void equalization(double rate) {
    darkvoice::ToneEffects fx;
    check(fx.prepare(rate, 1024, 1), "rate preparation");
    for (int band = 0; band < 3; ++band) {
        for (float gain : {-9.0f, 9.0f}) {
            darkvoice::ToneEffectsParameters p;
            double hz = 1200;
            if (band == 0) {
                p.eqLowHz = 600;
                p.eqLowGainDb = gain;
                hz = 30;
            }
            if (band == 1) {
                p.eqMidGainDb = gain;
            }
            if (band == 2) {
                p.eqHighHz = 1500;
                p.eqHighGainDb = gain;
                hz = 18000;
            }
            auto data = tone(rate, hz);
            const double before = energy(data, data.size() / 2, data.size());
            check(fx.setParameters(p), "EQ parameter snapshot");
            fx.reset();
            check(run(fx, data, 1024), "EQ tone render");
            const double measured =
                10 * std::log10(energy(data, data.size() / 2, data.size()) / before);
            check(std::abs(measured - gain) < 0.15, "shelf and peak frequency gain in dB");
        }
    }
    check(fx.prepare(rate, 1024, 1), "reprepare neutral defaults");
    auto neutral = tone(rate, 1000), original = neutral;
    check(run(fx, neutral, 37) && neutral == original,
          "neutral EQ and spatial mixes preserve identity");
}
void delayAndReverb(double rate) {
    darkvoice::ToneEffects fx;
    check(fx.prepare(rate, 1024, 1), "spatial preparation");
    darkvoice::ToneEffectsParameters p;
    p.delayMs = 100;
    p.delayMix = 0.5f;
    p.delayFeedback = 0.5f;
    check(fx.setParameters(p), "delay snapshot");
    fx.reset();
    const auto period = static_cast<std::size_t>(rate / 10);
    std::vector<float> impulse(period * 4 + 1, 0);
    impulse[0] = 1;
    check(run(fx, impulse), "delay impulse render");
    check(std::abs(impulse[0] - 0.5f) < 1e-6 && std::abs(impulse[period] - 0.5f) < 1e-5 &&
              std::abs(impulse[2 * period] - 0.25f) < 1e-5 &&
              std::abs(impulse[3 * period] - 0.125f) < 1e-5,
          "delay time and feedback repeats");
    check(energy(impulse, 1, period) < 1e-15, "no early delay echo");
    p.delayMs = 20.3f;
    p.delayFeedback = 0;
    check(fx.setParameters(p), "fractional delay snapshot");
    fx.reset();
    impulse.assign(4096, 0);
    impulse[0] = 1;
    check(run(fx, impulse), "interpolated delay render");
    const double delay = static_cast<double>(p.delayMs) * rate * 0.001;
    const auto early = static_cast<std::size_t>(delay);
    const double fraction = delay - early;
    check(std::abs(impulse[early] - 0.5 * (1 - fraction)) < 1e-5 &&
              std::abs(impulse[early + 1] - 0.5 * fraction) < 1e-5,
          "linear fractional delay weights");
    p = {};
    p.reverbMix = 0.5f;
    p.reverbSize = 0.75f;
    p.reverbDamping = 0.6f;
    check(fx.setParameters(p), "reverb snapshot");
    fx.reset();
    impulse.assign(static_cast<std::size_t>(rate * 3), 0);
    impulse[0] = 1;
    check(run(fx, impulse), "reverb tail render on silent input");
    const double earlyEnergy = energy(impulse, static_cast<std::size_t>(rate * 0.05),
                                      static_cast<std::size_t>(rate * 0.4));
    const double lateEnergy = energy(impulse, static_cast<std::size_t>(rate * 2), impulse.size());
    check(earlyEnergy > 1e-5 && lateEnergy < earlyEnergy * 0.01, "reverb audible tail decays");
    fx.reset();
    std::vector<float> silence(2048, 0);
    check(run(fx, silence) && energy(silence, 0, silence.size()) == 0,
          "reset clears delay reverb histories");
}
void stereoAtRate(double rate) {
    darkvoice::ToneEffects fx, reference;
    check(fx.prepare(rate, 256, 2) && reference.prepare(rate, 256, 2),
          "stereo supported sample rate");
    darkvoice::ToneEffectsParameters p;
    p.eqMidGainDb = 8;
    p.delayMix = 0.5f;
    p.delayMs = 20;
    p.reverbMix = 0.5f;
    check(fx.setParameters(p) && reference.setParameters(p), "stereo active snapshot");
    fx.reset();
    reference.reset();
    std::array<float, 256> left{}, right{}, referenceLeft{}, referenceRight{};
    left[0] = referenceLeft[0] = 1;
    std::array<float *, 2> channels{left.data(), right.data()};
    std::array<float *, 2> expected{referenceLeft.data(), referenceRight.data()};
    std::array<float *, 2> invalid{left.data(), nullptr};
    check(!fx.process(invalid, 256) && left[0] == 1, "invalid stereo block untouched");
    check(fx.process(channels, 256) && reference.process(expected, 256) && left == referenceLeft &&
              right == referenceRight,
          "rejected block preserves effect state");
    double tail = 0;
    bool isolated = true;
    for (int block = 0; block < 100; ++block) {
        left.fill(0);
        check(fx.process(channels, 256), "stereo tail processing");
        for (std::size_t i = 0; i < left.size(); ++i) {
            tail += static_cast<double>(left[i]) * left[i];
            isolated = isolated && right[i] == 0 && std::isfinite(left[i]);
        }
    }
    check(tail > 1e-5 && isolated, "stereo tail nonzero without crosstalk");
}
void stateAndSafety() {
    darkvoice::ToneEffects first, second;
    check(first.prepare(48000, 65536, 1) && second.prepare(48000, 65536, 1), "partition setup");
    darkvoice::ToneEffectsParameters p;
    p.eqLowGainDb = 9;
    p.eqMidGainDb = -6;
    p.eqMidQ = 4;
    p.eqHighGainDb = 5;
    p.delayMs = 240.7f;
    p.delayMix = 0.4f;
    p.delayFeedback = 0.7f;
    p.reverbMix = 0.3f;
    p.reverbSize = 0.8f;
    p.reverbDamping = 0.7f;
    check(first.setParameters(p) && second.setParameters(p), "nonneutral snapshot");
    auto a = tone(48000, 753), b = a;
    auto invalid = p;
    invalid.delayMix = 0;
    invalid.eqMidQ = 0;
    check(!first.setParameters(invalid), "whole invalid snapshot rejection");
    std::array<float *, 1> empty{nullptr};
    check(first.process(empty, 0), "zero frames do not advance state");
    check(run(first, a, 65536) && run(second, b, 61) && a == b,
          "smoothing and effect partition equivalence");
    p.eqLowGainDb = -12;
    p.eqMidGainDb = 12;
    p.eqMidHz = 5000;
    p.eqMidQ = 6;
    p.eqHighHz = 12000;
    p.eqHighGainDb = -12;
    p.delayMs = 20;
    check(first.setParameters(p) && second.setParameters(p), "extreme automated snapshot");
    a = tone(48000, 2000);
    b = a;
    check(run(first, a, 777) && run(second, b, 23) && a == b &&
              std::all_of(a.begin(), a.end(), [](float x) { return std::isfinite(x); }),
          "automated coefficients remain finite and partition invariant");
    darkvoice::ToneEffects stereo;
    check(stereo.prepare(96000, 256, 2) && stereo.setParameters(p), "stereo effects setup");
    stereo.reset();
    std::array<float, 256> left{}, right{};
    std::array<float *, 2> channels{left.data(), right.data()};
    left[0] = 1;
    check(stereo.process(channels, 256) &&
              std::all_of(right.begin(), right.end(), [](float x) { return x == 0; }),
          "stereo effects isolated");
    left.fill(0);
    left[0] = std::numeric_limits<float>::quiet_NaN();
    left[1] = std::numeric_limits<float>::infinity();
    check(stereo.process(channels, 256) &&
              std::all_of(left.begin(), left.end(), [](float x) { return std::isfinite(x); }),
          "nonfinite input sanitized");
    const auto before = allocations.load();
    const bool snapshot = stereo.setParameters(p);
    stereo.reset();
    const bool processed = stereo.process(channels, 256);
    const auto after = allocations.load();
    check(snapshot && processed && before == after,
          "no ordinary callback snapshot reset allocations");
}
// Public parameter regressions for a tap just above an integer. In the old
// floating wrap, a negative epsilon could round up to the ring length.
void fractionalWrapRegression() {
    for (double rate : {44100., 48000., 88200., 96000.})
        for (std::size_t count : {1U, 2U}) {
            darkvoice::ToneEffects fx;
            check(fx.prepare(rate, 512, count), "near-integer wrap preparation");
            darkvoice::ToneEffectsParameters p;
            p.reverbSize = .03125f;
            p.reverbMix = .5f;
            check(fx.setParameters(p), "near-integer public parameters");
            constexpr std::size_t frames = 12000;
            std::array<std::vector<float>, 2> first, second;
            for (auto *pair : {&first, &second}) {
                for (auto &channel : *pair)
                    channel.assign(frames, 0);
                (*pair)[0][0] = .5f;
            }
            const auto process = [&](auto &data, std::size_t block) {
                fx.reset();
                for (std::size_t offset = 0; offset < frames; offset += block) {
                    std::array<float *, 2> channels{data[0].data() + offset,
                                                    data[1].data() + offset};
                    if (!fx.process(std::span<float *>(channels.data(), count),
                                    std::min(block, frames - offset)))
                        return false;
                }
                return true;
            };
            check(process(first, 512) && process(second, 113), "near-integer wrap renders");
            check(first == second, "near-integer reset and partition invariant");
            check(std::all_of(
                      first[0].begin(), first[0].end(),
                      [](float value) { return std::isfinite(value) && std::abs(value) <= .5f; }),
                  "near-integer impulse finite and bounded");
            check(std::all_of(first[1].begin(), first[1].end(),
                              [](float value) { return value == 0; }),
                  "near-integer wrap preserves silent channel");
        }
}
} // namespace

#if defined(__GNUC__) || defined(__clang__)
#define DV_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#define DV_NOINLINE __declspec(noinline)
#else
#define DV_NOINLINE
#endif
DV_NOINLINE void *operator new(std::size_t bytes) {
    allocations.fetch_add(1);
    const long failure = failAfter.load();
    if (failure >= 0 && failAfter.fetch_sub(1) == 0)
        throw std::bad_alloc();
    if (void *result = std::malloc(bytes == 0 ? 1 : bytes))
        return result;
    throw std::bad_alloc();
}
DV_NOINLINE void *operator new[](std::size_t bytes) {
    return ::operator new(bytes);
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
    for (double rate : {44100.0, 48000.0, 88200.0, 96000.0}) {
        equalization(rate);
        delayAndReverb(rate);
        stereoAtRate(rate);
    }
    fractionalWrapRegression();
    stateAndSafety();
    std::printf("Effects checks: %d, failures: %d\n", checks, failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
