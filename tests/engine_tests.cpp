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
std::atomic<bool> rejectAllocations{false};
int failures = 0;
int checks = 0;
void check(bool condition, const char *message) {
    ++checks;
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}
bool run(darkvoice::AudioEngine &engine, std::vector<float> &data, std::size_t partition) {
    for (std::size_t offset = 0; offset < data.size(); offset += partition) {
        std::array<float *, 1> channel{data.data() + offset};
        if (!engine.process(channel, std::min(partition, data.size() - offset)))
            return false;
    }
    return true;
}
std::vector<float> sine(double rate, double frequency, double amplitude = 0.8) {
    std::vector<float> result(static_cast<std::size_t>(rate));
    for (std::size_t i = 0; i < result.size(); ++i)
        result[i] =
            static_cast<float>(amplitude * std::sin(2 * std::numbers::pi * frequency * i / rate));
    return result;
}
double rms(const std::vector<float> &data) {
    double sum = 0;
    for (std::size_t i = data.size() / 2; i < data.size(); ++i)
        sum += data[i] * data[i];
    return std::sqrt(sum / (data.size() - data.size() / 2));
}
void lifecycle() {
    darkvoice::AudioEngine engine;
    std::array<float, 2> left{0.5f, -0.5f}, right{0.25f, -0.25f};
    std::array<float *, 2> channels{left.data(), right.data()};
    check(!engine.process(channels, 2), "unprepared rejection");
    for (double rate : {0.0, -1.0, 22050.0, std::numeric_limits<double>::infinity()})
        check(!engine.prepare(rate, 2, 2), "invalid rate rejection");
    check(!engine.prepare(48000, 0, 2), "zero maximum rejection");
    check(!engine.prepare(48000, 65537, 2), "bounded preparation rejects excessive capacity");
    check(!engine.prepare(48000, 2, 3), "unsupported channel rejection");
    check(engine.prepare(48000, 2, 2), "valid stereo preparation");
    std::array<float *, 2> nullLast{left.data(), nullptr};
    check(!engine.process(nullLast, 2) && left[0] == 0.5f, "all pointers validated before writes");
    std::array<float *, 2> aliases{left.data(), left.data()};
    check(!engine.process(aliases, 2) && left[0] == 0.5f, "aliased pointers rejected");
    check(!engine.process(channels, 3) && right[0] == 0.25f, "oversized block untouched");
    check(!engine.process(std::span<float *>(channels.data(), 1), 2), "wrong channel count");
    check(engine.process(nullLast, 0), "empty block permits null pointer");
    check(engine.latencySamples() == 0, "starter chain zero latency");
    check(!engine.prepare(48000, 2, 0) && !engine.process(channels, 2),
          "invalid prepare unprepares");
}
void snapshotsAndSmoothing() {
    darkvoice::AudioEngine engine, reference;
    check(engine.prepare(48000, 48000, 1) && reference.prepare(48000, 48000, 1), "snapshot setup");
    darkvoice::EngineParameters p;
    p.mix = 0;
    p.outputDb = 12;
    check(engine.setParameters(p) && reference.setParameters(p), "valid parameter snapshot");
    const std::array<float darkvoice::EngineParameters::*, 26> fields{
        &darkvoice::EngineParameters::inputDb,
        &darkvoice::EngineParameters::outputDb,
        &darkvoice::EngineParameters::highPassHz,
        &darkvoice::EngineParameters::drive,
        &darkvoice::EngineParameters::thresholdDb,
        &darkvoice::EngineParameters::ratio,
        &darkvoice::EngineParameters::mix,
        &darkvoice::EngineParameters::eqLowHz,
        &darkvoice::EngineParameters::eqLowGainDb,
        &darkvoice::EngineParameters::eqMidHz,
        &darkvoice::EngineParameters::eqMidGainDb,
        &darkvoice::EngineParameters::eqMidQ,
        &darkvoice::EngineParameters::eqHighHz,
        &darkvoice::EngineParameters::eqHighGainDb,
        &darkvoice::EngineParameters::delayMs,
        &darkvoice::EngineParameters::delayFeedback,
        &darkvoice::EngineParameters::delayMix,
        &darkvoice::EngineParameters::reverbSize,
        &darkvoice::EngineParameters::reverbDamping,
        &darkvoice::EngineParameters::reverbMix,
        &darkvoice::EngineParameters::gateThresholdDb,
        &darkvoice::EngineParameters::gateRatio,
        &darkvoice::EngineParameters::gateReleaseMs,
        &darkvoice::EngineParameters::deEssHz,
        &darkvoice::EngineParameters::deEssThresholdDb,
        &darkvoice::EngineParameters::deEssReductionDb};
    for (auto field : fields) {
        auto invalid = p;
        invalid.inputDb = 24;
        invalid.outputDb = 0;
        invalid.*field = std::numeric_limits<float>::quiet_NaN();
        check(!engine.setParameters(invalid), "nonfinite snapshot rejected atomically");
    }
    for (auto field : fields) {
        auto invalid = p;
        invalid.*field = std::numeric_limits<float>::max();
        check(!engine.setParameters(invalid), "out of range snapshot rejected");
    }
    std::vector<float> first(48000, 0.1f), second = first;
    check(run(engine, first, 71) && run(reference, second, 48000), "variable block processing");
    check(first == second, "partition equivalence and rejected snapshots leave targets unchanged");
    check(first[0] < 0.101f && first.back() > 0.397f && first.back() < 0.399f,
          "gain smoothly approaches requested dB");
    check(first[1] > first[0] && std::abs(first[1] - first[0]) < 0.001f,
          "smooth gain sample increments");
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
    check(first.prepare(48000, 48000, 1) && second.prepare(48000, 48000, 1),
          "active partition setup");
    darkvoice::EngineParameters p;
    p.inputDb = 6;
    p.outputDb = -3;
    p.highPassHz = 300;
    p.drive = 0.4f;
    p.ratio = 4;
    p.thresholdDb = -24;
    p.mix = 0.7f;
    p.eqMidGainDb = -6;
    p.eqLowGainDb = 3;
    p.delayMix = 0.25f;
    p.delayFeedback = 0.45f;
    p.reverbMix = 0.15f;
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
    a = sine(48000, 251);
    b = a;
    check(run(first, a, 64) && run(second, b, 512) && a == b,
          "bypass freezes nonzero filter envelope and smoothing history");
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
    check(rms(low) < 0.065 && rms(high) > 0.48,
          "high pass attenuates low frequency and preserves highs");
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
    for (std::size_t i = 0; i < clean.size(); ++i)
        difference += std::abs(clean[i] - saturated[i]);
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
    check(run(highpass, silence, 101) &&
              std::all_of(silence.begin(), silence.end(), [](float x) { return x == 0; }),
          "finite exact silence");
}
void integratedGate(double rate) {
    darkvoice::AudioEngine gated, dry;
    check(gated.prepare(rate, 1024, 1) && dry.prepare(rate, 1024, 1), "gate engine prepare");
    darkvoice::EngineParameters p;
    p.gateThresholdDb = -20;
    p.gateRatio = 4;
    p.gateReleaseMs = 10;
    check(gated.setParameters(p), "gate engine controls");
    gated.reset();
    auto input = sine(rate, 1000, .005f);
    auto reference = input;
    check(run(gated, input, 257) && run(dry, reference, 257), "gate engine render");
    double wetEnergy = 0, dryEnergy = 0;
    for (std::size_t i = input.size() / 2; i < input.size(); ++i) {
        wetEnergy += static_cast<double>(input[i]) * input[i];
        dryEnergy += static_cast<double>(reference[i]) * reference[i];
    }
    check(wetEnergy < dryEnergy * .001, "gate controls reduce actual engine signal");
}

void integratedDeEsser(double rate) {
    darkvoice::AudioEngine fx, reference;
    check(fx.prepare(rate, 1024, 1) && reference.prepare(rate, 1024, 1), "de-esser engine prepare");
    darkvoice::EngineParameters p;
    p.deEssHz = 2000;
    p.deEssThresholdDb = -60;
    p.deEssReductionDb = 12;
    check(fx.setParameters(p), "de-esser engine controls");
    fx.reset();
    auto input = sine(rate, 8000, .1), original = input;
    check(run(fx, input, 257) && run(reference, original, 257), "de-esser engine render");
    check(rms(input) < rms(original) * .65, "de-esser changes actual engine audio");
}

void isolationBypassAndSafety() {
    darkvoice::AudioEngine stereo;
    check(stereo.prepare(48000, 32, 2), "stereo setup");
    std::array<float, 32> left{}, right{};
    left[0] = 1;
    std::array<float *, 2> channels{left.data(), right.data()};
    check(stereo.process(channels, 32), "stereo impulse processing");
    check(std::all_of(right.begin(), right.end(), [](float v) { return v == 0; }),
          "stereo channel isolation");
    check(stereo.meters().inputPeak == 1 && stereo.meters().outputPeak > 0, "block peak meters");
    left.fill(0);
    right.fill(0);
    left[0] = std::numeric_limits<float>::quiet_NaN();
    left[1] = std::numeric_limits<float>::infinity();
    check(stereo.process(channels, 32) && std::isfinite(left[0]) && std::isfinite(left[1]),
          "active nonfinite input sanitized");
    stereo.reset();
    check(stereo.meters().inputPeak == 0 && stereo.meters().outputPeak == 0, "reset clears meters");
    darkvoice::EngineParameters p;
    p.bypass = true;
    check(stereo.setParameters(p), "bypass setup");
    left.fill(0.25f);
    right.fill(-0.5f);
    left[0] = std::numeric_limits<float>::infinity();
    check(stereo.process(channels, 32) && std::isinf(left[0]) && left[1] == 0.25f &&
              right[0] == -0.5f,
          "bypass preserves original samples");
    check(stereo.meters().inputPeak == 0.5f && stereo.meters().outputPeak == 0.5f,
          "bypass ignores nonfinite meter input");
    p.bypass = false;
    check(stereo.setParameters(p), "exit bypass");
    left.fill(0);
    right.fill(0);
    check(stereo.process(channels, 32) && left[0] == 0 && right[0] == 0,
          "bypass freezes filter history");
    const bool prepared = stereo.prepare(48000, 32, 2);
    p.eqMidGainDb = 3;
    p.delayMix = 0.3f;
    p.reverbMix = 0.3f;
    const auto before = allocations.load();
    const bool parameters = stereo.setParameters(p);
    stereo.reset();
    const bool processed = stereo.process(channels, 32);
    const auto after = allocations.load();
    check(prepared && parameters && processed && after == before,
          "no dynamic allocations during set/reset/audio with active effects");
}

void integratedEffects(double rate) {
    darkvoice::AudioEngine engine;
    check(engine.prepare(rate, 256, 1), "integrated effects prepare at required rate");
    auto baseline = sine(rate, 1000, 0.1), equalized = baseline;
    check(run(engine, baseline, 113), "neutral integrated reference render");
    darkvoice::EngineParameters p;
    p.eqMidHz = 1000;
    p.eqMidGainDb = 9;
    p.eqMidQ = 1;
    check(engine.setParameters(p), "integrated mid-EQ snapshot");
    engine.reset();
    check(run(engine, equalized, 127), "integrated mid-EQ render");
    check(rms(equalized) > rms(baseline) * 2.5 && rms(equalized) < rms(baseline) * 3.0,
          "mid EQ changes actual whole-chain frequency response");

    p = {};
    p.delayMs = 40;
    p.delayFeedback = 0.5f;
    p.delayMix = 0.5f;
    check(engine.setParameters(p), "integrated delay snapshot");
    engine.reset();
    const auto delay = static_cast<std::size_t>(rate * 0.04);
    std::vector<float> impulse(delay * 4, 0);
    impulse[0] = 1;
    check(run(engine, impulse, 71), "integrated feedback impulse render");
    const auto peakAround = [&impulse](std::size_t position) {
        float peak = 0;
        for (auto i = position - 3; i <= position + 3; ++i)
            peak = std::max(peak, std::abs(impulse[i]));
        return peak;
    };
    check(peakAround(delay) > 0.4f && peakAround(delay * 2) > 0.15f,
          "delay time and feedback repeats survive whole-chain processing");

    engine.reset();
    std::array<float, 256> block{};
    std::array<float *, 1> channel{block.data()};
    block[0] = 1;
    check(engine.process(channel, block.size()), "tail seed render");
    bool meteredTail = false;
    for (std::size_t processed = block.size(); processed < delay * 3; processed += block.size()) {
        block.fill(0);
        check(engine.process(channel, block.size()), "tail zero-input render");
        meteredTail |= engine.meters().inputPeak == 0 && engine.meters().outputPeak > 0.05f;
    }
    check(meteredTail, "output meters include delay tail without dry input");

    p.eqMidGainDb = 8;
    p.eqLowGainDb = -6;
    p.drive = 0.6f;
    p.ratio = 8;
    p.thresholdDb = -30;
    p.reverbMix = 0.4f;
    p.mix = 0;
    check(engine.setParameters(p), "fully active effects with original-dry mix");
    engine.reset();
    auto dry = sine(rate, 230, 0.2), original = dry;
    check(run(engine, dry, 131) && dry == original,
          "global zero wet mix preserves original dry despite all active effects");
    p.mix = 1;
    check(engine.setParameters(p), "integrated reverb snapshot");
    engine.reset();
    std::vector<float> reverb(static_cast<std::size_t>(rate * 0.4), 0);
    reverb[0] = 0.5f;
    check(run(engine, reverb, 193), "integrated reverb impulse render");
    double energy = 0;
    for (std::size_t i = static_cast<std::size_t>(rate * 0.07); i < reverb.size(); ++i)
        energy += static_cast<double>(reverb[i]) * reverb[i];
    check(energy > 1.0e-7 &&
              std::all_of(reverb.begin(), reverb.end(), [](float v) { return std::isfinite(v); }),
          "spatial whole-chain tail is real and finite");
    engine.reset();
    std::fill(reverb.begin(), reverb.end(), 0);
    check(run(engine, reverb, 251) &&
              std::all_of(reverb.begin(), reverb.end(), [](float v) { return v == 0; }),
          "reset clears all filter and spatial histories");
}

void invalidBlocksAndAllocationFailure() {
    darkvoice::AudioEngine first, reference;
    check(first.prepare(48000, 256, 1) && reference.prepare(48000, 256, 1),
          "invalid-history comparison setup");
    darkvoice::EngineParameters p;
    p.eqMidGainDb = 5;
    p.delayMix = 0.4f;
    p.reverbMix = 0.3f;
    check(first.setParameters(p) && reference.setParameters(p), "invalid-history active effects");
    std::array<float, 256> a{}, b{};
    a[0] = b[0] = 1;
    std::array<float *, 1> ca{a.data()}, cb{b.data()};
    check(first.process(ca, a.size()) && reference.process(cb, b.size()),
          "invalid-history seed render");
    const auto preserved = a;
    std::array<float *, 1> invalid{nullptr};
    check(!first.process(ca, 257) && !first.process(invalid, 8) && a == preserved,
          "rejected blocks do not alter caller samples");
    check(first.process(ca, 0), "zero frame call valid with spatial history");
    a.fill(0);
    b.fill(0);
    check(first.process(ca, a.size()) && reference.process(cb, b.size()) && a == b,
          "rejected/empty blocks do not advance spatial or smoothing history");

    darkvoice::AudioEngine allocationFailure;
    rejectAllocations.store(true);
    const bool failed = allocationFailure.prepare(48000, 256, 2);
    rejectAllocations.store(false);
    check(!failed && !allocationFailure.process(ca, 1),
          "allocation failure returns false and leaves engine unprepared");
    check(allocationFailure.prepare(48000, 256, 1) && allocationFailure.process(ca, 1),
          "preparation recovers after allocation failure");
    // Scratch capacity is now retained; the next forced failure exercises the
    // dependent spatial module's allocation path rather than scratch growth.
    rejectAllocations.store(true);
    const bool dependencyFailed = allocationFailure.prepare(48000, 256, 1);
    rejectAllocations.store(false);
    check(!dependencyFailed && !allocationFailure.process(ca, 1),
          "effect preparation failure also leaves engine unprepared");
    check(allocationFailure.prepare(48000, 256, 1) && allocationFailure.process(ca, 1),
          "preparation recovers after dependent module allocation failure");
}
} // namespace

// Prevent allocator replacement inlining from confusing GCC's allocation pairing
// analysis at -O3; keep the compiler's mismatch diagnostics enabled.
#if defined(__GNUC__) || defined(__clang__)
#define DARKVOICE_TEST_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#define DARKVOICE_TEST_NOINLINE __declspec(noinline)
#else
#define DARKVOICE_TEST_NOINLINE
#endif

DARKVOICE_TEST_NOINLINE void *operator new(std::size_t bytes) {
    allocations.fetch_add(1);
    if (rejectAllocations.load())
        throw std::bad_alloc();
    if (void *result = std::malloc(bytes == 0 ? 1 : bytes))
        return result;
    throw std::bad_alloc();
}
DARKVOICE_TEST_NOINLINE void *operator new[](std::size_t bytes) {
    return ::operator new(bytes);
}
DARKVOICE_TEST_NOINLINE void operator delete(void *pointer) noexcept {
    std::free(pointer);
}
DARKVOICE_TEST_NOINLINE void operator delete[](void *pointer) noexcept {
    std::free(pointer);
}
DARKVOICE_TEST_NOINLINE void operator delete(void *pointer, std::size_t) noexcept {
    std::free(pointer);
}
DARKVOICE_TEST_NOINLINE void operator delete[](void *pointer, std::size_t) noexcept {
    std::free(pointer);
}

#undef DARKVOICE_TEST_NOINLINE

int main() {
    lifecycle();
    snapshotsAndSmoothing();
    activePartitionAndBypass();
    for (double rate : {44100.0, 48000.0, 88200.0, 96000.0}) {
        signalTests(rate);
        integratedEffects(rate);
        integratedGate(rate);
        integratedDeEsser(rate);
    }
    isolationBypassAndSafety();
    invalidBlocksAndAllocationFailure();
    std::printf("Engine checks: %d, failures: %d\n", checks, failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
