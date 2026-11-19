#include <algorithm>
#include <cmath>
#include <cstdio>
#include <darkvoice/EffectPreview.hpp>
#include <limits>
#include <numbers>
#include <set>
#include <thread>

namespace {
int checks = 0, failures = 0;
void check(bool value, const char *label) {
    ++checks;
    if (!value) {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", label);
    }
}
std::vector<float> fixture(double rate, double seconds = .12) {
    std::vector<float> x(static_cast<std::size_t>(rate * seconds));
    for (std::size_t i = 0; i < x.size(); ++i) {
        const double t = i / rate;
        x[i] = static_cast<float>((.6 + .4 * std::sin(2 * std::numbers::pi * 13 * t)) *
                                  (.36 * std::sin(2 * std::numbers::pi * 120 * t) +
                                   .2 * std::sin(2 * std::numbers::pi * 1200 * t) +
                                   .12 * std::sin(2 * std::numbers::pi * 6500 * t)));
    }
    return x;
}
void catalogTests() {
    std::set<darkvoice::PreviewEffectId> ids;
    check(darkvoice::previewCatalog().size() == 30, "complete effect family catalog");
    std::array<std::vector<float>, 3> transducerOutputs;
    darkvoice::AudioEngine validator;
    check(validator.prepare(48000, 512, 2), "validator prepared");
    for (const auto &d : darkvoice::previewCatalog()) {
        check(d.id && ids.insert(d.id).second && !d.name.empty(), "unique positive named ID");
        check(darkvoice::previewDefinition(d.id) == &d, "stable catalog lookup");
        if (!validator.setParameters(d.example))
            std::fprintf(stderr, "INVALID %.*s\n", static_cast<int>(d.name.size()), d.name.data());
        check(validator.setParameters(d.example), "valid curated snapshot");
        check(d.tailSeconds >= 0 && d.tailSeconds <= 3, "bounded tail");
        auto source = fixture(48000, .4);
        darkvoice::PreviewRequest r;
        r.effect = d.id;
        r.generation = 1;
        darkvoice::PreviewClip out;
        std::atomic<std::uint64_t> gen{1};
        std::atomic<unsigned> progress{0};
        check(darkvoice::renderPreview(d, source, r, gen, out, progress) ==
                  darkvoice::PreviewRenderResult::Success,
              "catalog example renders");
        double difference = 0;
        for (std::size_t c = 0; c < out.channels; ++c)
            for (std::size_t i = 0; i < out.frames; ++i)
                difference += std::abs(out.processed[c][i] - out.dry[c][i]);
        check(difference > .001, "example differs audibly in signal domain");
        if (d.id >= 28 && d.id <= 30) {
            const auto model = static_cast<std::size_t>(d.id - 28);
            check(d.example.transducerModel == static_cast<int>(model) &&
                      d.example.transducerMix == 1 &&
                      d.recommendedSample == darkvoice::PreviewSampleKind::Speech &&
                      d.tailSeconds == .25,
                  "transducer example has actual model and speech recommendation");
            transducerOutputs[model] = out.processed[0];
        }
    }
    for (std::size_t model = 0; model < transducerOutputs.size(); ++model)
        for (std::size_t other = 0; other < model; ++other) {
            check(transducerOutputs[model].size() == transducerOutputs[other].size(),
                  "transducer comparison shares source clock and tail");
            double difference = 0;
            for (std::size_t i = 0; i < transducerOutputs[model].size(); ++i)
                difference += std::abs(transducerOutputs[model][i] - transducerOutputs[other][i]);
            check(difference > .001, "transducer previews differ from each other");
        }
    check(!darkvoice::previewDefinition(0) && !darkvoice::previewDefinition(65535),
          "unknown lookup");
}
void alignmentTests() {
    for (auto quality : {darkvoice::QualityMode::Live, darkvoice::QualityMode::Studio,
                         darkvoice::QualityMode::Offline})
        for (double rate : {44100., 48000., 88200., 96000.})
            for (std::size_t channels : {1u, 2u}) {
                darkvoice::PreviewDefinition d{700, "Neutral"};
                darkvoice::PreviewRequest r;
                r.effect = d.id;
                r.generation = 11;
                r.format = {rate, channels, 37, quality};
                auto source = fixture(rate);
                darkvoice::PreviewClip clip;
                std::atomic<std::uint64_t> gen{11};
                std::atomic<unsigned> progress{0};
                check(darkvoice::renderPreview(d, source, r, gen, clip, progress) ==
                          darkvoice::PreviewRenderResult::Success,
                      "all formats render");
                check(clip.frames == source.size() && clip.channels == channels &&
                          clip.quality == quality && clip.sampleRate == rate &&
                          clip.generation == 11 && clip.effect == 700 && clip.voice == r.voice &&
                          clip.sampleKind == r.sampleKind && progress == 100,
                      "clip metadata");
                darkvoice::AudioEngine direct;
                check(direct.prepare(rate, 512, channels, quality) &&
                          direct.setParameters(d.example),
                      "direct prepare");
                direct.reset();
                std::array<std::vector<float>, 2> data;
                for (std::size_t c = 0; c < channels; ++c) {
                    data[c] = clip.dry[c];
                    data[c].resize(clip.frames + direct.latencySamples());
                }
                bool equal = true;
                for (std::size_t offset = 0; offset < data[0].size(); offset += 512) {
                    std::array<float *, 2> pointers{};
                    for (std::size_t c = 0; c < channels; ++c)
                        pointers[c] = data[c].data() + offset;
                    check(direct.process({pointers.data(), channels},
                                         std::min(std::size_t{512}, data[0].size() - offset)),
                          "direct chunk");
                }
                for (std::size_t c = 0; c < channels; ++c)
                    for (std::size_t i = 0; i < clip.frames; ++i)
                        equal &= data[c][i + direct.latencySamples()] == clip.processed[c][i];
                check(equal, "only reported latency removed: exact direct engine");
                // A real time effect must preserve its delay, not align away the echo.
                auto echo = *darkvoice::previewDefinition(5);
                echo.tailSeconds = .1;
                r.effect = echo.id;
                check(darkvoice::renderPreview(echo, source, r, gen, clip, progress) ==
                          darkvoice::PreviewRenderResult::Success,
                      "effect all formats");
                check(clip.frames == source.size() + static_cast<std::size_t>(std::ceil(.1 * rate)),
                      "effect tail frame bound");
            }
}
void failuresAndGain() {
    auto d = *darkvoice::previewDefinition(1);
    darkvoice::PreviewRequest r;
    r.effect = d.id;
    r.generation = 2;
    std::atomic<std::uint64_t> gen{2};
    std::atomic<unsigned> progress{0};
    darkvoice::PreviewClip clip;
    clip.generation = 999;
    auto source = fixture(48000);
    auto render = [&] { return darkvoice::renderPreview(d, source, r, gen, clip, progress); };
    auto invalid = [&] {
        check(render() == darkvoice::PreviewRenderResult::InvalidInput && clip.generation == 999,
              "invalid preserves output");
    };
    r.generation = 0;
    invalid();
    r.generation = 2;
    r.effect = 0;
    invalid();
    r.effect = d.id;
    r.voice = static_cast<darkvoice::PreviewVoice>(9);
    invalid();
    r.voice = darkvoice::PreviewVoice::Female;
    r.sampleKind = static_cast<darkvoice::PreviewSampleKind>(9);
    invalid();
    r.sampleKind = darkvoice::PreviewSampleKind::Singing;
    r.format.sampleRate = 32000;
    invalid();
    r.format.sampleRate = 48000;
    d.tailSeconds = 4;
    invalid();
    d.tailSeconds = 0;
    d.example.drive = std::numeric_limits<float>::quiet_NaN();
    invalid();
    d.example.drive = 0;
    source[1] = std::numeric_limits<float>::infinity();
    invalid();
    source = fixture(48000);
    gen = 3;
    check(render() == darkvoice::PreviewRenderResult::Cancelled && clip.generation == 999,
          "cancel preserves output");
    gen = 2;
    source.clear();
    invalid();
    source.assign(288001, 0);
    invalid();
    source = fixture(48000);
    d = *darkvoice::previewDefinition(10);
    r.effect = d.id;
    r.format.channels = 1;
    invalid();
    r.format.channels = 2;
    check(render() == darkvoice::PreviewRenderResult::Success, "stereo demonstration");
    const auto delay = static_cast<std::size_t>(336);
    check(clip.frames == source.size() + delay, "stereo extra delay bounded");
    bool identical = true;
    for (std::size_t i = 0; i < source.size(); ++i)
        identical &= clip.dry[0][i] == clip.dry[1][i + delay];
    check(identical, "shared stereo source delay");
    d = {701, "Neutral gain"};
    r.effect = d.id;
    source.assign(4800, 2);
    check(render() == darkvoice::PreviewRenderResult::Success, "common safety render");
    check(std::abs(clip.commonGain - .4f) < 1e-6, "common gain based on peak");
    bool bounded = true;
    for (std::size_t c = 0; c < 2; ++c)
        for (std::size_t i = 0; i < clip.frames; ++i)
            bounded &= std::isfinite(clip.processed[c][i]) &&
                       std::abs(clip.processed[c][i]) <= .800001f &&
                       std::abs(clip.dry[c][i]) <= .800001f;
    check(bounded, "finite common bounded output");
    check(clip.dry[0].front() == 0 && clip.dry[0].back() == 0, "shared edge fades");
    source.assign(288000, 0);
    d.tailSeconds = 3;
    check(render() == darkvoice::PreviewRenderResult::Success && clip.frames == 432000,
          "maximum source and tail");
    d = *darkvoice::previewDefinition(5);
    r.effect = d.id;
    source.assign(4800, 0);
    source[480] = .5f;
    check(render() == darkvoice::PreviewRenderResult::Success, "delayed impulse render");
    double echoEnergy = 0;
    for (std::size_t i = 10000; i < 13000; ++i)
        echoEnergy += std::abs(clip.processed[0][i]);
    check(echoEnergy > .01, "real echo remains at its musical delay after latency crop");
    source.clear();
    clip.generation = 999;
    invalid();
    source.assign(288001, 0);
    invalid();
    // Cancel after real processing has begun, rather than merely a stale request.
    source = fixture(48000, 6);
    clip.generation = 999;
    progress = 0;
    std::thread cancel([&] {
        while (progress.load(std::memory_order_acquire) == 0)
            std::this_thread::yield();
        gen.store(3, std::memory_order_release);
    });
    const auto result = render();
    cancel.join();
    check(result == darkvoice::PreviewRenderResult::Cancelled && clip.generation == 999,
          "in-flight cancellation discards partial clip");
}
} // namespace
int main() {
    catalogTests();
    alignmentTests();
    failuresAndGain();
    std::printf("Preview renderer: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
