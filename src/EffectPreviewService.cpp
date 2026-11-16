#include "EffectPreviewService.hpp"
#include <DarkVoicePreviewAssets.h>
#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <juce_audio_formats/juce_audio_formats.h>
#include <mutex>
#include <optional>
#include <thread>

namespace darkvoice {
namespace {
struct Asset {
    const void *data;
    int size;
    std::size_t cue;
};
Asset assetFor(PreviewVoice voice, PreviewSampleKind kind) {
    using namespace DarkVoicePreviewAssets;
    if (kind == PreviewSampleKind::Speech)
        return voice == PreviewVoice::Male ? Asset{male_speech_wav, male_speech_wavSize, 54077}
                                           : Asset{female_speech_wav, female_speech_wavSize, 56002};
    return voice == PreviewVoice::Male ? Asset{male_singing_wav, male_singing_wavSize, 0}
                                       : Asset{female_singing_wav, female_singing_wavSize, 0};
}
PreviewError decode(const PreviewRequest &request, std::vector<float> &output) {
    const auto asset = assetFor(request.voice, request.sampleKind);
    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(
        new juce::MemoryInputStream(asset.data, static_cast<std::size_t>(asset.size), false),
        true));
    if (!reader)
        return PreviewError::DecodeFailure;
    if (reader->numChannels != 1 || reader->sampleRate <= 0 || reader->lengthInSamples <= 0)
        return PreviewError::DecodeFailure;
    const auto frames = std::min<std::size_t>(
        static_cast<std::size_t>(reader->lengthInSamples),
        asset.cue != 0
            ? asset.cue
            : static_cast<std::size_t>(maximumPreviewSourceSeconds * reader->sampleRate));
    juce::AudioBuffer<float> buffer(1, static_cast<int>(frames));
    if (!reader->read(&buffer, 0, static_cast<int>(frames), 0, true, false))
        return PreviewError::DecodeFailure;
    const auto *source = buffer.getReadPointer(0);
    if (!std::all_of(source, source + frames, [](float x) { return std::isfinite(x); }))
        return PreviewError::DecodeFailure;
    if (reader->sampleRate == request.format.sampleRate) {
        output.assign(source, source + frames);
        return PreviewError::None;
    }
    const auto ratio = reader->sampleRate / request.format.sampleRate;
    const auto count = static_cast<std::size_t>(std::llround(static_cast<double>(frames) / ratio));
    juce::WindowedSincInterpolator interpolator;
    const auto delay =
        static_cast<std::size_t>(std::llround(interpolator.getBaseLatency() / ratio));
    // Include enough trailing zero input to flush the FIR and rounded delay.
    std::vector<float> padded(frames + 512, 0.0f);
    std::copy_n(source, frames, padded.begin());
    std::vector<float> resampled(count + delay);
    interpolator.process(ratio, padded.data(), resampled.data(),
                         static_cast<int>(resampled.size()));
    output.assign(resampled.begin() + static_cast<std::ptrdiff_t>(delay), resampled.end());
    return PreviewError::None;
}
} // namespace

struct EffectPreviewService::Impl {
    PreviewExchange exchange;
    EffectPreviewPlayer player;
    mutable std::mutex mutex;
    std::condition_variable condition;
    std::thread worker;
    std::optional<PreviewRequest> pending;
    PreviewRequest current;
    PreviewFormat format;
    PreviewError error = PreviewError::None;
    std::atomic<unsigned> progress{0};
    std::atomic<std::uint64_t> workingGeneration{0};
    std::uint64_t serial = 0, readyGeneration = 0;
    std::size_t readyFrames = 0;
    bool prepared = false, quitting = false;

    void shutdown() noexcept {
        {
            std::lock_guard lock(mutex);
            // Close the UI request gate before joining outside the mutex.
            // A request already holding the lock completes before cancellation;
            // later requests cannot inspect or replace the joining thread.
            prepared = false;
            quitting = true;
            exchange.requestedGeneration.store(0);
            exchange.renderGeneration.store(0);
            pending.reset();
        }
        condition.notify_all();
        if (worker.joinable())
            worker.join();
        player.release();
        exchange.readyIndex.store(-1);
        for (auto &slot : exchange.slots) {
            slot.clip = {};
            slot.state.store(PreviewSlotState::Free);
        }
        std::lock_guard lock(mutex);
        prepared = false;
        readyGeneration = 0;
        error = PreviewError::None;
    }
    void run() noexcept {
        for (;;) {
            PreviewRequest request;
            {
                std::unique_lock lock(mutex);
                condition.wait(lock, [&] { return quitting || pending.has_value(); });
                if (quitting)
                    return;
                request = *pending;
                pending.reset();
            }
            workingGeneration.store(request.generation);
            progress.store(0);
            int index = -1;
            for (std::size_t i = 0; i < previewSlotCount; ++i) {
                auto state = exchange.slots[i].state.load(std::memory_order_acquire);
                if ((state == PreviewSlotState::Free || state == PreviewSlotState::Retired) &&
                    exchange.slots[i].state.compare_exchange_strong(
                        state, PreviewSlotState::Writing, std::memory_order_acq_rel)) {
                    index = static_cast<int>(i);
                    break;
                }
            }
            if (index < 0) {
                // The mailbox can contain a superseded clip; exchanging transfers ownership.
                index = exchange.readyIndex.exchange(-1, std::memory_order_acq_rel);
                if (index >= 0)
                    exchange.slots[static_cast<std::size_t>(index)].state.store(
                        PreviewSlotState::Writing);
            }
            if (index < 0)
                continue;
            auto &slot = exchange.slots[static_cast<std::size_t>(index)];
            PreviewError failure = PreviewError::None;
            PreviewRenderResult result = PreviewRenderResult::Cancelled;
            try {
                std::vector<float> source;
                failure = decode(request, source);
                if (failure == PreviewError::None &&
                    exchange.renderGeneration.load() == request.generation)
                    result = renderPreview(*previewDefinition(request.effect), source, request,
                                           exchange.renderGeneration, slot.clip, progress);
                if (failure == PreviewError::None && result != PreviewRenderResult::Success &&
                    result != PreviewRenderResult::Cancelled)
                    failure = PreviewError::RenderFailure;
            } catch (...) {
                failure = PreviewError::RenderFailure;
            }
            // Serialize publication with UI selection, but never decoding or rendering.
            std::lock_guard lock(mutex);
            if (failure == PreviewError::None && result == PreviewRenderResult::Success &&
                exchange.requestedGeneration.load() == request.generation &&
                exchange.renderGeneration.load() == request.generation &&
                exchange.blockedThroughGeneration.load() < request.generation && !quitting) {
                slot.state.store(PreviewSlotState::Ready, std::memory_order_release);
                const auto old = exchange.readyIndex.exchange(index, std::memory_order_acq_rel);
                if (old >= 0)
                    exchange.slots[static_cast<std::size_t>(old)].state.store(
                        PreviewSlotState::Free, std::memory_order_release);
                readyGeneration = request.generation;
                readyFrames = slot.clip.frames;
            } else {
                slot.state.store(PreviewSlotState::Free, std::memory_order_release);
                if (exchange.requestedGeneration.load() == request.generation)
                    error = failure;
            }
        }
    }
};

EffectPreviewService::EffectPreviewService() : impl_(std::make_unique<Impl>()) {}
EffectPreviewService::~EffectPreviewService() {
    release();
}
bool EffectPreviewService::prepare(const PreviewFormat &format) noexcept {
    impl_->shutdown();
    if (!validPreviewFormat(format)) {
        std::lock_guard lock(impl_->mutex);
        impl_->error = PreviewError::UnsupportedFormat;
        return false;
    }
    if (!impl_->player.prepare(format, impl_->exchange))
        return false;
    std::lock_guard lock(impl_->mutex);
    impl_->format = format;
    impl_->prepared = true;
    impl_->quitting = false;
    return true;
}
void EffectPreviewService::release() noexcept {
    impl_->shutdown();
}
bool EffectPreviewService::request(PreviewEffectId id, PreviewVoice voice, PreviewSampleKind kind) {
    const auto *definition = previewDefinition(id);
    std::lock_guard lock(impl_->mutex);
    if (!definition || !validPreviewVoice(voice) || !validPreviewSampleKind(kind))
        return false;
    if (!impl_->prepared || impl_->quitting ||
        (definition->stereoDemonstration && impl_->format.channels != 2))
        return false;
    if (!impl_->worker.joinable()) {
        try {
            impl_->worker = std::thread([this] { impl_->run(); });
        } catch (...) {
            impl_->error = PreviewError::RenderFailure;
            return false;
        }
    }
    impl_->current = {id, voice, kind, impl_->format, ++impl_->serial};
    impl_->pending = impl_->current;
    impl_->error = PreviewError::None;
    impl_->exchange.renderGeneration.store(impl_->serial);
    impl_->exchange.requestedGeneration.store(impl_->serial);
    impl_->condition.notify_one();
    return true;
}
void EffectPreviewService::stop() noexcept {
    std::lock_guard lock(impl_->mutex);
    impl_->pending.reset();
    impl_->error = PreviewError::None;
    impl_->exchange.requestedGeneration.store(0);
    impl_->exchange.renderGeneration.store(0);
}
void EffectPreviewService::setProcessed(bool value) noexcept {
    impl_->exchange.processed.store(value);
}
bool EffectPreviewService::setLevel(float value) noexcept {
    if (!std::isfinite(value) || value < 0 || value > 1)
        return false;
    impl_->exchange.level.store(value);
    return true;
}
PreviewStatus EffectPreviewService::status() const {
    std::lock_guard lock(impl_->mutex);
    PreviewStatus status;
    const auto &x = impl_->exchange;
    status.prepared = impl_->prepared;
    status.format = impl_->format;
    status.error = impl_->error;
    status.effect = impl_->current.effect;
    status.voice = impl_->current.voice;
    status.sampleKind = impl_->current.sampleKind;
    status.blocked = x.blocked.load();
    status.processed = x.processed.load();
    status.level = x.level.load();
    status.progress = impl_->workingGeneration.load() == x.requestedGeneration.load()
                          ? impl_->progress.load()
                          : 0;
    status.positionFrames = x.positionFrames.load();
    status.totalFrames = x.totalFrames.load();
    const auto wanted = x.requestedGeneration.load();
    if (!status.prepared)
        status.state = PreviewState::Unavailable;
    else if (status.error != PreviewError::None)
        status.state = PreviewState::Error;
    else if (wanted == 0 && x.audioState.load() == PreviewState::Stopping)
        status.state = PreviewState::Stopping;
    else if (wanted == 0 || x.renderGeneration.load() == 0 ||
             x.completedGeneration.load() == wanted || x.blockedThroughGeneration.load() >= wanted)
        status.state = PreviewState::Idle;
    else if (x.playingGeneration.load() == wanted && x.audioState.load() == PreviewState::Playing)
        status.state = PreviewState::Playing;
    else if (impl_->readyGeneration == wanted) {
        status.state = PreviewState::WaitingForAudio;
        status.totalFrames = impl_->readyFrames;
        status.positionFrames = 0;
    } else
        status.state = PreviewState::Preparing;
    return status;
}
PreviewProcessResult EffectPreviewService::process(std::span<float *> channels, std::size_t frames,
                                                   bool blocked) noexcept {
    return impl_->player.process(channels, frames, blocked);
}
std::string effectPreviewCredits() {
    using namespace DarkVoicePreviewAssets;
    std::string credits(CREDITS_md, static_cast<std::size_t>(CREDITS_mdSize));
    credits.append("\n\n").append(CMU_ARCTIC_COPYING_txt,
                                  static_cast<std::size_t>(CMU_ARCTIC_COPYING_txtSize));
    credits.append("\n\n").append(VOCALSET_ATTRIBUTION_md,
                                  static_cast<std::size_t>(VOCALSET_ATTRIBUTION_mdSize));
    credits.append("\n\n").append(VOCALSET_LICENSE_txt,
                                  static_cast<std::size_t>(VOCALSET_LICENSE_txtSize));
    return credits;
}
} // namespace darkvoice
