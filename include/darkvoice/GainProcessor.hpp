#pragma once

#include <cmath>
#include <cstddef>
#include <span>

namespace darkvoice {

struct ProcessSpec {
    double sampleRate;
    std::size_t maxBlockSize;
    std::size_t channels;
};

class GainProcessor {
public:
    [[nodiscard]] bool prepare(ProcessSpec spec) noexcept {
        prepared_ = false;
        if (!std::isfinite(spec.sampleRate) || spec.sampleRate <= 0.0 ||
            spec.maxBlockSize == 0 || (spec.channels != 1 && spec.channels != 2)) {
            return false;
        }
        spec_ = spec;
        gain_ = 1.0F;
        prepared_ = true;
        return true;
    }

    [[nodiscard]] bool setGain(float gain) noexcept {
        if (!std::isfinite(gain) || gain < 0.0F || gain > 4.0F) {
            return false;
        }
        gain_ = gain;
        return true;
    }

    [[nodiscard]] bool process(std::span<float*> channels, std::size_t frames) noexcept {
        if (!prepared_ || channels.size() != spec_.channels || frames > spec_.maxBlockSize) {
            return false;
        }
        // Validate the complete block before modifying any channel.
        for (std::size_t channel = 0; channel < channels.size(); ++channel) {
            if (frames != 0 && channels[channel] == nullptr) {
                return false;
            }
            for (std::size_t previous = 0; previous < channel; ++previous) {
                if (channels[channel] == channels[previous]) {
                    return false;
                }
            }
        }
        for (float* channel : channels) {
            for (std::size_t frame = 0; frame < frames; ++frame) {
                channel[frame] *= gain_;
            }
        }
        return true;
    }

    void reset() noexcept { gain_ = 1.0F; }

    [[nodiscard]] std::size_t latencySamples() const noexcept { return 0; }

private:
    ProcessSpec spec_{};
    float gain_{1.0F};
    bool prepared_{false};
};

} // namespace darkvoice
