#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <darkvoice/SpaceReverb.hpp>
#include <limits>
#include <new>
#include <numbers>
#include <vector>
namespace {
std::atomic<std::size_t> allocations{0};
std::atomic<long> failAfter{-1};
int checks = 0, failures = 0;
void check(bool ok, const char *name) {
    ++checks;
    if (!ok) {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", name);
    }
}
std::size_t delay(double rate, double ms) {
    return static_cast<std::size_t>(std::llround(rate * ms / 1000));
}
bool render(darkvoice::SpaceReverb &fx, std::vector<float> &left, std::vector<float> *right,
            std::size_t block) {
    for (std::size_t offset = 0; offset < left.size(); offset += block) {
        std::array<float *, 2> ptr{left.data() + offset, right ? right->data() + offset : nullptr};
        if (!fx.process(std::span<float *>(ptr.data(), right ? 2 : 1),
                        std::min(block, left.size() - offset)))
            return false;
    }
    return true;
}
std::vector<float> signal(double rate, double seconds) {
    std::vector<float> values(static_cast<std::size_t>(rate * seconds));
    for (std::size_t i = 0; i < values.size(); ++i)
        values[i] = static_cast<float>(.2 * std::sin(.071 * i) + .05 * std::sin(.017 * i));
    return values;
}
std::vector<float> impulse(double rate, int model, float decay = .5f, float predelay = 0,
                           double seconds = .16, float toneHz = 8000) {
    darkvoice::SpaceReverb fx;
    check(fx.prepare(rate, 1024, 1) && fx.setParameters({model, decay, toneHz, predelay, .5f}),
          "impulse setup");
    fx.reset();
    std::vector<float> data(static_cast<std::size_t>(std::ceil(rate * seconds)));
    data[0] = 1;
    check(render(fx, data, nullptr, 257), "impulse render");
    for (std::size_t i = 0; i < data.size(); ++i)
        data[i] = 2 * data[i] - (i == 0 ? 1.f : 0.f);
    return data;
}
double energy(const std::vector<float> &data, std::size_t begin = 0) {
    double sum = 0;
    for (std::size_t i = begin; i < data.size(); ++i)
        sum += static_cast<double>(data[i]) * data[i];
    return sum;
}
void validation() {
    darkvoice::SpaceReverb fx;
    std::array<float, 8> l{1}, r{};
    std::array<float *, 2> ptr{l.data(), r.data()}, bad{l.data(), nullptr};
    check(!fx.process(ptr, 8), "unprepared rejects");
    for (double rate : {0., 32000., 192000., std::numeric_limits<double>::quiet_NaN()})
        check(!fx.prepare(rate, 8, 2), "rate invalid");
    check(!fx.prepare(48000, 0, 2) && !fx.prepare(48000, 65537, 2), "frame capacity bounds");
    check(!fx.prepare(48000, 8, 0) && !fx.prepare(48000, 8, 3), "layout bounds");
    check(fx.prepare(48000, 8, 2), "valid prepare");
    check(!fx.process(bad, 8) && l[0] == 1, "null rejects before partial write");
    bad[1] = bad[0];
    check(!fx.process(bad, 8) && l[0] == 1, "alias rejects atomically");
    check(!fx.process(ptr, 9) && l[0] == 1, "oversize rejection");
    check(!fx.process(std::span<float *>(ptr.data(), 1), 8), "wrong channel count");
    bad = {};
    check(fx.process(bad, 0), "zero-frame null allowed");
    for (int model : {-1, 3, std::numeric_limits<int>::max()})
        check(!fx.setParameters({model, .5f, 8000, 0, 0}), "invalid enum");
    for (auto field :
         {&darkvoice::SpaceReverbParameters::decay, &darkvoice::SpaceReverbParameters::toneHz,
          &darkvoice::SpaceReverbParameters::preDelayMs, &darkvoice::SpaceReverbParameters::mix})
        for (float value : {std::numeric_limits<float>::quiet_NaN(),
                            std::numeric_limits<float>::infinity(), -1.f, 16001.f}) {
            darkvoice::SpaceReverbParameters p;
            p.*field = value;
            check(!fx.setParameters(p), "invalid complete snapshot");
        }
    for (auto p : {darkvoice::SpaceReverbParameters{0, 1.001f, 8000, 0, 0},
                   {0, .5f, 999, 0, 0},
                   {0, .5f, 8000, 100.01f, 0},
                   {0, .5f, 8000, 0, .501f}})
        check(!fx.setParameters(p), "exact bounds");
    check(!fx.prepare(12345, 8, 2) && !fx.process(ptr, 8), "invalid reprepare unprepares");
    const auto beforePrepare = allocations.load();
    check(fx.prepare(48000, 8, 2), "allocation probe prepare");
    const auto prepareAllocations = static_cast<long>(allocations.load() - beforePrepare);
    check(prepareAllocations > 2, "all models allocate during prepare");
    std::printf("Prepare allocation sites exercised: %ld\n", prepareAllocations);
    for (long step = 0; step < prepareAllocations; ++step) {
        failAfter = step;
        const bool prepared = fx.prepare(48000, 8, 2);
        failAfter = -1;
        check(!prepared && !fx.process(ptr, 8),
              "allocation failures unprepare without partial processing");
        check(fx.prepare(48000, 8, 2), "recovery after allocation failure");
    }
}
void earlyOracle(double rate) {
    constexpr std::array<double, 6> roomTimes{3, 5, 8, 12, 17, 23};
    constexpr std::array<double, 6> roomWeights{.18, .144, .108, .078, .054, .036};
    constexpr std::array<double, 4> hallTimes{12, 19, 27, 37};
    constexpr std::array<double, 4> hallWeights{.08, .06, .04, .02};
    const double pole = std::exp(-2 * std::numbers::pi * 8000 / rate);
    for (int model = 0; model < 3; ++model) {
        const auto actual = impulse(rate, model);
        const auto end = model == 0   ? delay(rate, 29) + 1
                         : model == 1 ? delay(rate, 43) + 1
                                      : delay(rate, 23.9) + 1;
        double tone = 0;
        bool exact = true;
        for (std::size_t i = 0; i < end; ++i) {
            double raw = 0;
            if (model == 0)
                for (std::size_t j = 0; j < roomTimes.size(); ++j)
                    if (i == delay(rate, roomTimes[j]))
                        raw += roomWeights[j];
            if (model == 1)
                for (std::size_t j = 0; j < hallTimes.size(); ++j)
                    if (i == delay(rate, hallTimes[j]))
                        raw += hallWeights[j];
            if (model == 2 && i == delay(rate, 23.9))
                raw = (1 - (.25 + .70 * .5)) / 8;
            if (model == 0 && i == delay(rate, 29))
                raw += .40 * (1 - .60) / 4;
            if (model == 1 && i == delay(rate, 43))
                raw += .80 * (1 - .60) / 32;
            tone = pole * tone + (1 - pole) * raw;
            exact = exact && std::abs(actual[i] - tone) < 2e-8;
        }
        check(exact, "independent early coefficients and first-plate arrival with tone recurrence");
        std::size_t nonzero = 0;
        for (float value : actual)
            if (std::abs(value) > 1e-9f)
                ++nonzero;
        const auto lateStart = model == 0   ? delay(rate, 29)
                               : model == 1 ? delay(rate, 43)
                                            : delay(rate, 23.9);
        check(nonzero > 0 && energy(actual, lateStart) > 0,
              "structural late network response exists after analytically verified first arrival");
        std::printf("IR structure %.0fHz model%d: nonzero=%zu/%zu lateEnergy=%.9g\n", rate, model,
                    nonzero, actual.size(), energy(actual, lateStart));
        std::array<std::size_t, 3> occupied{};
        for (std::size_t i = 0; i < actual.size(); ++i) {
            const auto bin = static_cast<std::size_t>(i / (.05 * rate));
            if (bin < occupied.size() && std::abs(actual[i]) > 1e-9f)
                ++occupied[bin];
        }
        std::printf("Density50ms bins model%d: %zu %zu %zu\n", model, occupied[0], occupied[1],
                    occupied[2]);
    }
}
void predelayOracle(double rate) {
    for (int model = 0; model < 3; ++model) {
        const auto base = impulse(rate, model);
        for (float ms : {0.f, static_cast<float>(500.0 / rate), 10.3f, 100.f}) {
            const auto shifted = impulse(rate, model, .5f, ms, .30);
            const double samples = .001 * rate * static_cast<double>(ms);
            const auto integer = static_cast<std::size_t>(samples);
            const double fraction = samples - integer;
            bool exact = true;
            for (std::size_t i = 0; i < shifted.size(); ++i) {
                const auto read = [&](std::size_t d) {
                    return i >= d && i - d < base.size() ? base[i - d] : 0.f;
                };
                // Only compare where the finite reference contains all required history.
                if (i >= integer + base.size())
                    break;
                const double expected =
                    (1 - fraction) * read(integer) + fraction * read(integer + 1);
                exact = exact && std::abs(shifted[i] - expected) < 3e-8;
            }
            check(exact, "zero/subsample/integer/fractional predelay linear oracle");
        }
    }
}
void toneOracle(double rate) {
    for (int model = 0; model < 3; ++model) {
        const auto first = model == 0   ? delay(rate, 3)
                           : model == 1 ? delay(rate, 12)
                                        : delay(rate, 23.9);
        const double coefficient = model == 0 ? .18 : model == 1 ? .08 : (1 - .60) / 8;
        for (float hz : {1000.f, 16000.f}) {
            const auto response = impulse(rate, model, .5f, 0, .04, hz);
            const double expected = coefficient * (1 - std::exp(-2 * std::numbers::pi * hz / rate));
            check(std::abs(response[first] - expected) < 2e-8,
                  "tone control first arrival matches independent output-pole coefficient");
        }
    }
}
void crossfadeOracle(double rate) {
    const auto room = impulse(rate, 0), hall = impulse(rate, 1), plate = impulse(rate, 2);
    darkvoice::SpaceReverb fx;
    check(fx.prepare(rate, 65536, 1) && fx.setParameters({0, .5f, 8000, 0, .5f}),
          "crossfade oracle setup");
    fx.reset();
    std::vector<float> response(room.size());
    response[0] = 1;
    const auto firstSwitch = delay(rate, 50), secondSwitch = delay(rate, 100);
    std::array<float *, 1> ptr{response.data()};
    bool rendered = fx.process(ptr, firstSwitch);
    rendered = rendered && fx.setParameters({1, .5f, 8000, 0, .5f});
    ptr[0] = response.data() + firstSwitch;
    rendered = rendered && fx.process(ptr, secondSwitch - firstSwitch);
    rendered = rendered && fx.setParameters({2, .5f, 8000, 0, .5f});
    ptr[0] = response.data() + secondSwitch;
    rendered = rendered && fx.process(ptr, response.size() - secondSwitch);
    check(rendered, "crossfade oracle render");
    const double pole = std::exp(-2 * std::numbers::pi * 8000 / rate);
    const double survival = std::exp(-1 / (.020 * rate));
    const double roomAtSecond = std::pow(survival, secondSwitch - firstSwitch);
    double tone = 0;
    bool correct = true;
    for (std::size_t i = 0; i < response.size(); ++i) {
        const auto raw = [&](const auto &values) {
            return (values[i] - (i == 0 ? 0 : pole * values[i - 1])) / (1 - pole);
        };
        std::array<double, 3> weights{1, 0, 0};
        if (i >= secondSwitch) {
            const double remaining = std::pow(survival, i - secondSwitch + 1);
            weights = {roomAtSecond * remaining, (1 - roomAtSecond) * remaining, 1 - remaining};
        } else if (i >= firstSwitch) {
            const double remaining = std::pow(survival, i - firstSwitch + 1);
            weights = {remaining, 1 - remaining, 0};
        }
        const double blended =
            weights[0] * raw(room) + weights[1] * raw(hall) + weights[2] * raw(plate);
        tone = pole * tone + (1 - pole) * blended;
        const double actual = 2 * response[i] - (i == 0 ? 1.f : 0.f);
        correct = correct && std::abs(actual - tone) < 5e-8;
    }
    check(correct, "20ms convex model crossfade oracle preserves continuously running histories");
}

void stability(double rate) {
    for (std::size_t n : {4U, 8U}) {
        bool orthogonal = true;
        for (std::size_t i = 0; i < n; ++i)
            for (std::size_t j = 0; j < n; ++j) {
                int sum = 0;
                for (std::size_t k = 0; k < n; ++k)
                    sum +=
                        (std::popcount(i & k) % 2 ? -1 : 1) * (std::popcount(j & k) % 2 ? -1 : 1);
                orthogonal = orthogonal && sum == (i == j ? static_cast<int>(n) : 0);
            }
        check(orthogonal, "published normalized Hadamard matrix orthogonal and passive");
    }
    const auto lp = [&](double hz) {
        const double a = std::exp(-2 * std::numbers::pi * hz / rate);
        return (1 - a) / (1 - a * std::exp(.4 / rate));
    };
    const double room = .95 * std::exp(.4 * delay(rate, 53) / rate) * lp(10000);
    const double hall = .95 * std::exp(.4 * delay(rate, 113) / rate) * lp(6000);
    const double r = std::exp(.4 * delay(rate, 11.3) / rate);
    const double ap = (r - .5) / (1 - .5 * r);
    const double plate = .95 * std::exp(.4 * delay(rate, 31.3) / rate) * lp(8000) * ap;
    check(room < 1 && hall < 1 && plate < 1,
          "actual rounded exponential weighted feedback gains strictly below one");
    std::printf("Weighted bounds %.0fHz: room %.9f hall %.9f plate %.9f\n", rate, room, hall,
                plate);
}
void behavior(double rate, std::size_t channels) {
    darkvoice::SpaceReverb a, b;
    check(a.prepare(rate, 4096, channels) && b.prepare(rate, 4096, channels), "behavior prepare");
    auto left = signal(rate, .25), right = left;
    left[0] = 1e-21f;
    left[1] = -std::numeric_limits<float>::min();
    const auto dry = left, dryRight = right;
    check(render(a, left, channels == 2 ? &right : nullptr, 37) && left == dry &&
              (channels == 1 || right == dryRight),
          "neutral exact finite normal dry");
    check(a.setParameters({2, .9f, 4000, .001f, .5f}) &&
              b.setParameters({2, .9f, 4000, .001f, .5f}),
          "active parameters");
    a.reset();
    b.reset();
    left = signal(rate, .4);
    right = left;
    auto split = left, splitRight = right;
    check(render(a, left, channels == 2 ? &right : nullptr, 4096) &&
              render(b, split, channels == 2 ? &splitRight : nullptr, 37) && left == split &&
              (channels == 1 || right == splitRight),
          "active partition invariant all models running");
    const auto original = left;
    a.reset();
    left = signal(rate, .4);
    right = left;
    check(render(a, left, channels == 2 ? &right : nullptr, 257) && left == original,
          "reset deterministic all models");
    for (int model : {0, 1, 2}) {
        check(a.setParameters({model, 1, 16000, 100, .3f}) &&
                  b.setParameters({model, 1, 16000, 100, .3f}),
              "automated model target");
        left = signal(rate, .12);
        right = left;
        split = left;
        splitRight = right;
        check(render(a, left, channels == 2 ? &right : nullptr, 1024) &&
                  render(b, split, channels == 2 ? &splitRight : nullptr, 37) && left == split &&
                  (channels == 1 || right == splitRight),
              "five controls and model crossfade partition invariant");
    }
    std::array<float *, 2> bad{};
    check(!a.process(std::span<float *>(bad.data(), channels), 1) &&
              !a.setParameters({3, 1, 16000, 100, .3f}),
          "warmed invalid reject");
    check(a.process(std::span<float *>(bad.data(), channels), 0), "empty does not advance");
    left = signal(rate, .1);
    right = left;
    split = left;
    splitRight = right;
    check(render(a, left, channels == 2 ? &right : nullptr, 257) &&
              render(b, split, channels == 2 ? &splitRight : nullptr, 257) && left == split &&
              (channels == 1 || right == splitRight),
          "invalid warmed calls preserve every history/control");
    std::array<float, 128> l{}, r{};
    std::array<float *, 2> ptr{l.data(), r.data()};
    const auto before = allocations.load();
    failAfter = 0;
    bool safe = true;
    for (int model : {0, 1, 2}) {
        safe = safe && a.setParameters({model, 1, 16000, 100, .5f}) &&
               a.process(std::span<float *>(ptr.data(), channels), l.size());
        a.reset();
    }
    failAfter = -1;
    check(safe && allocations.load() == before, "callback/reset/model selection allocate nothing");
    for (int model = 0; model < 3; ++model) {
        check(a.setParameters({model, 1, 16000, 0, .5f}), "hostile model");
        a.reset();
        left.assign(static_cast<std::size_t>(rate * .2), std::numeric_limits<float>::max());
        right = left;
        left[0] = std::numeric_limits<float>::quiet_NaN();
        left[1] = std::numeric_limits<float>::infinity();
        left[2] = std::numeric_limits<float>::denorm_min();
        for (std::size_t i = 3; i < left.size(); ++i)
            if (i % 3)
                left[i] = -left[i];
        check(render(a, left, channels == 2 ? &right : nullptr, 257) &&
                  std::all_of(left.begin(), left.end(), [](float x) { return std::isfinite(x); }) &&
                  (channels == 1 || std::all_of(right.begin(), right.end(),
                                                [](float x) { return std::isfinite(x); })),
              "finite output with hostile floats and accumulated network energy");
    }
    a.reset();
    left.assign(static_cast<std::size_t>(rate * .2), 0);
    right = left;
    left[0] = 1;
    check(render(a, left, channels == 2 ? &right : nullptr, 257), "isolation render");
    if (channels == 2)
        check(std::all_of(right.begin(), right.end(), [](float x) { return x == 0; }),
              "no channel crossfeed");
}
void decayAndPassivity() {
    constexpr double rate = 48000;
    for (int model = 0; model < 3; ++model) {
        std::array<double, 3> normalized{};
        for (int index = 0; index < 3; ++index) {
            const auto response = impulse(rate, model, .5f * index, 0, 4);
            const double total = energy(response);
            normalized[index] = energy(response, static_cast<std::size_t>(rate)) / total;
            double previous = 1;
            bool descending = true;
            std::printf("Normalized decay curve model%d decay%.1f:", model, .5 * index);
            for (double seconds : {.1, .25, .5, 1., 2., 3.}) {
                const double remaining =
                    energy(response, static_cast<std::size_t>(rate * seconds)) / total;
                descending = descending && remaining <= previous;
                previous = remaining;
                std::printf(" %.9g", remaining);
            }
            std::printf("\n");
            check(descending, "normalized decay curve monotonically loses stored energy");
            check(total > 0 && total <= 1.000001, "normalized network impulse energy passive");
        }
        std::printf("Normalized one-second tails model%d: %.9g %.9g %.9g\n", model, normalized[0],
                    normalized[1], normalized[2]);
        check(normalized[0] < normalized[1] && normalized[1] < normalized[2],
              "normalized decay tail grows with decay control");
        const auto response = impulse(rate, model, 1, 0, 40);
        const double late = energy(response, static_cast<std::size_t>(rate * 35));
        const double total = energy(response);
        check(std::all_of(response.begin(), response.end(),
                          [](float x) { return std::isfinite(x); }) &&
                  late / total < 1e-10,
              "40second maximum-decay render finite and 35second residual energy negligible");
        std::printf("Long tail model%d late/total %.9g\n", model, late / total);
    }
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
        earlyOracle(rate);
        toneOracle(rate);
        crossfadeOracle(rate);
        predelayOracle(rate);
        stability(rate);
        for (std::size_t channels : {1U, 2U})
            behavior(rate, channels);
    }
    decayAndPassivity();
    std::printf("SpaceReverb checks: %d, failures: %d\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
