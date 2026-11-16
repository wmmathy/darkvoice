#include <darkvoice/EffectPreview.hpp>

#include <algorithm>
#include <cmath>
#include <new>

namespace darkvoice {
namespace {
const auto catalog = [] {
    std::array<PreviewDefinition, 27> result{};
    std::size_t next = 0;
    auto add = [&](std::string_view name, auto configure,
                   PreviewSampleKind kind = PreviewSampleKind::Singing, double tail = 0,
                   bool stereo = false) {
        auto &d = result[next++];
        d.id = static_cast<PreviewEffectId>(next);
        d.name = name;
        configure(d.example);
        d.recommendedSample = kind;
        d.tailSeconds = tail;
        d.stereoDemonstration = stereo;
    };
    using P = EngineParameters;
    using K = PreviewSampleKind;
    add("High-pass filter", [](P &p) { p.highPassHz = 250; });
    add(
        "Compressor",
        [](P &p) {
            p.thresholdDb = -28;
            p.ratio = 6;
        },
        K::Speech);
    add("Saturation", [](P &p) { p.drive = .7f; });
    add("Parametric EQ", [](P &p) {
        p.eqMidGainDb = 9;
        p.eqMidQ = 1.4f;
    });
    add(
        "Delay",
        [](P &p) {
            p.delayMs = 240;
            p.delayFeedback = .35f;
            p.delayMix = .45f;
        },
        K::Singing, 1.5);
    add(
        "Reverb",
        [](P &p) {
            p.reverbSize = .7f;
            p.reverbMix = .5f;
        },
        K::Singing, 2);
    add(
        "Gate / expander",
        [](P &p) {
            p.gateThresholdDb = -25;
            p.gateRatio = 8;
        },
        K::Speech);
    add(
        "De-esser",
        [](P &p) {
            p.deEssThresholdDb = -38;
            p.deEssReductionDb = 15;
        },
        K::Speech);
    add("Chorus", [](P &p) { p.chorusMix = .5f; }, K::Singing, .1);
    add(
        "Stereo width (stereo demonstration)", [](P &p) { p.stereoWidth = 1.8f; }, K::Singing, 0,
        true);
    add(
        "Flanger",
        [](P &p) {
            p.flangerMix = .65f;
            p.flangerFeedback = .45f;
        },
        K::Singing, .1);
    add(
        "Sample-peak limiter",
        [](P &p) {
            p.limiterEnabled = true;
            p.limiterCeilingDb = -18;
        },
        K::Speech);
    add(
        "Stereo delay",
        [](P &p) {
            p.stereoDelayMix = .5f;
            p.stereoDelayMs = 280;
            p.stereoDelayFeedback = .4f;
        },
        K::Singing, 2);
    add("Doubler", [](P &p) { p.doublerMix = .5f; }, K::Singing, .1);
    add(
        "Room",
        [](P &p) {
            p.spaceModel = 0;
            p.spaceMix = .5f;
        },
        K::Singing, 2);
    add(
        "Hall",
        [](P &p) {
            p.spaceModel = 1;
            p.spaceMix = .5f;
        },
        K::Singing, 3);
    add(
        "Plate",
        [](P &p) {
            p.spaceModel = 2;
            p.spaceMix = .5f;
        },
        K::Singing, 3);
    add("Tape colour", [](P &p) {
        p.colourModel = 0;
        p.colourDrive = .8f;
        p.colourMix = 1;
    });
    add("Tube colour", [](P &p) {
        p.colourModel = 1;
        p.colourDrive = .8f;
        p.colourMix = 1;
    });
    add("Body", [](P &p) { p.sculptBodyDb = 6; });
    add("Warmth", [](P &p) { p.sculptWarmthDb = 6; });
    add("Presence", [](P &p) { p.sculptPresenceDb = 6; });
    add("Breath texture", [](P &p) { p.sculptBreathiness = .9f; });
    add("Depth", [](P &p) { p.sculptDepthDb = 6; });
    add("Resonance", [](P &p) { p.sculptResonanceDb = 6; });
    add("Character: odd harmonics", [](P &p) { p.sculptCharacter = -1; });
    add("Character: even harmonics", [](P &p) { p.sculptCharacter = 1; });
    return result;
}();
} // namespace

std::span<const PreviewDefinition> previewCatalog() noexcept {
    return catalog;
}
const PreviewDefinition *previewDefinition(PreviewEffectId id) noexcept {
    for (const auto &d : catalog)
        if (d.id == id)
            return &d;
    return nullptr;
}
PreviewRenderResult renderPreview(const PreviewDefinition &definition,
                                  std::span<const float> source, const PreviewRequest &request,
                                  const std::atomic<std::uint64_t> &generation, PreviewClip &output,
                                  std::atomic<unsigned> &progress) noexcept {
    progress.store(0, std::memory_order_relaxed);
    const auto cancelled = [&] {
        return generation.load(std::memory_order_acquire) != request.generation;
    };
    if (!validPreviewFormat(request.format) || !validPreviewVoice(request.voice) ||
        !validPreviewSampleKind(request.sampleKind) ||
        !validPreviewSampleKind(definition.recommendedSample) || !request.generation ||
        !definition.id || request.effect != definition.id || definition.name.empty() ||
        !std::isfinite(definition.tailSeconds) || definition.tailSeconds < 0 ||
        definition.tailSeconds > maximumPreviewTailSeconds || source.empty() ||
        source.size() > maximumPreviewSourceSeconds * request.format.sampleRate ||
        (definition.stereoDemonstration && request.format.channels != 2) ||
        !std::all_of(source.begin(), source.end(), [](float x) { return std::isfinite(x); }))
        return PreviewRenderResult::InvalidInput;
    if (cancelled())
        return PreviewRenderResult::Cancelled;
    try {
        AudioEngine engine;
        // Validate the entire static snapshot before allocating output storage.
        if (!engine.setParameters(definition.example))
            return PreviewRenderResult::InvalidInput;
        if (!engine.prepare(request.format.sampleRate, 512, request.format.channels,
                            request.format.quality))
            return PreviewRenderResult::EngineFailure;
        if (!engine.setParameters(definition.example))
            return PreviewRenderResult::InvalidInput;
        engine.reset();
        const auto delay =
            definition.stereoDemonstration
                ? static_cast<std::size_t>(std::llround(.007 * request.format.sampleRate))
                : 0;
        const auto tail =
            static_cast<std::size_t>(std::ceil(definition.tailSeconds * request.format.sampleRate));
        const auto frames = source.size() + delay + tail;
        if (frames > maximumPreviewClipSeconds * request.format.sampleRate)
            return PreviewRenderResult::InvalidInput;
        PreviewClip clip;
        clip.frames = frames;
        clip.channels = request.format.channels;
        clip.sampleRate = request.format.sampleRate;
        clip.quality = request.format.quality;
        clip.generation = request.generation;
        clip.effect = request.effect;
        clip.voice = request.voice;
        clip.sampleKind = request.sampleKind;
        const auto latency = engine.latencySamples();
        const auto fade = static_cast<std::size_t>(std::llround(.005 * clip.sampleRate));
        for (std::size_t c = 0; c < clip.channels; ++c) {
            clip.dry[c].assign(frames, 0);
            const auto offset = c == 1 ? delay : 0;
            for (std::size_t i = 0; i < source.size(); ++i) {
                const double gain = std::min({1., static_cast<double>(i) / fade,
                                              static_cast<double>(source.size() - 1 - i) / fade});
                clip.dry[c][offset + i] = static_cast<float>(source[i] * gain);
            }
            clip.processed[c] = clip.dry[c];
            clip.processed[c].resize(frames + latency, 0);
        }
        for (std::size_t offset = 0; offset < frames + latency; offset += 512) {
            if (cancelled())
                return PreviewRenderResult::Cancelled;
            std::array<float *, 2> channels{};
            for (std::size_t c = 0; c < clip.channels; ++c)
                channels[c] = clip.processed[c].data() + offset;
            if (!engine.process(std::span<float *>(channels.data(), clip.channels),
                                std::min(std::size_t{512}, frames + latency - offset)))
                return PreviewRenderResult::EngineFailure;
            progress.store(
                static_cast<unsigned>(
                    99 * (offset + std::min(std::size_t{512}, frames + latency - offset)) /
                    (frames + latency)),
                std::memory_order_relaxed);
        }
        double peak = 0;
        for (std::size_t c = 0; c < clip.channels; ++c) {
            clip.processed[c].erase(clip.processed[c].begin(), clip.processed[c].begin() + latency);
            for (std::size_t i = 0; i < frames; ++i) {
                if (!std::isfinite(clip.processed[c][i]))
                    return PreviewRenderResult::EngineFailure;
                peak = std::max({peak, std::abs(static_cast<double>(clip.dry[c][i])),
                                 std::abs(static_cast<double>(clip.processed[c][i]))});
            }
        }
        clip.commonGain = static_cast<float>(peak > .8 ? .8 / peak : 1);
        for (std::size_t c = 0; c < clip.channels; ++c)
            for (std::size_t i = 0; i < frames; ++i) {
                clip.dry[c][i] *= clip.commonGain;
                clip.processed[c][i] *= clip.commonGain;
            }
        if (cancelled())
            return PreviewRenderResult::Cancelled;
        output = std::move(clip);
        progress.store(100, std::memory_order_release);
        return PreviewRenderResult::Success;
    } catch (const std::bad_alloc &) {
        return PreviewRenderResult::AllocationFailure;
    } catch (...) {
        return PreviewRenderResult::EngineFailure;
    }
}
} // namespace darkvoice
