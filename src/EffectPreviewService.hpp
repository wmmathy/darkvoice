#pragma once
#include <darkvoice/EffectPreviewPlayer.hpp>
#include <memory>
#include <string>

namespace darkvoice {
enum class PreviewError {
    None,
    InvalidRequest,
    UnsupportedFormat,
    StereoRequired,
    SourceUnavailable,
    DecodeFailure,
    RenderFailure
};
struct PreviewStatus {
    PreviewState state = PreviewState::Unavailable;
    PreviewError error = PreviewError::None;
    PreviewEffectId effect = 0;
    PreviewVoice voice = PreviewVoice::Male;
    PreviewSampleKind sampleKind = PreviewSampleKind::Speech;
    unsigned progress = 0;
    std::size_t positionFrames = 0, totalFrames = 0;
    bool prepared = false, blocked = false, processed = true;
    float level = .5f;
    PreviewFormat format{};
};
class EffectPreviewService {
  public:
    EffectPreviewService();
    ~EffectPreviewService();
    EffectPreviewService(const EffectPreviewService &) = delete;
    EffectPreviewService &operator=(const EffectPreviewService &) = delete;
    bool prepare(const PreviewFormat &) noexcept;
    void release() noexcept;
    bool request(PreviewEffectId, PreviewVoice, PreviewSampleKind);
    void stop() noexcept;
    void setProcessed(bool) noexcept;
    bool setLevel(float) noexcept;
    PreviewStatus status() const;
    PreviewProcessResult process(std::span<float *>, std::size_t frames, bool blocked) noexcept;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
// UI only: all recording copyright/licence/change notices, no network request.
std::string effectPreviewCredits();
} // namespace darkvoice
