#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <darkvoice/NoiseGate.hpp>
#include <limits>
#include <new>
#include <vector>

namespace {
std::atomic<std::size_t> allocations{0};
int checks = 0, failures = 0;
void check(bool condition, const char *message) {
    ++checks;
    if (!condition) {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", message);
    }
}
bool render(darkvoice::NoiseGate &gate, std::vector<float> &left, std::vector<float> *right,
            std::size_t block) {
    for (std::size_t offset = 0; offset < left.size(); offset += block) {
        std::array<float *, 2> channels{left.data() + offset,
                                        right ? right->data() + offset : nullptr};
        if (!gate.process(std::span<float *>(channels.data(), right ? 2 : 1),
                          std::min(block, left.size() - offset)))
            return false;
    }
    return true;
}
void validation() {
    darkvoice::NoiseGate gate;
    std::array<float, 8> samples{1, 0};
    std::array<float *, 2> channels{samples.data(), nullptr};
    check(!gate.process(std::span<float *>(channels.data(), 1), 8), "unprepared rejects");
    for (double rate : {0.0, 32000.0, std::numeric_limits<double>::infinity()})
        check(!gate.prepare(rate, 8, 1), "invalid rate");
    check(!gate.prepare(48000, 0, 1) && !gate.prepare(48000, 65537, 1), "frame bounds");
    check(!gate.prepare(48000, 8, 0) && !gate.prepare(48000, 8, 3), "layout bounds");
    check(gate.prepare(48000, 8, 2), "prepare stereo");
    check(!gate.process(channels, 8) && samples[0] == 1, "null reject without partial write");
    check(gate.process(channels, 0), "empty block permits null");
    channels[1] = samples.data();
    check(!gate.process(channels, 8) && samples[0] == 1, "alias reject");
    channels[1] = samples.data() + 4;
    check(!gate.process(channels, 9) && samples[0] == 1, "oversize reject");
    check(!gate.process(std::span<float *>(channels.data(), 1), 4), "wrong layout reject");
    for (auto field :
         {&darkvoice::NoiseGateParameters::thresholdDb, &darkvoice::NoiseGateParameters::ratio,
          &darkvoice::NoiseGateParameters::releaseMs}) {
        for (float invalid : {std::numeric_limits<float>::quiet_NaN(),
                              std::numeric_limits<float>::infinity(), -1000.0f, 1001.0f}) {
            darkvoice::NoiseGateParameters p;
            p.*field = invalid;
            check(!gate.setParameters(p), "invalid snapshot rejects");
        }
    }
    check(!gate.prepare(12345, 8, 2) && !gate.process(channels, 4), "invalid prepare unprepares");
}
void signals(double rate) {
    const auto frames = static_cast<std::size_t>(rate);
    for (std::size_t count : {1U, 2U}) {
        darkvoice::NoiseGate gate;
        check(gate.prepare(rate, 1024, count), "supported preparation");
        std::vector<float> left(frames, 0.25f), right(frames, -0.125f);
        auto original = left;
        check(render(gate, left, count == 2 ? &right : nullptr, 37) && left == original,
              "default gate exact identity");
        if (count == 2)
            check(right.back() == -0.125f, "default stereo identity");
        check(gate.setParameters({-40, 2, 10}), "set active gate");
        gate.reset();
        left.assign(frames, 0.001f); // -60 dBFS -> -20 dB attenuation at ratio 2.
        right.assign(frames, -0.0005f);
        check(render(gate, left, count == 2 ? &right : nullptr, 257), "quiet render");
        check(std::abs(left.back() - 0.0001f) < 1.0e-8f, "measured downward expansion slope");
        if (count == 2)
            check(std::abs(right.back() / left.back() + 0.5f) < 1.0e-6f,
                  "stereo link preserves balance");
        left.assign(frames, 0.1f);
        right.assign(frames, 0.001f);
        check(render(gate, left, count == 2 ? &right : nullptr, 1024), "loud render");
        check(std::abs(left[static_cast<std::size_t>(rate * .010)] - 0.1f) < 0.0001f,
              "gate reopens within ten ms");
        if (count == 2)
            check(std::abs(right.back() - 0.001f) < 1.0e-8f,
                  "loud opposite channel opens quiet channel");
        check(gate.setParameters({-40, 1, 10}), "disable active gate");
        left.assign(frames, .001f);
        right.assign(frames, -.0005f);
        check(render(gate, left, count == 2 ? &right : nullptr, 257) && left.back() == .001f,
              "automation settles to neutral when ratio returns to one");
        const auto before = allocations.load();
        std::array<float, 32> a{}, b{};
        std::array<float *, 2> pointers{a.data(), b.data()};
        const bool processed = gate.setParameters({-60, 20, 120}) &&
                               gate.process(std::span<float *>(pointers.data(), count), 32);
        gate.reset();
        check(processed && allocations.load() == before, "callback operations do not allocate");
    }
    darkvoice::NoiseGate fast, slow;
    check(fast.prepare(rate, 1024, 1) && slow.prepare(rate, 1024, 1), "release prepare");
    check(fast.setParameters({-40, 2, 10}) && slow.setParameters({-40, 2, 500}), "release setup");
    fast.reset();
    slow.reset();
    std::vector<float> quick(static_cast<std::size_t>(rate * .1), .001f), gradual = quick;
    check(render(fast, quick, nullptr, 128) && render(slow, gradual, nullptr, 128),
          "release render");
    check(quick.back() < gradual.back() * .25f, "release control affects closing time");
    darkvoice::NoiseGate one, split;
    check(one.prepare(rate, 65536, 2) && split.prepare(rate, 65536, 2), "partition prepare");
    check(one.setParameters({-30, 4, 50}) && split.setParameters({-30, 4, 50}), "partition setup");
    std::vector<float> a(6000), b(6000);
    for (std::size_t i = 0; i < a.size(); ++i) {
        a[i] = static_cast<float>(.02 * std::sin(i * .07));
        b[i] = a[i] * -.3f;
    }
    auto c = a, d = b;
    check(render(one, a, &b, 6000) && render(split, c, &d, 37) && a == c && b == d,
          "variable block partition invariance");
    std::array<float, 4> dirty{std::numeric_limits<float>::infinity(),
                               std::numeric_limits<float>::quiet_NaN(),
                               std::numeric_limits<float>::max(), 1.0e-35f};
    darkvoice::NoiseGate clean;
    check(clean.prepare(rate, 4, 1), "sanitize prepare");
    std::array<float *, 1> ptr{dirty.data()};
    check(clean.process(ptr, 4) && dirty[0] == 0 && dirty[1] == 0 && std::isfinite(dirty[2]) &&
              dirty[3] == 0,
          "nonfinite input and subnormal state handling");
}
} // namespace
void *operator new(std::size_t bytes) {
    ++allocations;
    if (void *memory = std::malloc(bytes == 0 ? 1 : bytes))
        return memory;
    throw std::bad_alloc();
}
void *operator new[](std::size_t bytes) {
    return ::operator new(bytes);
}
void operator delete(void *memory) noexcept {
    std::free(memory);
}
void operator delete[](void *memory) noexcept {
    std::free(memory);
}
void operator delete(void *memory, std::size_t) noexcept {
    std::free(memory);
}
void operator delete[](void *memory, std::size_t) noexcept {
    std::free(memory);
}
int main() {
    validation();
    for (double rate : {44100.0, 48000.0, 88200.0, 96000.0})
        signals(rate);
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
