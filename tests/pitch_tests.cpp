#include <array>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <darkvoice/PitchDetector.hpp>
#include <iostream>
#include <limits>
#include <new>
#include <numbers>
#include <random>
#include <vector>

namespace {
std::atomic<std::size_t> allocations{0};
int checks = 0, failures = 0;
void check(bool value, const char *message) {
    ++checks;
    if (!value) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}
} // namespace
#if defined(__GNUC__) || defined(__clang__)
#define PITCH_NOINLINE __attribute__((noinline))
#else
#define PITCH_NOINLINE
#endif
PITCH_NOINLINE void *operator new(std::size_t bytes) {
    ++allocations;
    if (auto *p = std::malloc(bytes == 0 ? 1 : bytes))
        return p;
    throw std::bad_alloc();
}
PITCH_NOINLINE void *operator new[](std::size_t bytes) {
    return ::operator new(bytes);
}
PITCH_NOINLINE void operator delete(void *p) noexcept {
    std::free(p);
}
PITCH_NOINLINE void operator delete[](void *p) noexcept {
    std::free(p);
}
PITCH_NOINLINE void operator delete(void *p, std::size_t) noexcept {
    std::free(p);
}
PITCH_NOINLINE void operator delete[](void *p, std::size_t) noexcept {
    std::free(p);
}

int main() {
    darkvoice::PitchDetector detector;
    std::vector<float> signal(8192);
    check(!detector.analyze(signal).voiced, "unprepared rejected");
    for (double rate : {44100.0, 48000.0, 88200.0, 96000.0}) {
        check(detector.prepare(rate), "prepare required rate");
        check(!detector.analyze(signal).voiced, "silence unvoiced");
        for (double frequency : {40.0, 55.0, 82.4069, 110.0, 220.0, 440.0, 880.0, 1000.0}) {
            for (std::size_t i = 0; i < signal.size(); ++i)
                signal[i] =
                    static_cast<float>(0.3 + 0.2 * std::sin(2.0 * std::numbers::pi * frequency *
                                                            static_cast<double>(i) / rate));
            const auto before = allocations.load();
            const auto pitch = detector.analyze(signal);
            const auto after = allocations.load();
            check(after == before, "analysis allocates no ordinary C++ storage");
            if (!pitch.voiced)
                std::cerr << "Unvoiced fixture rate=" << rate << " frequency=" << frequency << "\n";
            check(pitch.voiced, "sine with DC is voiced");
            const double cents =
                pitch.voiced ? 1200.0 * std::log2(pitch.frequencyHz / frequency) : 1.0e9;
            check(std::abs(cents) < 5.0, "sine accuracy within five cents");
            check(pitch.confidence >= 0.8f && pitch.confidence <= 1.0f, "confidence bounded");
            if (frequency == 440.0)
                check(std::abs(pitch.midiNote - 69.0) < 0.05, "A4 MIDI mapping");
        }
        for (std::size_t i = 0; i < signal.size(); ++i) {
            const auto phase = 2.0 * std::numbers::pi * 110.0 * static_cast<double>(i) / rate;
            signal[i] = static_cast<float>(0.4 * std::sin(phase) + 0.7 * std::sin(2 * phase) +
                                           0.25 * std::sin(3 * phase));
        }
        const auto harmonic = detector.analyze(signal);
        check(harmonic.voiced && std::abs(1200.0 * std::log2(harmonic.frequencyHz / 110.0)) < 5.0,
              "harmonic-rich signal finds fundamental");
        std::mt19937 generator(12345);
        std::uniform_real_distribution<float> random(-0.5f, 0.5f);
        for (auto &value : signal)
            value = random(generator);
        check(!detector.analyze(signal).voiced, "seeded noise unvoiced");
        signal.assign(8192, 0.4f);
        check(!detector.analyze(signal).voiced, "DC unvoiced");
        signal[10] = std::numeric_limits<float>::quiet_NaN();
        check(!detector.analyze(signal).voiced, "nonfinite rejected");
        check(!detector.analyze(std::span<const float>(signal.data(), 100)).voiced,
              "short input rejected");
        signal.assign(8192, 0.0f);
    }
    check(!detector.prepare(32000), "unsupported rate rejected");
    check(!detector.analyze(signal).voiced, "failed prepare unprepares");
    check(!detector.prepare(96000, 1024, 40, 1000), "insufficient periods rejected");
    check(!detector.prepare(48000, 20000), "oversized window rejected");
    check(!detector.prepare(48000, 8192, 1, 1000), "invalid lower bound rejected");
    check(!detector.prepare(48000, 8192, 1000, 1000), "inverted frequency bounds rejected");
    check(!detector.prepare(48000, 8192, 40, std::numeric_limits<double>::infinity()),
          "nonfinite bound rejected");
    std::cout << "Pitch analysis checks: " << checks << ", failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
