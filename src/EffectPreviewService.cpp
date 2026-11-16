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
        exchange.requestedGeneration.store(0);
        exchange.renderGeneration.store(0);
        {
            std::lock_guard lock(mutex);
            quitting = true;
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
    if (!impl_->prepared || (definition->stereoDemonstration && impl_->format.channels != 2))
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
    return R"DVCREDITS(# Preview recording credits and provenance

These are natural human recordings, not generated male/female voice conversions.
Published source metadata supplies the male/female labels. Spoken material is
labelled speech; the VocalSet material is sustained sung /a/, not spoken words.
The four original downloaded WAV files are retained byte for byte. Suggested
playback excerpts are metadata only; the runtime renderer may excerpt, apply
shared edge fades and process effects. It does not change these original files.

## English spoken samples — CMU ARCTIC

`male_speech.wav`: CMU ARCTIC BDL, published male speaker.
`female_speech.wav`: CMU ARCTIC SLT, published female speaker.

Copyright (c) 2003 Carnegie Mellon University. The recordings are supplied under
the permissive CMU ARCTIC voice licence, permitting commercial and other uses
subject to preserving its copyright, conditions, original authors and disclaimer,
and clearly marking modifications. The complete publisher notice is retained
unaltered in [CMU_ARCTIC_COPYING.txt](licenses/CMU_ARCTIC_COPYING.txt). Its English
section applies to these two files; its separate FLEURS section concerns other
recordings that are NOT bundled here. This is not an assertion that the audio
uses the mirror application's code licence.

Original source: http://www.festvox.org/cmu_arctic/
Suggested bibliographic credit: Kominek, J. and Black, A. W. (2004),
“The CMU Arctic speech databases.”

Pinned distribution mirror: JimLiu/baocut,
`bc57be6f9afde8d738e25d3c8438da0377efd041`,
`packages/models/assets/tts-voices/en-male.wav` and `en-female.wav`.
The publisher identifies each as original `arctic_a0001.wav` plus
`arctic_a0002.wav` from its BDL/SLT corpus. The publisher joined the two sentences
with 0.3 seconds silence, trimmed outer silence retaining 0.15 seconds padding,
and peak-normalized to -3.5 dBFS. It reports no speed or spoken-content change.
These prior modifications remain part of the attribution.

The suggested first-sentence excerpts end 150 ms inside the measured silent
separator: 54,077 frames for male and 56,002 for female at 16 kHz. The first
sentence is “Author of the danger trail, Philip Steels, etc.” Do not describe
these 16 kHz recordings as full-band 44.1/48 kHz capture after resampling.

## Singing samples — VocalSet

`male_singing.wav`: VocalSet singer m8, published male/bass,
original filename `m8_long_straight_a.wav`.
`female_singing.wav`: VocalSet singer f1, published female/soprano,
original filename `f1_long_straight_a.wav`.

Wilkins, J., Seetharaman, P., Wahl, A., & Pardo, B. (2018).
“VocalSet: A Singing Voice Dataset.” Proceedings of the 19th International
Society for Music Information Retrieval Conference (ISMIR 2018).

Recordings licensed Creative Commons Attribution 4.0 International (CC BY 4.0):
https://creativecommons.org/licenses/by/4.0/
Source record (v1.2): https://zenodo.org/records/1442513
Retain attribution, the licence link and an indication of excerpting, fades,
resampling or effects if material is adapted. The original recording notice,
singer metadata and mirror attribution are retained in `licenses/`.

Pinned distribution mirror: mblackman/vocalset-vowel-voicebanks,
`c48ab544e94e9788fec11be52b365026bb4a017c`, `source/vocalset/`.
These are full source-file recordings, NOT the mirror's cut, faded and
normalized 80 ms `voicebanks/` outputs. The mirror MIT code/arrangement licence
does not replace CC BY 4.0 for the recordings.

## Verification limits

The manifest records immutable source URLs, published labels, source/destination
SHA256, original PCM format and proposed cue bounds. No primary CMU archive byte
comparison was performed. Direct Zenodo and DataCite access was denied by the
research environment's network policy, so identity against the primary VocalSet
archive remains unverified. The pinned mirrors supply explicit recording
provenance and licence notices; no stronger primary-archive proof is claimed.
Objective PCM/hash/clipping checks are not manual listening qualification.

This voice is free for use for any purpose (commercial or otherwise)
subject to the pretty light restrictions detailed below.

                     Carnegie Mellon University
                         Copyright (c) 2003
                        All Rights Reserved.

  Permission to use, copy, modify,  and licence this software and its
  documentation for any purpose, is hereby granted without fee,
  subject to the following conditions:
   1. The code must retain the above copyright notice, this list of
      conditions and the following disclaimer.
   2. Any modifications must be clearly marked as such.
   3. Original authors' names are not deleted.

  THE AUTHORS OF THIS WORK DISCLAIM ALL WARRANTIES WITH REGARD TO
  THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
  AND FITNESS, IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
  SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
  THIS SOFTWARE.

VocalSet: A Singing Voice Dataset
=================================

The recordings in this directory are a subset of VocalSet (the "long straight"
sustained vowel tones), redistributed here under their original license.

License: Creative Commons Attribution 4.0 International (CC BY 4.0)
         https://creativecommons.org/licenses/by/4.0/

You are free to share and adapt the material for any purpose, including
commercially, provided you give appropriate credit, link to the license, and
indicate if changes were made. The clips in ../../voicebanks/ ARE changed (cut,
faded, and normalized — see make_voicebanks.py).

Please credit:

  Wilkins, J., Seetharaman, P., Wahl, A., & Pardo, B. (2018).
  VocalSet: A Singing Voice Dataset.
  Proceedings of the 19th International Society for Music Information
  Retrieval Conference (ISMIR 2018).

Source record (v1.2): https://zenodo.org/records/1442513

The original singer-voice-type notes distributed with the dataset are preserved
in README-vocalset.txt.
)DVCREDITS";
}
} // namespace darkvoice
