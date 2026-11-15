#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <darkvoice/AudioEngine.hpp>
#include <span>
#include <string_view>
#include <vector>

namespace darkvoice {
enum class PreviewVoice : std::uint8_t { Male, Female };
enum class PreviewSampleKind : std::uint8_t { Speech, Singing };
using PreviewEffectId = std::uint16_t;
inline constexpr std::size_t previewSlotCount = 3;
inline constexpr double maximumPreviewSourceSeconds = 6;
inline constexpr double maximumPreviewTailSeconds = 3;
inline constexpr double maximumPreviewClipSeconds = 10;
constexpr bool validPreviewVoice(PreviewVoice voice) noexcept {
    return voice == PreviewVoice::Male || voice == PreviewVoice::Female;
}
constexpr bool validPreviewSampleKind(PreviewSampleKind kind) noexcept {
    return kind == PreviewSampleKind::Speech || kind == PreviewSampleKind::Singing;
}
struct PreviewFormat {
    double sampleRate = 48000;
    std::size_t channels = 2, maximumFrames = 512;
    QualityMode quality = QualityMode::Live;
};
constexpr bool validPreviewFormat(const PreviewFormat &format) noexcept {
    return (format.sampleRate == 44100 || format.sampleRate == 48000 ||
            format.sampleRate == 88200 || format.sampleRate == 96000) &&
           (format.channels == 1 || format.channels == 2) && format.maximumFrames > 0 &&
           format.maximumFrames <= 65536 && validQualityMode(format.quality);
}
struct PreviewDefinition {
    PreviewEffectId id = 0;
    std::string_view name;
    EngineParameters example{};
    PreviewSampleKind recommendedSample = PreviewSampleKind::Speech;
    bool stereoDemonstration = false;
    double tailSeconds = 0;
};
struct PreviewRequest {
    PreviewEffectId effect = 0;
    PreviewVoice voice = PreviewVoice::Male;
    PreviewSampleKind sampleKind = PreviewSampleKind::Speech;
    PreviewFormat format{};
    std::uint64_t generation = 0;
};
struct PreviewClip {
    std::array<std::vector<float>, 2> dry, processed;
    std::size_t frames = 0, channels = 0;
    double sampleRate = 0;
    QualityMode quality = QualityMode::Live;
    std::uint64_t generation = 0;
    PreviewEffectId effect = 0;
    PreviewVoice voice = PreviewVoice::Male;
    PreviewSampleKind sampleKind = PreviewSampleKind::Speech;
    float commonGain = 1;
};
enum class PreviewRenderResult {
    Success,
    Cancelled,
    InvalidInput,
    AllocationFailure,
    EngineFailure
};
std::span<const PreviewDefinition> previewCatalog() noexcept;
const PreviewDefinition *previewDefinition(PreviewEffectId) noexcept;
// Worker only. Input is finite mono audio already resampled to format.sampleRate.
PreviewRenderResult renderPreview(const PreviewDefinition &, std::span<const float> source,
                                  const PreviewRequest &,
                                  const std::atomic<std::uint64_t> &renderGeneration, PreviewClip &,
                                  std::atomic<unsigned> &progress) noexcept;
} // namespace darkvoice
