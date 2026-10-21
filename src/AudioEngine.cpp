#include <darkvoice/AudioEngine.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace darkvoice {
namespace {
float bounded(double value) noexcept {
    constexpr double limit = std::numeric_limits<float>::max();
    if (std::abs(value) < 1.0e-20) return 0.0f;
    return static_cast<float>(std::clamp(value, -limit, limit));
}
bool inRange(float value, float minimum, float maximum) noexcept {
    return std::isfinite(value) && value >= minimum && value <= maximum;
}
}

bool AudioEngine::prepare(double sampleRate, std::size_t maxFrames,
                          std::size_t channels) noexcept {
    rate_ = 0.0;
    if ((sampleRate != 44100.0 && sampleRate != 48000.0 &&
         sampleRate != 88200.0 && sampleRate != 96000.0) ||
        maxFrames == 0 || (channels != 1 && channels != 2)) return false;
    rate_ = sampleRate;
    maxFrames_ = maxFrames;
    channels_ = channels;
    target_ = {};
    reset();
    return true;
}

bool AudioEngine::setParameters(const EngineParameters& p) noexcept {
    if (!inRange(p.inputDb, -24.0f, 24.0f) ||
        !inRange(p.outputDb, -24.0f, 24.0f) ||
        !inRange(p.highPassHz, 20.0f, 400.0f) ||
        !inRange(p.drive, 0.0f, 1.0f) ||
        !inRange(p.thresholdDb, -60.0f, 0.0f) ||
        !inRange(p.ratio, 1.0f, 20.0f) || !inRange(p.mix, 0.0f, 1.0f)) return false;
    target_ = p;
    return true;
}

void AudioEngine::reset() noexcept {
    previousInput_.fill(0.0f);
    previousOutput_.fill(0.0f);
    envelope_.fill(0.0f);
    meters_ = {};
    current_ = target_;
}

bool AudioEngine::process(std::span<float*> channels, std::size_t frames) noexcept {
    if (rate_ == 0.0 || channels.size() != channels_ || frames > maxFrames_) return false;
    for (std::size_t c = 0; c < channels.size(); ++c) {
        if (frames != 0 && channels[c] == nullptr) return false;
        for (std::size_t prior = 0; prior < c; ++prior)
            if (channels[c] == channels[prior]) return false;
    }
    meters_ = {};
    if (target_.bypass) {
        for (float* channel : channels)
            for (std::size_t i = 0; i < frames; ++i)
                if (std::isfinite(channel[i]))
                    meters_.inputPeak = std::max(meters_.inputPeak, std::abs(channel[i]));
        meters_.outputPeak = meters_.inputPeak;
        return true;
    }
    const double smoothing = 1.0 - std::exp(-1.0 / (0.010 * rate_));
    const double attack = std::exp(-1.0 / (0.010 * rate_));
    const double release = std::exp(-1.0 / (0.100 * rate_));
    const auto smooth = [smoothing](float& value, float target) noexcept {
        value = static_cast<float>(value + smoothing * (target - value));
    };
    for (std::size_t i = 0; i < frames; ++i) {
        smooth(current_.inputDb, target_.inputDb);
        smooth(current_.outputDb, target_.outputDb);
        smooth(current_.highPassHz, target_.highPassHz);
        smooth(current_.drive, target_.drive);
        smooth(current_.thresholdDb, target_.thresholdDb);
        smooth(current_.ratio, target_.ratio);
        smooth(current_.mix, target_.mix);
        const double inputGain = std::pow(10.0, current_.inputDb / 20.0);
        const double outputGain = std::pow(10.0, current_.outputDb / 20.0);
        const double hp = std::exp(-2.0 * std::numbers::pi * current_.highPassHz / rate_);
        const double threshold = std::pow(10.0, current_.thresholdDb / 20.0);
        const double drive = 10.0 * current_.drive;
        for (std::size_t c = 0; c < channels.size(); ++c) {
            const float dry = std::isfinite(channels[c][i]) ? channels[c][i] : 0.0f;
            meters_.inputPeak = std::max(meters_.inputPeak, std::abs(dry));
            const float gained = bounded(dry * inputGain);
            float wet = bounded(hp * (static_cast<double>(previousOutput_[c]) + gained -
                                      previousInput_[c]));
            previousInput_[c] = gained;
            previousOutput_[c] = wet;
            if (drive > 1.0e-6) wet = bounded(std::tanh(drive * wet) / std::tanh(drive));
            const double magnitude = std::abs(wet);
            const double coefficient = magnitude > envelope_[c] ? attack : release;
            envelope_[c] = bounded(coefficient * envelope_[c] + (1.0 - coefficient) * magnitude);
            if (envelope_[c] > threshold)
                wet = bounded(wet * std::pow(envelope_[c] / threshold,
                                             1.0 / current_.ratio - 1.0));
            const float output = bounded((static_cast<double>(dry) * (1.0 - current_.mix) +
                                          static_cast<double>(wet) * current_.mix) * outputGain);
            channels[c][i] = output;
            meters_.outputPeak = std::max(meters_.outputPeak, std::abs(output));
        }
    }
    return true;
}
}
