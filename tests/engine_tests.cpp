#include <darkvoice/AudioEngine.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
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
// Only use when all final controls are static and settled. Streaming run() above
// intentionally neither flushes nor crops history or automation.
bool runStaticAligned(darkvoice::AudioEngine &engine, std::vector<float> &data,
                      std::size_t partition) {
    const auto frames = data.size();
    data.resize(frames + 56, 0);
    if (!run(engine, data, partition))
        return false;
    data.erase(data.begin(), data.begin() + 56);
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
    check(engine.latencySamples() == 56, "prepared chain fixed alignment latency");
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
    const std::array<float darkvoice::EngineParameters::*, 51> fields{
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
        &darkvoice::EngineParameters::deEssReductionDb,
        &darkvoice::EngineParameters::chorusRateHz,
        &darkvoice::EngineParameters::chorusDepthMs,
        &darkvoice::EngineParameters::chorusMix,
        &darkvoice::EngineParameters::stereoWidth,
        &darkvoice::EngineParameters::flangerRateHz,
        &darkvoice::EngineParameters::flangerDepthMs,
        &darkvoice::EngineParameters::flangerFeedback,
        &darkvoice::EngineParameters::flangerMix,
        &darkvoice::EngineParameters::limiterCeilingDb,
        &darkvoice::EngineParameters::limiterReleaseMs,
        &darkvoice::EngineParameters::stereoDelayMs,
        &darkvoice::EngineParameters::stereoDelayFeedback,
        &darkvoice::EngineParameters::stereoDelayMix,
        &darkvoice::EngineParameters::stereoDelayBpm,
        &darkvoice::EngineParameters::doublerSeparationMs,
        &darkvoice::EngineParameters::doublerVariationMs,
        &darkvoice::EngineParameters::doublerRateHz,
        &darkvoice::EngineParameters::doublerMix,
        &darkvoice::EngineParameters::spaceDecay,
        &darkvoice::EngineParameters::spaceToneHz,
        &darkvoice::EngineParameters::spacePreDelayMs,
        &darkvoice::EngineParameters::spaceMix,
        &darkvoice::EngineParameters::colourDrive,
        &darkvoice::EngineParameters::colourToneHz,
        &darkvoice::EngineParameters::colourMix};
    for (int value : {-1, 3}) {
        auto invalid = p;
        invalid.stereoDelayMode = value;
        check(!engine.setParameters(invalid), "Invalid stereo delay mode rejected atomically");
    }
    for (int value : {-1, 6}) {
        auto invalid = p;
        invalid.stereoDelayDivision = value;
        check(!engine.setParameters(invalid), "Invalid stereo delay division rejected atomically");
    }
    for (int invalid : {-1, 3}) {
        auto candidate = p;
        candidate.spaceModel = invalid;
        check(!engine.setParameters(candidate),
              "Invalid Space model engine snapshot rejected atomically");
    }
    for (int value : {-1, 2}) {
        auto invalid = p;
        invalid.colourModel = value;
        check(!engine.setParameters(invalid), "Invalid Colour model rejected atomically");
    }
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
    check(first[57] > first[56] && std::abs(first[57] - first[56]) < 0.001f,
          "smooth gain sample increments");
    engine.reset();
    std::vector<float> settled(8, 0.1f);
    check(runStaticAligned(engine, settled, 8) && settled[0] > 0.397f,
          "reset settles parameters to targets");
    check(engine.prepare(48000, 8, 1), "reprepare defaults");
    p = {};
    p.mix = 0;
    check(engine.setParameters(p), "dry setup");
    engine.reset();
    std::vector<float> dry(8, 0.1f);
    check(runStaticAligned(engine, dry, 8) && dry[0] == 0.1f, "prepare resets gain defaults");
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
    check(run(first, a, 64) && run(second, b, 512) &&
              std::equal(a.begin() + 184, a.end(), b.begin() + 184),
          "after explicit resume transition legacy filter envelope and smoothing history stay "
          "frozen");
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

void integratedModulation(double rate) {
    darkvoice::AudioEngine stereo;
    check(stereo.prepare(rate, 1024, 2), "modulation stereo engine prepare");
    darkvoice::EngineParameters p;
    p.stereoWidth = 0;
    check(stereo.setParameters(p), "width engine controls");
    stereo.reset();
    std::array<float, 512> left{}, right{};
    for (std::size_t i = 0; i < left.size(); ++i) {
        left[i] = static_cast<float>(.1 * std::sin(i * .17));
        right[i] = -.5f * left[i];
    }
    std::array<float *, 2> ptr{left.data(), right.data()};
    check(stereo.process(ptr, 512) && left == right,
          "width controls fold actual wet engine to mono");
    p.mix = 0;
    check(stereo.setParameters(p), "global dry setup");
    stereo.reset();
    left.fill(.1f);
    right.fill(-.2f);
    check(stereo.process(ptr, 512) && left[100] == .1f && right[100] == -.2f,
          "global dry preserves original stereo before width");
    darkvoice::AudioEngine chorus;
    check(chorus.prepare(rate, 1024, 1), "chorus engine prepare");
    p = {};
    p.chorusMix = .5f;
    p.chorusDepthMs = 0;
    check(chorus.setParameters(p), "chorus engine controls");
    chorus.reset();
    std::vector<float> impulse(static_cast<std::size_t>(rate * .04));
    impulse[0] = 1;
    check(runStaticAligned(chorus, impulse, 257), "chorus engine render");
    const auto delayed = static_cast<std::size_t>(rate * .015);
    check(impulse[delayed] > .2f, "chorus controls generate actual delayed engine signal");
}

void isolationBypassAndSafety() {
    darkvoice::AudioEngine stereo;
    check(stereo.prepare(48000, 128, 2), "stereo setup");
    std::array<float, 128> left{}, right{};
    left[0] = 1;
    std::array<float *, 2> channels{left.data(), right.data()};
    check(stereo.process(channels, 128), "stereo impulse processing");
    check(std::all_of(right.begin(), right.end(), [](float v) { return v == 0; }),
          "stereo channel isolation");
    check(stereo.meters().inputPeak == 1 && stereo.meters().outputPeak > 0, "block peak meters");
    left.fill(0);
    right.fill(0);
    left[0] = std::numeric_limits<float>::quiet_NaN();
    left[1] = std::numeric_limits<float>::infinity();
    check(stereo.process(channels, 128) && std::isfinite(left[0]) && std::isfinite(left[1]),
          "active nonfinite input sanitized");
    stereo.reset();
    check(stereo.meters().inputPeak == 0 && stereo.meters().outputPeak == 0, "reset clears meters");
    darkvoice::EngineParameters p;
    p.bypass = true;
    check(stereo.setParameters(p), "bypass setup");
    left.fill(0.25f);
    right.fill(-0.5f);
    left[0] = std::numeric_limits<float>::infinity();
    check(stereo.process(channels, 128) && std::isinf(left[56]) && left[57] == 0.25f &&
              right[56] == -0.5f,
          "bypass preserves original samples");
    check(stereo.meters().inputPeak == 0.5f && stereo.meters().outputPeak == 0.5f,
          "bypass ignores nonfinite meter input");
    p.bypass = false;
    check(stereo.setParameters(p), "exit bypass");
    left.fill(0);
    right.fill(0);
    check(stereo.process(channels, 128) && left[56] == 0 && right[56] == 0,
          "bypass freezes filter history");
    const bool prepared = stereo.prepare(48000, 128, 2);
    p.eqMidGainDb = 3;
    p.delayMix = 0.3f;
    p.reverbMix = 0.3f;
    const auto before = allocations.load();
    const bool parameters = stereo.setParameters(p);
    stereo.reset();
    const bool processed = stereo.process(channels, 128);
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
    check(runStaticAligned(engine, impulse, 71), "integrated feedback impulse render");
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
    check(runStaticAligned(engine, dry, 131) && dry == original,
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

void integratedFlanger(double rate) {
    darkvoice::AudioEngine engine;
    check(engine.prepare(rate, 512, 1), "integrated flanger prepare");
    darkvoice::EngineParameters p;
    p.flangerMix = 1;
    p.flangerDepthMs = 0;
    p.flangerFeedback = .5f;
    check(engine.setParameters(p), "integrated flanger snapshot");
    engine.reset();
    std::vector<float> x(static_cast<std::size_t>(rate * .03));
    x[0] = .5f;
    check(runStaticAligned(engine, x, 131), "integrated flanger render");
    const auto delay = static_cast<std::size_t>(rate * .0045);
    check(std::all_of(x.begin(), x.begin() + delay - 1, [](float v) { return v == 0; }) &&
              std::max(std::abs(x[delay]), std::abs(x[delay + 1])) > .2f,
          "engine routes actual delayed flanger signal");
    p.mix = 0;
    check(engine.setParameters(p), "integrated flanger dry snapshot");
    engine.reset();
    x = sine(rate, 220, .1);
    const auto original = x;
    check(runStaticAligned(engine, x, 193) && x == original,
          "global dry mix preserves unprocessed flanger input");
}

void integratedSpaceReverb(double rate) {
    darkvoice::AudioEngine actual, reference, partition;
    check(actual.prepare(rate, 257, 1) && reference.prepare(rate, 257, 1) &&
              partition.prepare(rate, 257, 1),
          "Integrated Space Reverb prepare");
    darkvoice::EngineParameters p;
    p.spaceMix = .5f;
    check(actual.setParameters(p) && partition.setParameters(p), "Integrated Space snapshot");
    actual.reset();
    partition.reset();
    std::vector<float> wet(static_cast<std::size_t>(rate * .08), 0), dry = wet, other = wet;
    wet[0] = dry[0] = other[0] = .8f;
    check(runStaticAligned(actual, wet, 137) && runStaticAligned(reference, dry, 257) &&
              runStaticAligned(partition, other, 257) && wet == other,
          "Space engine partition invariant");
    const auto first = static_cast<std::size_t>(std::llround(rate * .003));
    for (std::size_t i = 0; i < first; ++i)
        check(std::abs(2 * wet[i] - dry[i]) < 1e-7,
              "Zero predelay Space must wait only for Room network arrival");
    const double expected = .18 * (1 - std::exp(-2 * std::numbers::pi * 8000 / rate)) * dry[0];
    check(std::abs(2 * wet[first] - dry[first] - expected) < 2e-7,
          "Integrated Room first arrival matches independent coefficient");
    p.mix = 0;
    check(actual.setParameters(p), "Space global dry snapshot");
    actual.reset();
    std::array<float, 257> data{};
    data[0] = .123f;
    std::array<float *, 1> channels{data.data()};
    check(actual.process(channels, 257) && data[56] == .123f,
          "Global dry excludes Space wet output");
    p.mix = 1;
    p.bypass = true;
    check(actual.setParameters(p), "Space bypass snapshot");
    data.fill(.456f);
    check(actual.process(channels, 257) && data[56] == .456f,
          "Global bypass remains exact with Space active");
    p.bypass = false;
    p.spaceModel = 2;
    p.spacePreDelayMs = 2.5f;
    check(actual.setParameters(p), "Space model/predelay snapshot");
    actual.reset();
    const auto before = allocations.load();
    rejectAllocations.store(true);
    const bool callback = actual.setParameters(p) && actual.process(channels, 137);
    actual.reset();
    rejectAllocations.store(false);
    check(callback && allocations.load() == before,
          "Space integrated callback/reset allocate nothing");
}

void integratedDoubler(double rate) {
    darkvoice::AudioEngine actual, reference, partition;
    check(actual.prepare(rate, 257, 1) && reference.prepare(rate, 257, 1) &&
              partition.prepare(rate, 257, 1),
          "Integrated doubler prepare");
    darkvoice::EngineParameters p;
    p.doublerMix = .5f;
    p.doublerVariationMs = 0;
    check(actual.setParameters(p) && partition.setParameters(p), "Integrated doubler snapshot");
    actual.reset();
    partition.reset();
    std::vector<float> wet(static_cast<std::size_t>(rate * .08), 0), dry = wet, other = wet;
    wet[0] = dry[0] = other[0] = .8f;
    check(runStaticAligned(actual, wet, 137) && runStaticAligned(reference, dry, 257) &&
              runStaticAligned(partition, other, 257) && wet == other,
          "Doubler integrated partition equivalence");
    const auto tap = [&](std::size_t i, double seconds) {
        const double delay = rate * seconds;
        const auto integral = static_cast<std::size_t>(std::floor(delay));
        const double fraction = delay - integral;
        return (i >= integral ? (1 - fraction) * dry[i - integral] : 0) +
               (i > integral ? fraction * dry[i - integral - 1] : 0);
    };
    double maxError = 0;
    for (std::size_t i = 0; i < wet.size(); ++i)
        maxError = std::max(
            maxError, std::abs(wet[i] - (.5 * dry[i] + .25 * tap(i, .020) + .25 * tap(i, .031))));
    check(maxError < 2e-7, "Integrated mono doubler matches independent two-tap dry/wet reference");
    p.doublerVariationMs = 5;
    p.doublerRateHz = 1.5f;
    check(actual.setParameters(p), "Doubler varying snapshot");
    actual.reset();
    p.mix = 0;
    check(actual.setParameters(p), "Doubler global dry snapshot");
    actual.reset();
    std::array<float, 257> sample{};
    sample[0] = .123f;
    std::array<float *, 1> channels{sample.data()};
    check(actual.process(channels, 257) && sample[56] == .123f,
          "Global dry ignores simulated doubler");
    p.mix = 1;
    p.bypass = true;
    check(actual.setParameters(p), "Doubler bypass snapshot");
    sample.fill(.456f);
    check(actual.process(channels, 257) && sample[56] == .456f,
          "Global bypass exact with simulated doubler");
    p.bypass = false;
    check(actual.setParameters(p), "Doubler callback snapshot");
    actual.reset();
    const auto before = allocations.load();
    rejectAllocations.store(true);
    const bool callback = actual.setParameters(p) && actual.process(channels, 137);
    actual.reset();
    rejectAllocations.store(false);
    check(callback && allocations.load() == before,
          "Doubler integrated callback/reset allocate nothing");
}

void integratedStereoDelay(double rate) {
    darkvoice::AudioEngine a, b;
    check(a.prepare(rate, 257, 1) && b.prepare(rate, 257, 1), "Musical delay engine prepare");
    darkvoice::EngineParameters p;
    p.stereoDelayMix = .5f;
    p.stereoDelayFeedback = 0;
    p.stereoDelayMs = 100;
    check(a.setParameters(p) && b.setParameters(p), "Musical delay engine parameters");
    a.reset();
    b.reset();
    std::vector<float> first(static_cast<std::size_t>(rate * .3), 0), second = first;
    first[0] = second[0] = .8f;
    check(runStaticAligned(a, first, 137) && runStaticAligned(b, second, 257) && first == second,
          "Musical delay engine partition equivalence");
    const auto delay = static_cast<std::size_t>(std::round(rate * .1));
    auto at = delay;
    for (std::size_t i = delay - 1; i <= delay + 1; ++i)
        if (std::abs(first[i]) > std::abs(first[at]))
            at = i;
    check(std::abs(first[at]) > .3f, "Musical delay routed after existing wet chain");
    check(std::abs(first[delay * 2]) < .001f, "Musical delay zero feedback gives single echo");
    p.mix = 0;
    check(a.setParameters(p), "Musical delay global dry snapshot");
    a.reset();
    std::array<float, 257> sample{};
    sample[0] = .8f;
    std::array<float *, 1> channel{sample.data()};
    check(a.process(channel, 257) && sample[56] == .8f, "Global dry mix excludes musical delay");
    p.mix = 1;
    p.bypass = true;
    check(a.setParameters(p), "Musical delay bypass snapshot");
    sample.fill(.123f);
    check(a.process(channel, 257) && sample[56] == .123f, "Musical delay global bypass exact");
    p.bypass = false;
    check(a.setParameters(p), "Musical delay allocation snapshot");
    a.reset();
    const auto before = allocations.load();
    rejectAllocations.store(true);
    const bool callback = a.setParameters(p) && a.process(channel, 137);
    a.reset();
    rejectAllocations.store(false);
    check(callback && allocations.load() == before,
          "Musical delay integrated callbacks/reset allocate nothing");
}

void integratedLimiter(double rate) {
    for (std::size_t count : {1U, 2U}) {
        darkvoice::AudioEngine engine;
        check(engine.prepare(rate, 257, count), "limiter engine preparation");
        darkvoice::EngineParameters p;
        check(!p.limiterEnabled, "limiter default remains disabled");
        p.mix = 0;
        p.outputDb = 12;
        check(engine.setParameters(p), "limiter disabled snapshot");
        engine.reset();
        std::array<float, 257> left{}, right{};
        left.fill(.8f);
        right.fill(.4f);
        std::array<float *, 2> pointers{left.data(), right.data()};
        auto channels = std::span<float *>(pointers.data(), count);
        check(engine.process(channels, 257) && left[56] > 3.f,
              "disabled limiter preserves amplified dry output");
        p.limiterEnabled = true;
        p.limiterCeilingDb = -6;
        p.limiterReleaseMs = 1000;
        check(engine.setParameters(p), "limiter active snapshot");
        engine.reset();
        left.fill(.8f);
        right.fill(.4f);
        const double ceiling = std::pow(10., -6. / 20.);
        check(engine.process(channels, 257), "limiter final dry-output render");
        for (std::size_t i = 0; i < left.size(); ++i) {
            check(std::abs(left[i]) <= ceiling,
                  "final gain and dry mix cannot exceed limiter ceiling");
            if (count == 2)
                check(std::abs(right[i] - .5f * left[i]) < 1e-7f, "limiter stereo gain linked");
        }
        check(engine.meters().outputPeak <= ceiling && engine.meters().outputPeak > .49f,
              "output meters report post-limiter signal");
        p.limiterCeilingDb = -18;
        check(engine.setParameters(p), "ceiling update snapshot");
        left.fill(.8f);
        right.fill(.4f);
        check(engine.process(channels, 1) && left[0] <= std::pow(10., -18. / 20.),
              "ceiling reduction protects next sample");
        auto invalid = p;
        invalid.limiterCeilingDb = -25;
        invalid.limiterEnabled = false;
        check(!engine.setParameters(invalid), "invalid limiter snapshot rejected atomically");
        left.fill(.8f);
        right.fill(.4f);
        check(engine.process(channels, 1) && left[0] <= std::pow(10., -18. / 20.),
              "invalid snapshot cannot disable final limiter");
        left.fill(.001f);
        right.fill(.0005f);
        check(engine.process(channels, 257), "limiter release render");
        check(left.back() > left[56] && left.back() < .001f * std::pow(10., 12. / 20.),
              "limiter gain recovers continuously during release");
        p.limiterEnabled = false;
        check(engine.setParameters(p), "disable limiter transition");
        p.limiterEnabled = true;
        check(engine.setParameters(p), "reenable limiter transition");
        left.fill(.001f);
        right.fill(.0005f);
        check(engine.process(channels, 1) &&
                  std::abs(left[0] - .001f * std::pow(10., 12. / 20.)) < 1e-8,
              "enable transition clears stale gain reduction");
        p.bypass = true;
        check(engine.setParameters(p), "limiter global bypass snapshot");
        left.fill(2.5f);
        right.fill(-1.5f);
        check(engine.process(channels, 257) && left[56] == 2.5f && right[56] == -1.5f,
              "global bypass bypasses limiter exactly");
        p.bypass = false;
        p.mix = 1;
        check(engine.setParameters(p), "wet final-output limiter snapshot");
        engine.reset();
        left.fill(.8f);
        right.fill(.4f);
        check(engine.process(channels, 257), "wet limiter render");
        for (std::size_t i = 0; i < left.size(); ++i)
            check(std::abs(left[i]) <= std::pow(10., -18. / 20.),
                  "wet chain output also protected");
        const auto before = allocations.load();
        rejectAllocations.store(true);
        const bool callback = engine.setParameters(p) && engine.process(channels, 17);
        engine.reset();
        rejectAllocations.store(false);
        check(callback && allocations.load() == before,
              "enabled limiter callback/reset never allocate");
    }
    darkvoice::AudioEngine a, b;
    check(a.prepare(rate, 1024, 1) && b.prepare(rate, 1024, 1), "limiter partition preparation");
    darkvoice::EngineParameters p;
    p.mix = 0;
    p.limiterEnabled = true;
    p.limiterCeilingDb = -12;
    check(a.setParameters(p) && b.setParameters(p), "limiter partition snapshot");
    auto first = sine(rate, 170, .95), second = first;
    check(run(a, first, 137) && run(b, second, 1024) && first == second,
          "enabled limiter variable buffer partition invariance");
}

// Raw queue is an output-alignment boundary, including host-forced bypass. Keep
// the reference as integer bits so NaN payloads and signed zero cannot be hidden
// by floating point equality or arithmetic.
void rawAlignment(double rate, std::size_t count) {
    darkvoice::AudioEngine engine;
    check(engine.prepare(rate, 257, count) && engine.latencySamples() == 56,
          "aligned engine lifecycle at all rates/layouts");
    constexpr std::array<std::uint32_t, 13> patterns{
        0,           0x80000000U, 1,           0x80000001U, 0x007fffffU, 0x00800000U, 0x7f800000U,
        0xff800000U, 0x7fc12345U, 0xffc23456U, 0x7fa12345U, 0x3e800000U, 0xbf000000U};
    std::array<std::array<std::uint32_t, 56>, 2> queue{};
    std::size_t position = 0, absolute = 0;
    std::array<std::array<float, 257>, 2> audio{};
    std::array<float *, 2> pointers{audio[0].data(), audio[1].data()};
    const auto channels = std::span<float *>(pointers.data(), count);
    darkvoice::EngineParameters p;
    p.colourMix = 1;
    p.limiterEnabled = true;
    p.outputDb = 24;
    for (std::size_t block = 0; block < 14; ++block) {
        const std::size_t frames = std::array<std::size_t, 7>{1, 17, 0, 55, 56, 57, 257}[block % 7];
        p.bypass = block % 2 == 0;
        check(engine.setParameters(p), "raw alignment bypass snapshot");
        std::array<std::array<std::uint32_t, 257>, 2> expected{};
        float inputPeak = 0, outputPeak = 0;
        for (std::size_t i = 0; i < frames; ++i) {
            for (std::size_t c = 0; c < count; ++c) {
                const auto bits = patterns[(absolute + i + c * 3) % patterns.size()];
                audio[c][i] = std::bit_cast<float>(bits);
                expected[c][i] = queue[c][position];
                queue[c][position] = bits;
                const float in = std::bit_cast<float>(bits);
                const float out = std::bit_cast<float>(expected[c][i]);
                if (std::isfinite(in))
                    inputPeak = std::max(inputPeak, std::abs(in));
                if (std::isfinite(out))
                    outputPeak = std::max(outputPeak, std::abs(out));
            }
            position = (position + 1) % 56;
        }
        const auto previousMeters = engine.meters();
        const auto before = allocations.load();
        rejectAllocations.store(true);
        const bool processed =
            p.bypass ? engine.process(channels, frames) : engine.processBypassed(channels, frames);
        rejectAllocations.store(false);
        bool exact = true;
        for (std::size_t c = 0; c < count; ++c)
            for (std::size_t i = 0; i < frames; ++i)
                exact &= std::bit_cast<std::uint32_t>(audio[c][i]) == expected[c][i];
        check(processed && exact && allocations.load() == before,
              "actual and forced bypass retain all raw bits through one queue without allocation");
        check(engine.latencySamples() == 56, "automation/bypass never changes latency");
        check(frames == 0 ? engine.meters().inputPeak == previousMeters.inputPeak &&
                                engine.meters().outputPeak == previousMeters.outputPeak
                          : engine.meters().inputPeak == inputPeak &&
                                engine.meters().outputPeak == outputPeak,
              "input meters current raw, output meters delayed raw, empty meters unchanged");
        absolute += frames;
    }
    engine.release();
    check(engine.latencySamples() == 0 && !engine.processBypassed(channels, 1),
          "release unprepares both process paths and clears reported latency");
    check(engine.prepare(rate, 257, count) && engine.latencySamples() == 56,
          "released engine prepares again");
    check(!engine.prepare(12345, 257, count) && engine.latencySamples() == 0,
          "failed preparation clears reported latency");
}

void neutralColourAlignment(double rate) {
    darkvoice::AudioEngine engine;
    check(engine.prepare(rate, 257, 1), "neutral Colour startup preparation");
    std::array<float, 257> data{}, expected{};
    data[0] = .8f;
    float previousInput = 0, previousOutput = 0;
    const double pole = std::exp(-2 * std::numbers::pi * 20 / rate);
    for (std::size_t i = 0; i + 56 < data.size(); ++i) {
        const float source = i == 0 ? .8f : 0.f;
        const double next = pole * (static_cast<double>(previousOutput) + source - previousInput);
        previousOutput = std::abs(next) < 1e-20 ? 0.f : static_cast<float>(next);
        previousInput = source;
        expected[i + 56] = previousOutput;
    }
    std::array<float *, 1> channel{data.data()};
    check(engine.process(channel, data.size()) && data == expected,
          "neutral Colour is exactly legacy HP impulse delayed56 with zero startup");
    engine.reset();
    data.fill(0);
    check(engine.process(channel, data.size()) &&
              std::all_of(data.begin(), data.end(), [](float x) { return x == 0; }),
          "reset clears neutral Colour/raw alignment histories");
}

void alignedAutomationAndTiny(double rate) {
    darkvoice::AudioEngine engine;
    check(engine.prepare(rate, 257, 1), "output-time automation preparation");
    darkvoice::EngineParameters p;
    p.mix = 0;
    check(engine.setParameters(p), "output-time initial snapshot");
    engine.reset();
    std::array<float, 257> data{};
    std::array<float *, 1> channel{data.data()};
    std::array<float, 56> queue{};
    std::size_t position = 0, absolute = 0;
    float outputDb = 0;
    const double smoothing = 1 - std::exp(-1 / (.010 * rate));
    for (float target : {12.f, -9.f, 24.f}) {
        p.outputDb = target;
        check(engine.setParameters(p), "output-time target change");
        std::array<float, 257> expected{};
        for (std::size_t i = 0; i < data.size(); ++i) {
            data[i] = static_cast<float>(.12 * std::sin((absolute + i) * .077) + .15);
            const float delayed = queue[position];
            queue[position] = data[i];
            position = (position + 1) % 56;
            outputDb = static_cast<float>(outputDb + smoothing * (target - outputDb));
            expected[i] = static_cast<float>(delayed * std::pow(10., outputDb / 20.));
        }
        check(engine.process(channel, data.size()) && data == expected,
              "final gain automation follows output time over independently delayed source");
        absolute += data.size();
    }
    p.outputDb = 24;
    check(engine.setParameters(p), "tiny normal positive final gain snapshot");
    engine.reset();
    data.fill(9e-22f);
    check(engine.process(channel, data.size()) &&
              std::abs(data[56] - 9e-22f * std::pow(10., 24. / 20.)) < 1e-27,
          "normal tiny aligned dry survives pre-gain sanitization and final cleanup threshold");
    data.fill(9e-22f);
    check(engine.processBypassed(channel, data.size()), "tiny guard bypass entry");
    data.fill(9e-22f);
    check(engine.process(channel, data.size()) &&
              std::abs(data[0] - 9e-22f * std::pow(10., 24. / 20.)) < 1e-27,
          "resume guard also preserves tiny normal dry before positive final gain");
}

// Independent routing oracle: explicit HP recurrence and legacy delay state,
// standalone Colour history/reset, raw alignment, output-time control envelopes,
// and the complete initial 56/128 transition, without using an AudioEngine as
// the reference. Non-neutral delay history must survive every bypass interval.
void colourResumeOracle(double rate) {
    darkvoice::AudioEngine engine;
    darkvoice::ToneEffects legacy;
    darkvoice::Colour colour;
    check(engine.prepare(rate, 257, 1) && legacy.prepare(rate, 1, 1) && colour.prepare(rate, 1, 1),
          "Colour resume independent module preparation");
    darkvoice::EngineParameters p;
    p.highPassHz = 60;
    p.delayMs = 20;
    p.delayFeedback = .2f;
    p.delayMix = .3f;
    p.colourModel = 1;
    p.colourDrive = .6f;
    p.colourToneHz = 9000;
    p.colourMix = .7f;
    p.mix = .65f;
    p.outputDb = 3;
    darkvoice::ToneEffectsParameters tone;
    tone.delayMs = p.delayMs;
    tone.delayFeedback = p.delayFeedback;
    tone.delayMix = p.delayMix;
    check(engine.setParameters(p) && legacy.setParameters(tone) &&
              colour.setParameters({p.colourModel, p.colourDrive, p.colourToneHz, p.colourMix}),
          "Colour resume oracle initial parameters");
    engine.reset();
    legacy.reset();
    colour.reset();
    const auto clamp = [](double x) { return std::abs(x) < 1e-20 ? 0.f : static_cast<float>(x); };
    const double pole = std::exp(-2 * std::numbers::pi * p.highPassHz / rate);
    const double smoothing = 1 - std::exp(-1 / (.010 * rate));
    float previousInput = 0, previousOutput = 0, mix = p.mix, outputDb = p.outputDb;
    std::array<float, 56> raw{};
    std::size_t position = 0, guard = 0, fade = 128, absolute = 0;
    bool wasBypassed = false;
    double largestError = 0;
    struct Segment {
        std::size_t frames, partition;
        bool bypass, force;
    };
    // Re-entry during both guard and fade must restart Colour and counters.
    for (const auto segment : std::array<Segment, 9>{{{1200, 257, false, false},
                                                      {73, 17, true, false},
                                                      {31, 1, false, false},
                                                      {13, 13, false, true},
                                                      {94, 23, false, false},
                                                      {19, 19, true, false},
                                                      {320, 137, false, false},
                                                      {61, 61, false, true},
                                                      {320, 97, false, false}}}) {
        p.bypass = segment.bypass;
        p.outputDb = segment.bypass || segment.force ? -6.f : 9.f;
        p.mix = segment.bypass || segment.force ? .2f : .8f;
        check(engine.setParameters(p), "Colour resume segment snapshot");
        for (std::size_t offset = 0; offset < segment.frames; offset += segment.partition) {
            const auto frames = std::min(segment.partition, segment.frames - offset);
            std::array<float, 257> actual{}, expected{};
            for (std::size_t i = 0; i < frames; ++i) {
                const float source = static_cast<float>(.16 * std::sin((absolute + i) * .071) +
                                                        .05 * std::cos((absolute + i) * .193));
                actual[i] = source;
                const float dry = raw[position];
                raw[position] = source;
                position = (position + 1) % 56;
                if (segment.bypass || segment.force) {
                    expected[i] = dry;
                    wasBypassed = true;
                    continue;
                }
                if (wasBypassed) {
                    colour.reset();
                    guard = 56;
                    fade = 0;
                    wasBypassed = false;
                }
                float sample =
                    clamp(pole * (static_cast<double>(previousOutput) + source - previousInput));
                previousInput = source;
                previousOutput = sample;
                std::array<float *, 1> one{&sample};
                check(legacy.process(one, 1) && colour.process(one, 1),
                      "independent legacy/Colour single sample process");
                mix = static_cast<float>(mix + smoothing * (p.mix - mix));
                outputDb = static_cast<float>(outputDb + smoothing * (p.outputDb - outputDb));
                const double blend =
                    static_cast<double>(dry) * (1 - mix) + static_cast<double>(sample) * mix;
                const double weight = guard != 0   ? (--guard, 0.)
                                      : fade < 128 ? static_cast<double>(++fade) / 128
                                                   : 1.;
                expected[i] = clamp((weight == 0 ? dry : dry * (1 - weight) + blend * weight) *
                                    std::pow(10., outputDb / 20.));
            }
            std::array<float *, 1> channel{actual.data()};
            const auto before = allocations.load();
            rejectAllocations.store(true);
            const bool processed = segment.force ? engine.processBypassed(channel, frames)
                                                 : engine.process(channel, frames);
            // Empty forced/normal calls cannot enter or leave actual bypass.
            const bool empty = engine.processBypassed(channel, 0) && engine.process(channel, 0);
            rejectAllocations.store(false);
            check(processed && empty && allocations.load() == before,
                  "Colour resume callbacks including reset and empty calls allocate nothing");
            for (std::size_t i = 0; i < frames; ++i)
                largestError =
                    std::max(largestError, std::abs(static_cast<double>(actual[i]) - expected[i]));
            absolute += frames;
        }
    }
    check(
        largestError < 2e-7,
        "full Colour reset/56 guard/128 fade matches independent frozen-legacy/output-time oracle");
}

void limiterResumeGuard(double rate) {
    darkvoice::AudioEngine engine;
    check(engine.prepare(rate, 257, 1), "resume limiter preparation");
    darkvoice::EngineParameters p;
    p.mix = 0;
    p.outputDb = 24;
    p.limiterEnabled = true;
    p.limiterCeilingDb = -6;
    p.limiterReleaseMs = 1000;
    check(engine.setParameters(p), "resume limiter snapshot");
    engine.reset();
    std::array<float, 257> data{};
    std::array<float *, 1> channel{data.data()};
    data.fill(2);
    check(engine.processBypassed(channel, data.size()), "resume limiter raw queue primed");
    data.fill(2);
    check(engine.process(channel, 257), "resume limiter full guard and fade render");
    const double ceiling = std::pow(10., -6. / 20.);
    check(data[0] > .49 && std::all_of(data.begin(), data.end(),
                                       [ceiling](float x) { return std::abs(x) <= ceiling; }),
          "enabled limiter protects very first resume/guard/fade sample after positive gain");
    // Preserve stale reduction while bypassed; net enabled state remains true.
    data.fill(.001f);
    check(engine.processBypassed(channel, data.size()), "resume limiter quiet raw queue");
    p.limiterEnabled = false;
    check(engine.setParameters(p), "disable limiter while host bypassed");
    p.limiterEnabled = true;
    check(engine.setParameters(p), "reenable limiter while host bypassed");
    auto invalid = p;
    invalid.colourModel = 2;
    invalid.limiterEnabled = false;
    check(!engine.setParameters(invalid),
          "invalid Colour snapshot cannot alter limiter pending state");
    data.fill(.001f);
    check(engine.process(channel, 1) && std::abs(data[0] - .001f * std::pow(10., 24. / 20.)) < 1e-8,
          "off/on accepted during bypass resets stale limiter gain at actual resume");
}

void invalidBlocksAndAllocationFailure() {
    darkvoice::AudioEngine first, reference;
    check(first.prepare(48000, 256, 1) && reference.prepare(48000, 256, 1),
          "invalid-history comparison setup");
    darkvoice::EngineParameters p;
    p.eqMidGainDb = 5;
    p.delayMix = 0.4f;
    p.reverbMix = 0.3f;
    p.limiterEnabled = true;
    p.limiterCeilingDb = -18;
    p.stereoDelayMix = .3f;
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
        rawAlignment(rate, 1);
        rawAlignment(rate, 2);
        neutralColourAlignment(rate);
        alignedAutomationAndTiny(rate);
        colourResumeOracle(rate);
        limiterResumeGuard(rate);
        signalTests(rate);
        integratedEffects(rate);
        integratedGate(rate);
        integratedDeEsser(rate);
        integratedModulation(rate);
        integratedFlanger(rate);
        integratedLimiter(rate);
        integratedStereoDelay(rate);
        integratedDoubler(rate);
        integratedSpaceReverb(rate);
    }
    isolationBypassAndSafety();
    invalidBlocksAndAllocationFailure();
    std::printf("Engine checks: %d, failures: %d\n", checks, failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
