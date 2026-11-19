#include <darkvoice/EffectPreviewPlayer.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <new>
#include <utility>

namespace darkvoice {
static_assert(std::atomic<int>::is_always_lock_free);
static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
static_assert(std::atomic<std::size_t>::is_always_lock_free);
static_assert(std::atomic<float>::is_always_lock_free);
static_assert(std::atomic<bool>::is_always_lock_free);
static_assert(std::atomic<PreviewSlotState>::is_always_lock_free);
static_assert(std::atomic<PreviewState>::is_always_lock_free);
namespace {
struct Ramp {
    float value = 0, target = 0, step = 0;
    std::size_t remaining = 0;
    void set(float next, std::size_t length) noexcept {
        if (next == target)
            return;
        target = next;
        remaining = length;
        step = (target - value) / static_cast<float>(length);
    }
    float tick() noexcept {
        if (remaining != 0 && --remaining == 0)
            value = target;
        else if (remaining != 0)
            value += step;
        return value;
    }
};
float finite(float value) noexcept {
    return std::isfinite(value) ? value : 0;
}
float boundedLevel(float value) noexcept {
    return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : .5f;
}
} // namespace
struct EffectPreviewPlayer::Impl {
    PreviewFormat format;
    PreviewExchange *exchange = nullptr;
    int active = -1;
    std::uint64_t generation = 0;
    std::size_t cursor = 0, rampFrames = 1;
    Ramp blend, wet, level;
    bool stopping = false;

    void retire(bool completed) noexcept {
        if (active < 0)
            return;
        // All clip reads precede this release. Only the producer reclaims storage.
        exchange->slots[static_cast<std::size_t>(active)].state.store(PreviewSlotState::Retired,
                                                                      std::memory_order_release);
        if (completed)
            exchange->completedGeneration.store(generation, std::memory_order_release);
        active = -1;
        cursor = 0;
        generation = 0;
        blend = {};
        stopping = false;
        exchange->playingGeneration.store(0, std::memory_order_relaxed);
        exchange->positionFrames.store(0, std::memory_order_relaxed);
        exchange->totalFrames.store(0, std::memory_order_relaxed);
        exchange->audioState.store(PreviewState::Idle, std::memory_order_release);
    }
    void discardReady() noexcept {
        const int index = exchange->readyIndex.exchange(-1, std::memory_order_acquire);
        if (index >= 0 && index < static_cast<int>(previewSlotCount)) {
            auto &state = exchange->slots[static_cast<std::size_t>(index)].state;
            auto expected = PreviewSlotState::Ready;
            state.compare_exchange_strong(expected, PreviewSlotState::Retired,
                                          std::memory_order_acq_rel);
        }
    }
    bool acquire() noexcept {
        const int index = exchange->readyIndex.exchange(-1, std::memory_order_acquire);
        if (index < 0 || index >= static_cast<int>(previewSlotCount))
            return false;
        auto &slot = exchange->slots[static_cast<std::size_t>(index)];
        auto expected = PreviewSlotState::Ready;
        if (!slot.state.compare_exchange_strong(expected, PreviewSlotState::Active,
                                                std::memory_order_acq_rel))
            return false;
        const auto &clip = slot.clip;
        const auto requested = exchange->requestedGeneration.load(std::memory_order_acquire);
        bool valid =
            requested != 0 && clip.generation == requested &&
            clip.generation == exchange->renderGeneration.load(std::memory_order_acquire) &&
            clip.generation > exchange->blockedThroughGeneration.load(std::memory_order_acquire) &&
            clip.generation != exchange->completedGeneration.load(std::memory_order_acquire) &&
            clip.channels == format.channels && clip.sampleRate == format.sampleRate &&
            clip.quality == format.quality && clip.frames > 0 &&
            clip.frames <= static_cast<std::size_t>(maximumPreviewClipSeconds * format.sampleRate);
        for (std::size_t channel = 0; valid && channel < format.channels; ++channel)
            valid = clip.dry[channel].size() >= clip.frames &&
                    clip.processed[channel].size() >= clip.frames;
        if (!valid) {
            slot.state.store(PreviewSlotState::Retired, std::memory_order_release);
            return false;
        }
        active = index;
        generation = clip.generation;
        cursor = 0;
        blend = {};
        blend.set(1, std::min(rampFrames, std::max<std::size_t>(1, clip.frames / 2)));
        stopping = false;
        exchange->playingGeneration.store(generation, std::memory_order_relaxed);
        exchange->totalFrames.store(clip.frames, std::memory_order_relaxed);
        exchange->audioState.store(PreviewState::Playing, std::memory_order_release);
        return true;
    }
};
EffectPreviewPlayer::EffectPreviewPlayer() noexcept = default;
EffectPreviewPlayer::~EffectPreviewPlayer() = default;
bool EffectPreviewPlayer::prepare(const PreviewFormat &format, PreviewExchange &exchange) noexcept {
    release();
    if (!validPreviewFormat(format))
        return false;
    auto next = std::unique_ptr<Impl>(new (std::nothrow) Impl);
    if (!next)
        return false;
    next->format = format;
    next->exchange = &exchange;
    next->rampFrames = std::max<std::size_t>(1, static_cast<std::size_t>(format.sampleRate * .01));
    next->wet.value = next->wet.target = exchange.processed.load() ? 1.0f : 0.0f;
    next->level.value = next->level.target = boundedLevel(exchange.level.load());
    exchange.blocked.store(false);
    exchange.audioState.store(PreviewState::Idle);
    exchange.playingGeneration.store(0);
    exchange.positionFrames.store(0);
    exchange.totalFrames.store(0);
    impl_ = std::move(next);
    return true;
}
void EffectPreviewPlayer::release() noexcept {
    if (impl_) {
        impl_->retire(false);
        impl_->discardReady();
        impl_->exchange->audioState.store(PreviewState::Unavailable, std::memory_order_release);
        impl_.reset();
    }
}
PreviewProcessResult EffectPreviewPlayer::process(std::span<float *> channels, std::size_t frames,
                                                  bool blocked) noexcept {
    PreviewProcessResult result;
    if (!impl_)
        return result;
    auto &self = *impl_;
    auto &exchange = *self.exchange;
    if (blocked) {
        exchange.blocked.store(true, std::memory_order_relaxed);
        const auto rendered = exchange.renderGeneration.exchange(0, std::memory_order_acq_rel);
        const auto requested = exchange.requestedGeneration.load(std::memory_order_acquire);
        const auto previous = exchange.blockedThroughGeneration.load(std::memory_order_relaxed);
        // Audio is the only running writer of this monotonic watermark. Include the
        // exchanged render generation if a request raced with the blocking callback.
        exchange.blockedThroughGeneration.store(std::max({previous, requested, rendered}),
                                                std::memory_order_release);
        self.retire(false);
        self.discardReady();
        exchange.audioState.store(PreviewState::Idle, std::memory_order_release);
        return result;
    }
    if (frames == 0)
        return result;
    if (frames > self.format.maximumFrames || channels.size() != self.format.channels)
        return result;
    const auto bytes = frames * sizeof(float);
    for (std::size_t channel = 0; channel < channels.size(); ++channel) {
        if (channels[channel] == nullptr)
            return result;
        const auto address = reinterpret_cast<std::uintptr_t>(channels[channel]);
        if (address > std::numeric_limits<std::uintptr_t>::max() - bytes)
            return result;
        for (std::size_t other = 0; other < channel; ++other) {
            const auto previous = reinterpret_cast<std::uintptr_t>(channels[other]);
            if ((address >= previous ? address - previous : previous - address) < bytes)
                return result;
        }
    }
    exchange.blocked.store(false, std::memory_order_relaxed);
    const auto requested = exchange.requestedGeneration.load(std::memory_order_acquire);
    const auto rendered = exchange.renderGeneration.load(std::memory_order_acquire);
    if (self.active >= 0 && (self.generation != requested || self.generation != rendered)) {
        self.stopping = true;
        self.blend.set(0, self.rampFrames);
        exchange.audioState.store(PreviewState::Stopping, std::memory_order_release);
    }
    self.wet.set(exchange.processed.load(std::memory_order_relaxed) ? 1.0f : 0.0f, self.rampFrames);
    self.level.set(boundedLevel(exchange.level.load(std::memory_order_relaxed)), self.rampFrames);
    if (self.active < 0)
        self.acquire();
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const float wet = self.wet.tick();
        const float level = self.level.tick();
        if (self.active < 0)
            continue;
        const auto &clip = exchange.slots[static_cast<std::size_t>(self.active)].clip;
        const auto remaining = clip.frames - self.cursor;
        if (!self.stopping && remaining <= std::min(self.rampFrames, clip.frames / 2))
            self.blend.set(0, remaining);
        const float blend = self.blend.tick();
        if (blend > 0) {
            for (std::size_t channel = 0; channel < channels.size(); ++channel) {
                const double dry = finite(clip.dry[channel][self.cursor]);
                const double processed = finite(clip.processed[channel][self.cursor]);
                const double preview = (dry * (1.0 - wet) + processed * wet) * level;
                const double mixed =
                    static_cast<double>(finite(channels[channel][frame])) * (1.0 - blend) +
                    preview * blend;
                channels[channel][frame] = static_cast<float>(
                    std::clamp(mixed, -static_cast<double>(std::numeric_limits<float>::max()),
                               static_cast<double>(std::numeric_limits<float>::max())));
            }
            result.replaced = true;
        }
        ++self.cursor;
        const bool finished = self.cursor == clip.frames;
        // Do not inspect clip (or its vectors) after retire: a worker may reclaim it immediately.
        if (finished || (self.stopping && self.blend.value == 0))
            self.retire(finished && !self.stopping);
        else
            exchange.positionFrames.store(self.cursor, std::memory_order_release);
    }
    if (result.replaced)
        for (auto *channel : channels)
            for (std::size_t frame = 0; frame < frames; ++frame)
                result.outputPeak = std::max(result.outputPeak, std::abs(finite(channel[frame])));
    return result;
}
} // namespace darkvoice
