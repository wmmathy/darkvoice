#include "DryPitchAnalyzer.hpp"
#include <algorithm>
#include <cmath>
namespace darkvoice {
static_assert(std::atomic<int>::is_always_lock_free && std::atomic<float>::is_always_lock_free &&
              std::atomic<bool>::is_always_lock_free);
DryPitchAnalyzer::DryPitchAnalyzer() : juce::Thread("DarkVoice dry pitch") {}
DryPitchAnalyzer::~DryPitchAnalyzer() {
    release();
}
bool DryPitchAnalyzer::prepare(double rate) {
    release();
    fifo_.reset();
    discontinuity_.store(false, std::memory_order_relaxed);
    if (!detector_.prepare(rate, windowSize))
        return false;
    ready_ = startThread();
    return ready_;
}
void DryPitchAnalyzer::release() {
    ready_ = false;
    stopThread(-1); // Non-real-time lifecycle only; analysis work is algorithmically bounded.
    frequency_.store(0, std::memory_order_relaxed);
    confidence_.store(0, std::memory_order_relaxed);
}
void DryPitchAnalyzer::push(const juce::AudioBuffer<float> &input) noexcept {
    if (!ready_ || input.getNumChannels() < 1 || input.getNumChannels() > 2)
        return;
    const int count = input.getNumSamples();
    if (count == 0)
        return;
    if (count > fifo_.getFreeSpace()) {
        discontinuity_.store(true, std::memory_order_release);
        return;
    }
    int start1, count1, start2, count2;
    fifo_.prepareToWrite(count, start1, count1, start2, count2);
    const auto *left = input.getReadPointer(0);
    const auto *right = input.getNumChannels() == 2 ? input.getReadPointer(1) : left;
    const auto copy = [&](int start, int amount, int offset) {
        for (int i = 0; i < amount; ++i) {
            const float a = std::isfinite(left[offset + i]) ? left[offset + i] : 0.0f;
            const float b = std::isfinite(right[offset + i]) ? right[offset + i] : 0.0f;
            ring_[static_cast<std::size_t>(start + i)] = 0.5f * a + 0.5f * b;
        }
    };
    copy(start1, count1, 0);
    copy(start2, count2, count1);
    fifo_.finishedWrite(count1 + count2);
    // No event notification here: JUCE WaitableEvent::signal can acquire an OS mutex.
    // The worker polls a short timed wait instead.
}
void DryPitchAnalyzer::run() {
    int write = 0, filled = 0, sinceAnalysis = 0;
    while (!threadShouldExit()) {
        if (discontinuity_.exchange(false, std::memory_order_acq_rel)) {
            int s1, n1, s2, n2;
            fifo_.prepareToRead(fifo_.getNumReady(), s1, n1, s2, n2);
            fifo_.finishedRead(n1 + n2);
            filled = sinceAnalysis = write = 0;
            frequency_.store(0, std::memory_order_relaxed);
            confidence_.store(0, std::memory_order_relaxed);
        }
        const int available = std::min(2048, fifo_.getNumReady());
        if (available == 0) {
            wait(5);
            continue;
        }
        int s1, n1, s2, n2;
        fifo_.prepareToRead(available, s1, n1, s2, n2);
        const auto consume = [&](int start, int amount) {
            for (int i = 0; i < amount; ++i) {
                history_[static_cast<std::size_t>(write)] =
                    ring_[static_cast<std::size_t>(start + i)];
                write = (write + 1) % windowSize;
                filled = std::min(filled + 1, windowSize);
                ++sinceAnalysis;
            }
        };
        consume(s1, n1);
        consume(s2, n2);
        fifo_.finishedRead(n1 + n2);
        if (filled == windowSize && sinceAnalysis >= hopSize) {
            for (int i = 0; i < windowSize; ++i)
                analysis_[static_cast<std::size_t>(i)] =
                    history_[static_cast<std::size_t>((write + i) % windowSize)];
            const auto pitch = detector_.analyze(analysis_);
            frequency_.store(pitch.voiced ? static_cast<float>(pitch.frequencyHz) : 0.0f,
                             std::memory_order_relaxed);
            confidence_.store(pitch.voiced ? pitch.confidence : 0.0f, std::memory_order_relaxed);
            sinceAnalysis = 0;
        }
    }
}
} // namespace darkvoice
