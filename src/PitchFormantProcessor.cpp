#include <algorithm>
#include <array>
#include <cmath>
#include <darkvoice/PitchFormantProcessor.hpp>
#include <limits>
#include <signalsmith-stretch.h>
#include <vector>
namespace darkvoice {
namespace {
float sample(double value) noexcept {
    constexpr double maximum = std::numeric_limits<float>::max();
    if (!std::isfinite(value) || std::abs(value) < std::numeric_limits<float>::min())
        return 0;
    return static_cast<float>(std::clamp(value, -maximum, maximum));
}
bool range(float value, float low, float high) noexcept {
    return std::isfinite(value) && value >= low && value <= high;
}
} // namespace
struct PitchFormantProcessor::Impl {
    signalsmith::stretch::SignalsmithStretch<float> backend{0};
    PitchFormantSpec spec;
    PitchFormantParameters current{};
    std::array<std::vector<float>, 2> input, output, dry;
    std::array<std::array<float, 64>, 2> delayed{};
    std::size_t latency = 0, block = 0, position = 0, controlOffset = 0;
    double smoothing = 0, blendSmoothing = 0, blend = 0;
    void update(const PitchFormantParameters &target) noexcept {
        const auto smooth = [this](float &value, float goal) {
            value = static_cast<float>(value + smoothing * (goal - value));
            if (std::abs(value - goal) < 1e-4f)
                value = goal;
        };
        smooth(current.pitchSemitones, target.pitchSemitones);
        smooth(current.formantSemitones, target.formantSemitones);
        if (target.formantBaseHz == 0 || current.formantBaseHz == 0)
            current.formantBaseHz = target.formantBaseHz;
        else
            smooth(current.formantBaseHz, target.formantBaseHz);
        current.preserveFormants = target.preserveFormants;
        backend.setTransposeSemitones(current.pitchSemitones);
        backend.setFormantSemitones(current.formantSemitones, current.preserveFormants);
        backend.setFormantBase(current.formantBaseHz / static_cast<float>(spec.sampleRate));
    }
};
PitchFormantProcessor::PitchFormantProcessor() noexcept = default;
PitchFormantProcessor::~PitchFormantProcessor() = default;
bool PitchFormantProcessor::valid(const PitchFormantParameters &p) noexcept {
    return range(p.pitchSemitones, -12, 12) && range(p.formantSemitones, -12, 12) &&
           (p.formantBaseHz == 0 || range(p.formantBaseHz, 40, 1000));
}
bool PitchFormantProcessor::prepare(const PitchFormantSpec &s) noexcept {
    impl_.reset();
    if ((s.sampleRate != 44100 && s.sampleRate != 48000 && s.sampleRate != 88200 &&
         s.sampleRate != 96000) ||
        s.maxFrames == 0 || s.maxFrames > 65536 || (s.channels != 1 && s.channels != 2) ||
        (s.quality != PitchQuality::Live && s.quality != PitchQuality::Studio &&
         s.quality != PitchQuality::Offline))
        return false;
    try {
        auto next = std::make_unique<Impl>();
        next->spec = s;
        next->block = (s.quality == PitchQuality::Live     ? 1024
                       : s.quality == PitchQuality::Studio ? 4096
                                                           : 8192) *
                      (s.sampleRate > 48000 ? 2 : 1);
        next->backend.configure(static_cast<int>(s.channels), static_cast<int>(next->block),
                                static_cast<int>(next->block / 8), true);
        next->latency =
            static_cast<std::size_t>(next->backend.inputLatency() + next->backend.outputLatency());
        if (next->latency == 0)
            return false;
        for (std::size_t c = 0; c < s.channels; ++c) {
            next->input[c].resize(64);
            next->output[c].resize(64);
            next->dry[c].assign(next->latency, 0);
        }
        next->smoothing = 1 - std::exp(-64 / (.010 * s.sampleRate));
        next->blendSmoothing = 1 - std::exp(-1 / (.020 * s.sampleRate));
        impl_ = std::move(next);
        target_ = {};
        reset();
        return true;
    } catch (...) {
        return false;
    }
}
bool PitchFormantProcessor::setParameters(const PitchFormantParameters &p) noexcept {
    if (!valid(p))
        return false;
    target_ = p;
    return true;
}
void PitchFormantProcessor::reset() noexcept {
    if (!impl_)
        return;
    auto &p = *impl_;
    p.backend.reset();
    p.current = target_;
    p.update(target_);
    p.position = p.controlOffset = 0;
    p.blend = (target_.pitchSemitones != 0 || target_.formantSemitones != 0) ? 1 : 0;
    for (auto &ring : p.dry)
        std::fill(ring.begin(), ring.end(), 0);
}
std::size_t PitchFormantProcessor::latencySamples() const noexcept {
    return impl_ ? impl_->latency : 0;
}
std::size_t PitchFormantProcessor::tailSamples() const noexcept {
    return impl_ ? impl_->latency + impl_->block : 0;
}
bool PitchFormantProcessor::process(std::span<float *> channels, std::size_t frames) noexcept {
    if (!impl_ || channels.size() != impl_->spec.channels || frames > impl_->spec.maxFrames)
        return false;
    for (std::size_t c = 0; c < channels.size(); ++c) {
        if (frames && channels[c] == nullptr)
            return false;
        for (std::size_t prior = 0; prior < c; ++prior)
            if (channels[c] == channels[prior])
                return false;
    }
    auto &p = *impl_;
    std::size_t offset = 0;
    while (offset < frames) {
        if (p.controlOffset == 0)
            p.update(target_);
        const auto count = std::min(frames - offset, 64 - p.controlOffset);
        for (std::size_t i = 0; i < count; ++i) {
            for (std::size_t c = 0; c < channels.size(); ++c) {
                const auto input = sample(channels[c][offset + i]);
                p.delayed[c][i] = p.dry[c][p.position];
                p.dry[c][p.position] = input;
                p.input[c][i] = std::clamp(input, -8.f, 8.f);
            }
            if (++p.position == p.latency)
                p.position = 0;
        }
        std::array<float *, 2> in{p.input[0].data(), p.input[1].data()},
            out{p.output[0].data(), p.output[1].data()};
        p.backend.process(in, static_cast<int>(count), out, static_cast<int>(count));
        const double goal =
            (p.current.pitchSemitones != 0 || p.current.formantSemitones != 0) ? 1 : 0;
        for (std::size_t i = 0; i < count; ++i) {
            p.blend += p.blendSmoothing * (goal - p.blend);
            if (std::abs(p.blend - goal) < 1e-6)
                p.blend = goal;
            for (std::size_t c = 0; c < channels.size(); ++c) {
                const auto dry = p.delayed[c][i];
                channels[c][offset + i] =
                    p.blend == 0 ? dry
                                 : sample(dry * (1 - p.blend) + sample(p.output[c][i]) * p.blend);
            }
        }
        offset += count;
        p.controlOffset = (p.controlOffset + count) % 64;
    }
    return true;
}
} // namespace darkvoice
