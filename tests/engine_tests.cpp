#include <darkvoice/AudioEngine.hpp>

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
int failures = 0;
int checks = 0;
void check(bool condition, const char* message) {
    ++checks;
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); ++failures; }
}
bool run(darkvoice::AudioEngine& engine, std::vector<float>& data,
         std::size_t partition) {
    for (std::size_t offset = 0; offset < data.size(); offset += partition) {
        std::array<float*, 1> channel{data.data() + offset};
        if (!engine.process(channel, std::min(partition, data.size() - offset))) return false;
    }
    return true;
}
std::vector<float> sine(double rate, double frequency, double amplitude = 0.8) {
    std::vector<float> result(static_cast<std::size_t>(rate));
    for (std::size_t i = 0; i < result.size(); ++i)
        result[i] = static_cast<float>(amplitude * std::sin(2 * std::numbers::pi * frequency * i / rate));
    return result;
}
double rms(const std::vector<float>& data) {
    double sum = 0;
    for (std::size_t i = data.size() / 2; i < data.size(); ++i) sum += data[i] * data[i];
    return std::sqrt(sum / (data.size() - data.size() / 2));
}
void lifecycle() {
    darkvoice::AudioEngine engine;
    std::array<float, 2> left{0.5f, -0.5f}, right{0.25f, -0.25f};
    std::array<float*, 2> channels{left.data(), right.data()};
    check(!engine.process(channels, 2), "unprepared rejection");
    for (double rate : {0.0, -1.0, 22050.0, std::numeric_limits<double>::infinity()})
        check(!engine.prepare(rate, 2, 2), "invalid rate rejection");
    check(!engine.prepare(48000, 0, 2), "zero maximum rejection");
    check(!engine.prepare(48000, 2, 3), "unsupported channel rejection");
    check(engine.prepare(48000, 2, 2), "valid stereo preparation");
    std::array<float*, 2> nullLast{left.data(), nullptr};
    check(!engine.process(nullLast, 2) && left[0] == 0.5f, "all pointers validated before writes");
    std::array<float*, 2> aliases{left.data(), left.data()};
    check(!engine.process(aliases, 2) && left[0] == 0.5f, "aliased pointers rejected");
    check(!engine.process(channels, 3) && right[0] == 0.25f, "oversized block untouched");
    check(!engine.process(std::span<float*>(channels.data(), 1), 2), "wrong channel count");
    check(engine.process(nullLast, 0), "empty block permits null pointer");
    check(engine.latencySamples() == 0, "starter chain zero latency");
    check(!engine.prepare(48000, 2, 0) && !engine.process(channels, 2), "invalid prepare unprepares");
}
void snapshotsAndSmoothing() {
    darkvoice::AudioEngine engine, reference;
    check(engine.prepare(48000, 48000, 1) && reference.prepare(48000, 48000, 1), "snapshot setup");
    darkvoice::EngineParameters p;
    p.mix = 0;
    p.outputDb = 12;
    check(engine.setParameters(p) && reference.setParameters(p), "valid parameter snapshot");
    const std::array<float darkvoice::EngineParameters::*, 7> fields{
        &darkvoice::EngineParameters::inputDb, &darkvoice::EngineParameters::outputDb,
        &darkvoice::EngineParameters::highPassHz, &darkvoice::EngineParameters::drive,
        &darkvoice::EngineParameters::thresholdDb, &darkvoice::EngineParameters::ratio,
        &darkvoice::EngineParameters::mix};
    for (auto field : fields) {
        auto invalid = p;
        invalid.inputDb = 24;
        invalid.outputDb = 0;
        invalid.*field = std::numeric_limits<float>::quiet_NaN();
        check(!engine.setParameters(invalid), "nonfinite snapshot rejected atomically");
    }
    for (auto field : fields) {
        auto invalid = p;
        invalid.*field = 1000;
        check(!engine.setParameters(invalid), "out of range snapshot rejected");
    }
    std::vector<float> first(48000, 0.1f), second = first;
    check(run(engine, first, 71) && run(reference, second, 48000), "variable block processing");
    check(first == second, "partition equivalence and rejected snapshots leave targets unchanged");
    check(first[0] < 0.101f && first.back() > 0.397f && first.back() < 0.399f, "gain smoothly approaches requested dB");
    check(first[1] > first[0] && std::abs(first[1] - first[0]) < 0.001f, "smooth gain sample increments");
    engine.reset();
    std::vector<float> settled(8, 0.1f);
    check(run(engine, settled, 8) && settled[0] > 0.397f, "reset settles parameters to targets");
    check(engine.prepare(48000, 8, 1), "reprepare defaults");
    p = {};
    p.mix = 0;
    check(engine.setParameters(p), "dry setup");
    engine.reset();
    std::vector<float> dry(8, 0.1f);
    check(run(engine, dry, 8) && dry[0] == 0.1f, "prepare resets gain defaults");
}
void activePartitionAndBypass() {
    darkvoice::AudioEngine first, second;
    check(first.prepare(48000, 48000, 1) && second.prepare(48000, 48000, 1), "active partition setup");
    darkvoice::EngineParameters p;
    p.inputDb = 6; p.outputDb = -3; p.highPassHz = 300;
    p.drive = 0.4f; p.ratio = 4; p.thresholdDb = -24; p.mix = 0.7f;
    check(first.setParameters(p) && second.setParameters(p), "active smoothed snapshot");
    auto a = sine(48000, 173), b = a;
    check(run(first, a, 48000) && run(second, b, 97), "active partition rendering");
    check(a == b, "active filter envelope and smoothing partition equivalence");
    p.bypass = true;
    check(first.setParameters(p), "history freeze enter bypass");
    auto skipped = sine(48000, 400);
    check(run(first, skipped, 128), "history freeze bypass rendering");
    p.bypass = false;
    check(first.setParameters(p), "history freeze resume");
    a = sine(48000, 251); b = a;
    check(run(first, a, 64) && run(second, b, 512) && a == b, "bypass freezes nonzero filter envelope and smoothing history");
}
void signalTests(double rate) {
    darkvoice::AudioEngine highpass;
    darkvoice::EngineParameters p;
    p.highPassHz = 400;
    auto low = sine(rate, 40), high = sine(rate, 4000);
    check(highpass.prepare(rate, 256, 1) && highpass.setParameters(p), "supported sample rate");
    highpass.reset();
    check(run(highpass, low, 256), "low sine rendering");
    highpass.reset();
    check(run(highpass, high, 73), "high sine rendering");
    check(rms(low) < 0.065 && rms(high) > 0.48, "high pass attenuates low frequency and preserves highs");
    auto clean = sine(rate, 1000), saturated = clean, compressed = clean;
    p = {};
    check(highpass.setParameters(p), "clean setup");
    highpass.reset();
    check(run(highpass, clean, 256), "clean rendering");
    p.drive = 0.6f;
    check(highpass.setParameters(p), "drive setup");
    highpass.reset();
    check(run(highpass, saturated, 256), "saturation rendering");
    double difference = 0;
    for (std::size_t i = 0; i < clean.size(); ++i) difference += std::abs(clean[i] - saturated[i]);
    check(difference / clean.size() > 0.1, "saturation materially changes signal");
    p.drive = 0;
    p.thresholdDb = -30;
    p.ratio = 10;
    check(highpass.setParameters(p), "compression setup");
    highpass.reset();
    check(run(highpass, compressed, 127), "compression rendering");
    check(rms(compressed) < rms(clean) * 0.15, "compressor reduces above-threshold signal");
    std::vector<float> silence(10000, 0);
    highpass.reset();
    check(run(highpass, silence, 101) && std::all_of(silence.begin(), silence.end(), [](float x) { return x == 0; }), "finite exact silence");
}
void isolationBypassAndSafety() {
    darkvoice::AudioEngine stereo;
    check(stereo.prepare(48000, 32, 2), "stereo setup");
    std::array<float, 32> left{}, right{};
    left[0] = 1;
    std::array<float*, 2> channels{left.data(), right.data()};
    check(stereo.process(channels, 32), "stereo impulse processing");
    check(std::all_of(right.begin(), right.end(), [](float v) { return v == 0; }), "stereo channel isolation");
    check(stereo.meters().inputPeak == 1 && stereo.meters().outputPeak > 0, "block peak meters");
    left.fill(0); right.fill(0);
    left[0] = std::numeric_limits<float>::quiet_NaN();
    left[1] = std::numeric_limits<float>::infinity();
    check(stereo.process(channels, 32) && std::isfinite(left[0]) && std::isfinite(left[1]), "active nonfinite input sanitized");
    stereo.reset();
    check(stereo.meters().inputPeak == 0 && stereo.meters().outputPeak == 0, "reset clears meters");
    darkvoice::EngineParameters p;
    p.bypass = true;
    check(stereo.setParameters(p), "bypass setup");
    left.fill(0.25f); right.fill(-0.5f);
    left[0] = std::numeric_limits<float>::infinity();
    check(stereo.process(channels, 32) && std::isinf(left[0]) && left[1] == 0.25f && right[0] == -0.5f, "bypass preserves original samples");
    check(stereo.meters().inputPeak == 0.5f && stereo.meters().outputPeak == 0.5f, "bypass ignores nonfinite meter input");
    p.bypass = false;
    check(stereo.setParameters(p), "exit bypass");
    left.fill(0); right.fill(0);
    check(stereo.process(channels, 32) && left[0] == 0 && right[0] == 0, "bypass freezes filter history");
    const auto before = allocations.load();
    const bool prepared = stereo.prepare(48000, 32, 2);
    const bool parameters = stereo.setParameters(p);
    stereo.reset();
    const bool processed = stereo.process(channels, 32);
    const auto after = allocations.load();
    check(prepared && parameters && processed && after == before, "no dynamic allocations during lifecycle or audio processing");
}
}

void* operator new(std::size_t bytes) {
    allocations.fetch_add(1);
    if (void* result = std::malloc(bytes == 0 ? 1 : bytes)) return result;
    throw std::bad_alloc();
}
void* operator new[](std::size_t bytes) { return ::operator new(bytes); }
[[gnu::noinline]] void operator delete(void* pointer) noexcept { std::free(pointer); }
[[gnu::noinline]] void operator delete[](void* pointer) noexcept { std::free(pointer); }
[[gnu::noinline]] void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
[[gnu::noinline]] void operator delete[](void* pointer, std::size_t) noexcept { std::free(pointer); }

int main() {
    lifecycle();
    snapshotsAndSmoothing();
    activePartitionAndBypass();
    for (double rate : {44100.0, 48000.0, 88200.0, 96000.0}) signalTests(rate);
    isolationBypassAndSafety();
    std::printf("Engine checks: %d, failures: %d\n", checks, failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
