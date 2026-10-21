#include "DryPitchAnalyzer.hpp"
#include "PluginProcessor.hpp"
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
#include <thread>
namespace {
thread_local bool probe = false;
thread_local std::size_t callbackAllocations = 0;
int checks = 0, failures = 0;
void check(bool value, const char *message) {
    ++checks;
    if (!value) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}
template <class Predicate> bool waitFor(Predicate condition) {
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < until) {
        if (condition())
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return condition();
}
void tone(juce::AudioBuffer<float> &buffer, double rate, float amplitude = 0.2f, int offset = 0,
          bool cancel = false) {
    for (int c = 0; c < buffer.getNumChannels(); ++c)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            buffer.setSample(c, i,
                             amplitude *
                                 static_cast<float>(std::sin(2 * juce::MathConstants<double>::pi *
                                                             110 * (offset + i) / rate)) *
                                 (cancel && c == 1 ? -1 : 1));
}
} // namespace
#if defined(__GNUC__) || defined(__clang__)
#define WORKER_NOINLINE __attribute__((noinline))
#else
#define WORKER_NOINLINE
#endif
WORKER_NOINLINE void *operator new(std::size_t bytes) {
    if (probe)
        ++callbackAllocations;
    if (auto *p = std::malloc(bytes ? bytes : 1))
        return p;
    throw std::bad_alloc();
}
WORKER_NOINLINE void *operator new[](std::size_t bytes) {
    return ::operator new(bytes);
}
WORKER_NOINLINE void operator delete(void *p) noexcept {
    std::free(p);
}
WORKER_NOINLINE void operator delete[](void *p) noexcept {
    std::free(p);
}
WORKER_NOINLINE void operator delete(void *p, std::size_t) noexcept {
    std::free(p);
}
WORKER_NOINLINE void operator delete[](void *p, std::size_t) noexcept {
    std::free(p);
}
int main() {
    juce::ScopedJuceInitialiser_GUI runtime;
    darkvoice::DryPitchAnalyzer worker;
    juce::AudioBuffer<float> mono(1, 8192);
    tone(mono, 48000);
    check(worker.prepare(48000), "worker starts at supported rate");
    const auto sample = mono.getSample(0, 37);
    callbackAllocations = 0;
    probe = true;
    worker.push(mono);
    probe = false;
    check(callbackAllocations == 0, "producer has no ordinary allocations");
    check(mono.getSample(0, 37) == sample, "analysis leaves input unchanged");
    check(waitFor([&] {
              return std::abs(worker.frequencyHz() - 110) < 1 && worker.confidence() >= .8f;
          }),
          "actual worker detects bass note");
    juce::AudioBuffer<float> overflow(1, 33000);
    overflow.clear();
    worker.push(overflow);
    check(waitFor([&] { return worker.frequencyHz() == 0 && worker.confidence() == 0; }),
          "overflow invalidates old estimate");
    worker.release();
    check(worker.frequencyHz() == 0, "release clears telemetry");
    check(!worker.prepare(32000), "unsupported rate does not start worker");
    check(worker.prepare(96000), "rate change restarts worker");
    juce::AudioBuffer<float> stereo(2, 8192);
    tone(stereo, 96000);
    worker.push(stereo);
    check(waitFor([&] { return std::abs(worker.frequencyHz() - 110) < 1; }),
          "stereo in-phase detects pitch");
    tone(stereo, 96000, .2f, 0, true);
    worker.push(stereo);
    check(waitFor([&] { return worker.frequencyHz() == 0; }),
          "opposite-phase mono average is unvoiced");
    worker.release();
    DarkVoiceProcessor processor;
    for (const auto *id : {"inputDb", "outputDb"}) {
        auto *parameter = processor.parameters.getParameter(id);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(-24));
    }
    processor.prepareToPlay(48000, 512);
    juce::AudioBuffer<float> audio(2, 512);
    juce::MidiBuffer midi;
    callbackAllocations = 0;
    for (int block = 0; block < 24; ++block) {
        tone(audio, 48000, .00005f, block * 512);
        probe = true;
        processor.processBlock(audio, midi);
        probe = false;
    }
    check(callbackAllocations == 0,
          "full audio callback has no ordinary allocations on producer thread");
    check(audio.getMagnitude(0, audio.getNumSamples()) < .000001f,
          "output below detector voicing threshold");
    check(waitFor([&] {
              return std::abs(processor.dryPitchHz() - 110) < 1 &&
                     processor.dryPitchConfidence() >= .8f;
          }),
          "processor analyzes dry input before attenuation");
    processor.releaseResources();
    check(processor.dryPitchHz() == 0, "processor release stops telemetry");
    std::cout << "Dry worker checks: " << checks << ", failures: " << failures << '\n';
    return failures ? 1 : 0;
}
