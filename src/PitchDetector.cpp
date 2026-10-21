#include <algorithm>
#include <cmath>
#include <darkvoice/PitchDetector.hpp>

namespace darkvoice {
bool PitchDetector::prepare(double rate, std::size_t window, double minimumHz,
                            double maximumHz) noexcept {
    rate_ = 0.0;
    if ((rate != 44100.0 && rate != 48000.0 && rate != 88200.0 && rate != 96000.0) ||
        window < 1024 || window > 16384 || !std::isfinite(minimumHz) || !std::isfinite(maximumHz) ||
        minimumHz < 20.0 || maximumHz > 4000.0 || minimumHz >= maximumHz)
        return false;
    const auto maximumLag = static_cast<std::size_t>(std::ceil(rate / minimumHz));
    if (window < 2 * maximumLag + 2)
        return false;
    try {
        difference_.assign(maximumLag + 2, 0.0);
    } catch (...) {
        return false;
    }
    rate_ = rate;
    window_ = window;
    minimum_ = minimumHz;
    maximum_ = maximumHz;
    minimumLag_ = static_cast<std::size_t>(std::floor(rate / maximumHz));
    maximumLag_ = maximumLag;
    return true;
}
PitchEstimate PitchDetector::analyze(std::span<const float> samples) noexcept {
    if (rate_ == 0.0 || samples.size() != window_)
        return {};
    double mean = 0.0;
    for (float value : samples) {
        if (!std::isfinite(value))
            return {};
        mean += value;
    }
    mean /= static_cast<double>(window_);
    double energy = 0.0;
    for (float value : samples) {
        const double centered = value - mean;
        energy += centered * centered;
    }
    if (energy / static_cast<double>(window_) < 1.0e-10)
        return {};
    const std::size_t comparisons = window_ - maximumLag_ - 1;
    double cumulative = 0.0;
    difference_[0] = 1.0;
    for (std::size_t lag = 1; lag <= maximumLag_ + 1; ++lag) {
        double difference = 0.0;
        for (std::size_t i = 0; i < comparisons; ++i) {
            const double delta = static_cast<double>(samples[i]) - samples[i + lag];
            difference += delta * delta;
        }
        cumulative += difference;
        difference_[lag] =
            cumulative > 0.0 ? difference * static_cast<double>(lag) / cumulative : 1.0;
    }
    std::size_t candidate = 0;
    for (std::size_t lag = minimumLag_; lag <= maximumLag_; ++lag) {
        if (difference_[lag] < 0.15) {
            while (lag < maximumLag_ && difference_[lag + 1] < difference_[lag])
                ++lag;
            candidate = lag;
            break;
        }
    }
    if (candidate == 0)
        return {};
    const double left = difference_[candidate - 1], middle = difference_[candidate],
                 right = difference_[candidate + 1];
    const double curvature = left - 2.0 * middle + right;
    const double refinement =
        curvature > 1.0e-15 ? std::clamp(0.5 * (left - right) / curvature, -1.0, 1.0) : 0.0;
    double frequency = rate_ / (static_cast<double>(candidate) + refinement);
    const double confidence = std::clamp(1.0 - middle, 0.0, 1.0);
    // Allow small parabolic interpolation bias at configured endpoints (under five cents).
    if (!std::isfinite(frequency) || confidence < 0.8 || frequency < minimum_ * 0.9975 ||
        frequency > maximum_ * 1.0025)
        return {};
    frequency = std::clamp(frequency, minimum_, maximum_);
    return {frequency, 69.0 + 12.0 * std::log2(frequency / 440.0), static_cast<float>(confidence),
            true};
}
} // namespace darkvoice
