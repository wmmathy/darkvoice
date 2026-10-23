#include <darkvoice/AudioEngine.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace darkvoice {
namespace {
float bounded(double value) noexcept {
    constexpr double limit = std::numeric_limits<float>::max();
    if (std::abs(value) < 1.0e-20)
        return 0.0f;
    return static_cast<float>(std::clamp(value, -limit, limit));
}
bool inRange(float value, float minimum, float maximum) noexcept {
    return std::isfinite(value) && value >= minimum && value <= maximum;
}
ToneEffectsParameters toneParameters(const EngineParameters &p) noexcept {
    return {p.eqLowHz,    p.eqLowGainDb,   p.eqMidHz,  p.eqMidGainDb,   p.eqMidQ,
            p.eqHighHz,   p.eqHighGainDb,  p.delayMs,  p.delayFeedback, p.delayMix,
            p.reverbSize, p.reverbDamping, p.reverbMix};
}
StereoDelayParameters stereoDelayParameters(const EngineParameters &p) noexcept {
    StereoDelayParameters result;
    result.timeMs = p.stereoDelayMs;
    result.feedback = p.stereoDelayFeedback;
    result.mix = p.stereoDelayMix;
    result.bpm = p.stereoDelayBpm;
    result.mode = p.stereoDelayMode;
    result.sync = p.stereoDelaySync;
    result.division = p.stereoDelayDivision;
    return result;
}

} // namespace

bool AudioEngine::prepare(double sampleRate, std::size_t maxFrames, std::size_t channels) noexcept {
    rate_ = 0.0;
    maxFrames_ = channels_ = 0;
    if ((sampleRate != 44100.0 && sampleRate != 48000.0 && sampleRate != 88200.0 &&
         sampleRate != 96000.0) ||
        maxFrames == 0 || maxFrames > 65536 || (channels != 1 && channels != 2))
        return false;
    // Preparation is explicitly non-real-time. All storage required by process
    // is allocated here; failed allocation leaves the engine unprepared.
    try {
        for (std::size_t c = 0; c < channels; ++c)
            wet_[c].resize(maxFrames);
    } catch (...) {
        return false;
    }
    if (!effects_.prepare(sampleRate, maxFrames, channels) ||
        !gate_.prepare(sampleRate, maxFrames, channels) ||
        !deEsser_.prepare(sampleRate, maxFrames, channels) ||
        !modulation_.prepare(sampleRate, maxFrames, channels) ||
        !flanger_.prepare(sampleRate, maxFrames, channels) ||
        !stereoDelay_.prepare(sampleRate, maxFrames, channels) ||
        !limiter_.prepare(sampleRate, maxFrames, channels))
        return false;
    rate_ = sampleRate;
    maxFrames_ = maxFrames;
    channels_ = channels;
    target_ = {};
    reset();
    return true;
}

bool AudioEngine::setParameters(const EngineParameters &p) noexcept {
    if (!inRange(p.inputDb, -24.0f, 24.0f) || !inRange(p.outputDb, -24.0f, 24.0f) ||
        !inRange(p.highPassHz, 20.0f, 400.0f) || !inRange(p.drive, 0.0f, 1.0f) ||
        !inRange(p.thresholdDb, -60.0f, 0.0f) || !inRange(p.ratio, 1.0f, 20.0f) ||
        !inRange(p.mix, 0.0f, 1.0f) || !ToneEffects::valid(toneParameters(p)) ||
        !NoiseGate::valid({p.gateThresholdDb, p.gateRatio, p.gateReleaseMs}) ||
        !DeEsser::valid({p.deEssHz, p.deEssThresholdDb, p.deEssReductionDb}) ||
        !ModulationEffects::valid({p.chorusRateHz, p.chorusDepthMs, p.chorusMix, p.stereoWidth}) ||
        !Flanger::valid({p.flangerRateHz, p.flangerDepthMs, p.flangerFeedback, p.flangerMix}) ||
        !VocalLimiter::valid({p.limiterCeilingDb, p.limiterReleaseMs}) ||
        !StereoDelay::valid(stereoDelayParameters(p)))
        return false;
    // All snapshots are fully validated before any target is replaced.
    if (!effects_.setParameters(toneParameters(p)))
        return false;
    (void)gate_.setParameters({p.gateThresholdDb, p.gateRatio, p.gateReleaseMs});
    (void)deEsser_.setParameters({p.deEssHz, p.deEssThresholdDb, p.deEssReductionDb});
    (void)modulation_.setParameters({p.chorusRateHz, p.chorusDepthMs, p.chorusMix, p.stereoWidth});
    (void)flanger_.setParameters(
        {p.flangerRateHz, p.flangerDepthMs, p.flangerFeedback, p.flangerMix});
    (void)stereoDelay_.setParameters(stereoDelayParameters(p));
    (void)limiter_.setParameters({p.limiterCeilingDb, p.limiterReleaseMs});
    if (p.limiterEnabled != target_.limiterEnabled)
        limiter_.reset();
    target_ = p;
    return true;
}

void AudioEngine::reset() noexcept {
    previousInput_.fill(0.0f);
    previousOutput_.fill(0.0f);
    envelope_.fill(0.0f);
    meters_ = {};
    current_ = target_;
    effects_.reset();
    gate_.reset();
    deEsser_.reset();
    modulation_.reset();
    flanger_.reset();
    stereoDelay_.reset();
    limiter_.reset();
}

bool AudioEngine::process(std::span<float *> channels, std::size_t frames) noexcept {
    if (rate_ == 0.0 || channels.size() != channels_ || frames > maxFrames_)
        return false;
    for (std::size_t c = 0; c < channels.size(); ++c) {
        if (frames != 0 && channels[c] == nullptr)
            return false;
        for (std::size_t prior = 0; prior < c; ++prior)
            if (channels[c] == channels[prior])
                return false;
    }
    meters_ = {};
    if (target_.bypass) {
        for (float *channel : channels)
            for (std::size_t i = 0; i < frames; ++i)
                if (std::isfinite(channel[i]))
                    meters_.inputPeak = std::max(meters_.inputPeak, std::abs(channel[i]));
        meters_.outputPeak = meters_.inputPeak;
        return true;
    }
    const double smoothing = 1.0 - std::exp(-1.0 / (0.010 * rate_));
    const double attack = std::exp(-1.0 / (0.010 * rate_));
    const double release = std::exp(-1.0 / (0.100 * rate_));
    const auto smooth = [smoothing](float &value, float target) noexcept {
        value = static_cast<float>(value + smoothing * (target - value));
    };
    for (std::size_t i = 0; i < frames; ++i) {
        smooth(current_.inputDb, target_.inputDb);
        smooth(current_.highPassHz, target_.highPassHz);
        smooth(current_.drive, target_.drive);
        smooth(current_.thresholdDb, target_.thresholdDb);
        smooth(current_.ratio, target_.ratio);
        const double inputGain = std::pow(10.0, current_.inputDb / 20.0);
        const double hp = std::exp(-2.0 * std::numbers::pi * current_.highPassHz / rate_);
        const double threshold = std::pow(10.0, current_.thresholdDb / 20.0);
        const double drive = 10.0 * current_.drive;
        for (std::size_t c = 0; c < channels.size(); ++c) {
            const float dry = std::isfinite(channels[c][i]) ? channels[c][i] : 0.0f;
            meters_.inputPeak = std::max(meters_.inputPeak, std::abs(dry));
            const float gained = bounded(dry * inputGain);
            float wet = bounded(
                hp * (static_cast<double>(previousOutput_[c]) + gained - previousInput_[c]));
            previousInput_[c] = gained;
            previousOutput_[c] = wet;
            if (drive > 1.0e-6)
                wet = bounded(std::tanh(drive * wet) / std::tanh(drive));
            const double magnitude = std::abs(wet);
            const double coefficient = magnitude > envelope_[c] ? attack : release;
            envelope_[c] = bounded(coefficient * envelope_[c] + (1.0 - coefficient) * magnitude);
            if (envelope_[c] > threshold)
                wet = bounded(wet * std::pow(envelope_[c] / threshold, 1.0 / current_.ratio - 1.0));
            wet_[c][i] = wet;
        }
    }
    std::array<float *, 2> wetChannels{wet_[0].data(), wet_[1].data()};
    if (!gate_.process(std::span<float *>(wetChannels.data(), channels_), frames) ||
        !deEsser_.process(std::span<float *>(wetChannels.data(), channels_), frames) ||
        !effects_.process(std::span<float *>(wetChannels.data(), channels_), frames) ||
        !modulation_.process(std::span<float *>(wetChannels.data(), channels_), frames) ||
        !flanger_.process(std::span<float *>(wetChannels.data(), channels_), frames) ||
        !stereoDelay_.process(std::span<float *>(wetChannels.data(), channels_), frames))
        return false; // Prepared capacities/layout have already been validated.
    for (std::size_t i = 0; i < frames; ++i) {
        smooth(current_.outputDb, target_.outputDb);
        smooth(current_.mix, target_.mix);
        const double outputGain = std::pow(10.0, current_.outputDb / 20.0);
        for (std::size_t c = 0; c < channels.size(); ++c) {
            const float dry = std::isfinite(channels[c][i]) ? channels[c][i] : 0.0f;
            const float output = bounded((static_cast<double>(dry) * (1.0 - current_.mix) +
                                          static_cast<double>(wet_[c][i]) * current_.mix) *
                                         outputGain);
            channels[c][i] = output;
        }
    }
    // The limiter protects the final blend and output gain, including dry-only
    // monitoring. Global bypass returned above before any limiter processing.
    if (target_.limiterEnabled && !limiter_.process(channels, frames))
        return false;
    for (float *channel : channels)
        for (std::size_t i = 0; i < frames; ++i)
            meters_.outputPeak = std::max(meters_.outputPeak, std::abs(channel[i]));
    return true;
}
} // namespace darkvoice
