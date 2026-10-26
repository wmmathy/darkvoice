// Finite developer trace equality establishes separation, not audio quality.
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <numbers>
#include <vector>
namespace phase_probe {
struct Event {
    std::uint64_t bits;
    std::uint32_t tag;
    bool operator==(const Event &) const = default;
};
struct Trace {
    std::vector<Event> storage = std::vector<Event>(4000000);
    std::size_t used = 0;
    bool overflow = false;
};
inline Trace *active = nullptr;
inline int mode = 0, call = 0;
inline std::uint64_t emitted = 0, zeroReference = 0, unity = 0, invalid = 0;
inline double maximumRelativeEnergyError = 0;
inline std::uint64_t rngDraws = 0, repeatedSnapshots = 0, fallbacks = 0, leaders = 0;
template <class T> void record(unsigned tag, const T &value) {
    static_assert(sizeof(T) <= sizeof(std::uint64_t));
    if (!active)
        return;
    if (tag == 40)
        ++rngDraws;
    if (tag == 2 && value == 0)
        ++repeatedSnapshots;
    if (tag == 33 && value != 0)
        ++fallbacks;
    if (tag == 34)
        ++leaders;
    if (active->used == active->storage.size()) {
        active->overflow = true;
        return;
    }
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(T));
    active->storage[active->used++] = {bits, tag | (static_cast<unsigned>(sizeof(T)) << 16)};
}
float gain(float measured, int channel, int band, float frequency) {
    switch (mode) {
    case 0:
        return 1;
    case 1:
        return measured;
    case 2: {
        const auto envelope = [](double hz) {
            return std::exp(-.5 * std::pow((hz - 600) / 100, 2)) + .002 / std::max(hz / 70, 1.);
        };
        const double ratio =
            envelope(frequency * 48000 * .5 / std::exp2(7. / 12)) / envelope(frequency * 48000);
        return static_cast<float>(ratio * ratio);
    }
    case 3:
        return band % 3 == 0 ? .25f : 4.f;
    case 4:
        return ((call / 3 + band) % 2) ? .0625f : 16.f;
    case 5:
        return band % 3 == 0 ? 0.f : 1.f;
    case 6:
        return channel == 0 ? .0625f : 16.f;
    default:
        return std::numeric_limits<float>::quiet_NaN();
    }
}
void emit(float reference, float desired, std::complex<float> before, std::complex<float> after) {
    ++emitted;
    if (!std::isfinite(reference) || !std::isfinite(desired) || reference < 0 || desired < 0 ||
        !std::isfinite(after.real()) || !std::isfinite(after.imag())) {
        ++invalid;
        return;
    }
    if (reference == 0) {
        ++zeroReference;
        if (after != std::complex<float>{})
            ++invalid;
        return;
    }
    if (reference == desired) {
        ++unity;
        if (std::bit_cast<std::array<std::uint32_t, 2>>(before) !=
            std::bit_cast<std::array<std::uint32_t, 2>>(after))
            ++invalid;
    }
    const double expected = std::norm(std::complex<double>(before)) * desired / reference;
    const double actual = std::norm(std::complex<double>(after));
    const double error = std::abs(actual - expected) /
                         std::max(expected, static_cast<double>(std::numeric_limits<float>::min()));
    maximumRelativeEnergyError = std::max(maximumRelativeEnergyError, error);
    if (error > 2e-6)
        ++invalid;
}
// Fully observed state and draws; no random_device or allocation.
struct Engine {
    using result_type = std::uint32_t;
    std::uint32_t state = 1;
    explicit Engine(long seed = 1) : state(static_cast<std::uint32_t>(seed) + 1) {}
    static constexpr result_type min() {
        return 1;
    }
    static constexpr result_type max() {
        return 2147483646;
    }
    result_type operator()() {
        state = static_cast<std::uint32_t>(static_cast<std::uint64_t>(state) * 48271 % 2147483647);
        record(40, state);
        return state;
    }
};
} // namespace phase_probe
#define DARKVOICE_PHASE_TRACE(tag, value) phase_probe::record(tag, value)
#define DARKVOICE_PHASE_GAIN(value, channel, band, frequency)                                      \
    phase_probe::gain(value, channel, band, frequency)
#define DARKVOICE_PHASE_EMIT(reference, desired, before, after)                                    \
    phase_probe::emit(reference, desired, before, after)
#define DARKVOICE_PHASE_RNG(engine) phase_probe::record(41, engine.state)
#include <signalsmith-stretch.h>
namespace {
using Backend = signalsmith::stretch::SignalsmithStretch<float, phase_probe::Engine>;
void prepare(Backend &backend, bool split, int block) {
    backend.configure(2, block, block / 8, split);
    backend.setTransposeSemitones(-12);
    backend.setFormantSemitones(7, true);
    backend.setFormantBase(70.f / 48000);
    backend.reset();
}
bool invariant(bool split, int candidate) {
    Backend reference{0}, experiment{0};
    prepare(reference, split, 512);
    prepare(experiment, split, 512);
    phase_probe::Trace a, b;
    std::array<std::array<float, 1024>, 2> input{};
    std::array<std::array<float, 4096>, 2> outputA{}, outputB{};
    std::array<float *, 2> in{input[0].data(), input[1].data()};
    std::array<float *, 2> outA{outputA[0].data(), outputA[1].data()};
    std::array<float *, 2> outB{outputB[0].data(), outputB[1].data()};
    std::uint64_t compared = 0, changed = 0;
    std::size_t position = 0;
    constexpr std::array sizes{1, 37, 64, 137, 511};
    for (int call = 0; call < 160; ++call) {
        if (call == 80) {
            reference.reset();
            experiment.reset();
        }
        if (call == 120) {
            prepare(reference, split, 1024);
            prepare(experiment, split, 1024);
        }
        if (call == 20 || call == 36) {
            reference.setFormantSemitones(call == 20 ? 0 : 7, call != 20);
            experiment.setFormantSemitones(call == 20 ? 0 : 7, call != 20);
        }
        if (call == 25 || call == 40) {
            reference.setTransposeSemitones(call == 25 ? 0 : -12);
            experiment.setTransposeSemitones(call == 25 ? 0 : -12);
        }
        const int n = sizes[call % sizes.size()];
        // Repeated spectra and large stretch exercise randomized vertical twists.
        const int inputs = call % 11 == 0 ? 0 : n;
        const int outputs = n * (call % 7 == 0 ? 3 : 1);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < inputs; ++i) {
                const double t = static_cast<double>(position + i) / 48000;
                input[c][i] =
                    call >= 40 && call < 65
                        ? 0.f
                        : static_cast<float>(.05 * std::sin(2 * std::numbers::pi * 70 * t + c) +
                                             .02 * std::sin(2 * std::numbers::pi * 630 * t));
            }
        position += inputs;
        if (call == 100) {
            reference.seek(in, inputs, 1.0);
            experiment.seek(in, inputs, 1.0);
        }
        phase_probe::call = call;
        a.used = b.used = 0;
        phase_probe::active = &a;
        phase_probe::mode = 0;
        reference.process(in, inputs, outA, outputs);
        phase_probe::active = &b;
        phase_probe::mode = candidate;
        experiment.process(in, inputs, outB, outputs);
        phase_probe::active = nullptr;
        if (a.overflow || b.overflow || a.used != b.used ||
            !std::equal(a.storage.begin(), a.storage.begin() + a.used, b.storage.begin())) {
            std::size_t first = 0;
            while (first < std::min(a.used, b.used) && a.storage[first] == b.storage[first])
                ++first;
            std::fprintf(stderr, "invariant FAILED split%d mode%d call%d events%zu/%zu first%zu\n",
                         split, candidate, call, a.used, b.used, first);
            return false;
        }
        compared += a.used;
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < outputs; ++i) {
                if (!std::isfinite(outputA[c][i]) || !std::isfinite(outputB[c][i]))
                    return false;
                changed += std::bit_cast<std::uint32_t>(outputA[c][i]) !=
                           std::bit_cast<std::uint32_t>(outputB[c][i]);
            }
    }
    std::printf("invariant split%d mode%d comparedEvents%llu changedSamples%llu pass1\n", split,
                candidate, static_cast<unsigned long long>(compared),
                static_cast<unsigned long long>(changed));
    return candidate == 0 ? changed == 0 : changed > 0;
}
} // namespace
int main(int argc, char **) {
    if (argc != 1)
        return 2;
    for (bool split : {false, true})
        for (int mode = 0; mode <= 6; ++mode)
            if (!invariant(split, mode))
                return 1;
    std::printf(
        "emitted%llu zeroReference%llu exactUnity%llu invalid%llu maxEnergyRelativeError%.9g\n",
        static_cast<unsigned long long>(phase_probe::emitted),
        static_cast<unsigned long long>(phase_probe::zeroReference),
        static_cast<unsigned long long>(phase_probe::unity),
        static_cast<unsigned long long>(phase_probe::invalid),
        phase_probe::maximumRelativeEnergyError);
    std::printf("rngDraws%llu repeatedSnapshots%llu fallbackDecisions%llu channelLeaders%llu\n",
                static_cast<unsigned long long>(phase_probe::rngDraws),
                static_cast<unsigned long long>(phase_probe::repeatedSnapshots),
                static_cast<unsigned long long>(phase_probe::fallbacks),
                static_cast<unsigned long long>(phase_probe::leaders));
    return phase_probe::invalid == 0 && phase_probe::rngDraws > 0 &&
                   phase_probe::repeatedSnapshots > 0 && phase_probe::fallbacks > 0 &&
                   phase_probe::leaders > 0
               ? 0
               : 1;
}
