#pragma once
#include <darkvoice/EffectPreview.hpp>
#include <memory>

namespace darkvoice {
enum class PreviewState { Idle, Preparing, WaitingForAudio, Playing, Stopping, Unavailable, Error };
enum class PreviewSlotState { Free, Writing, Ready, Active, Retired };
struct PreviewSlot {
    PreviewClip clip;
    std::atomic<PreviewSlotState> state{PreviewSlotState::Free};
};
// UI/worker own requests and storage; audio owns activation/cursors/retirement.
struct PreviewExchange {
    std::array<PreviewSlot, previewSlotCount> slots;
    std::atomic<int> readyIndex{-1};
    std::atomic<std::uint64_t> requestedGeneration{0}, renderGeneration{0};
    std::atomic<std::uint64_t> blockedThroughGeneration{0};
    std::atomic<bool> processed{true}, blocked{false};
    std::atomic<float> level{.5f};
    std::atomic<PreviewState> audioState{PreviewState::Idle};
    std::atomic<std::uint64_t> playingGeneration{0}, completedGeneration{0};
    std::atomic<std::size_t> positionFrames{0}, totalFrames{0};
};
struct PreviewProcessResult {
    bool replaced = false;
    float outputPeak = 0;
};
class EffectPreviewPlayer {
  public:
    EffectPreviewPlayer() noexcept;
    ~EffectPreviewPlayer();
    EffectPreviewPlayer(const EffectPreviewPlayer &) = delete;
    EffectPreviewPlayer &operator=(const EffectPreviewPlayer &) = delete;
    // Lifecycle only: callbacks and producer must be stopped.
    bool prepare(const PreviewFormat &, PreviewExchange &) noexcept;
    void release() noexcept;
    PreviewProcessResult process(std::span<float *>, std::size_t frames, bool blocked) noexcept;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace darkvoice
