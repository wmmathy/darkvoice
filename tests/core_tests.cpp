#include "darkvoice/GainProcessor.hpp"

#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <span>
#include <string_view>

namespace allocation_probe {
std::atomic<std::size_t> count{0};
void *allocate(std::size_t size) {
    count.fetch_add(1, std::memory_order_relaxed);
    if (auto *memory = std::malloc(size == 0 ? 1 : size)) {
        return memory;
    }
    throw std::bad_alloc{};
}
void *allocateAligned(std::size_t size, std::size_t alignment) {
    count.fetch_add(1, std::memory_order_relaxed);
    size = size == 0 ? alignment : size;
    if (size > std::numeric_limits<std::size_t>::max() - (alignment - 1)) {
        throw std::bad_alloc{};
    }
    const auto rounded = (size + alignment - 1) / alignment * alignment;
    if (auto *memory = std::aligned_alloc(alignment, rounded)) {
        return memory;
    }
    throw std::bad_alloc{};
}
} // namespace allocation_probe

void *operator new(std::size_t size) {
    return allocation_probe::allocate(size);
}
void *operator new[](std::size_t size) {
    return allocation_probe::allocate(size);
}
void operator delete(void *memory) noexcept {
    std::free(memory);
}
void operator delete[](void *memory) noexcept {
    std::free(memory);
}
void operator delete(void *memory, std::size_t) noexcept {
    std::free(memory);
}
void operator delete[](void *memory, std::size_t) noexcept {
    std::free(memory);
}
void *operator new(std::size_t size, std::align_val_t alignment) {
    return allocation_probe::allocateAligned(size, static_cast<std::size_t>(alignment));
}
void *operator new[](std::size_t size, std::align_val_t alignment) {
    return allocation_probe::allocateAligned(size, static_cast<std::size_t>(alignment));
}
void operator delete(void *memory, std::align_val_t) noexcept {
    std::free(memory);
}
void operator delete[](void *memory, std::align_val_t) noexcept {
    std::free(memory);
}
void operator delete(void *memory, std::size_t, std::align_val_t) noexcept {
    std::free(memory);
}
void operator delete[](void *memory, std::size_t, std::align_val_t) noexcept {
    std::free(memory);
}

namespace {
int failures = 0;
int checks = 0;
void check(bool condition, std::string_view message) {
    ++checks;
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}
bool near(float actual, float expected) {
    return std::abs(actual - expected) <= 1.0e-6F;
}

void lifecycle() {
    darkvoice::GainProcessor processor;
    std::array<float, 2> data{1.0F, -1.0F};
    std::array<float *, 1> channels{data.data()};
    check(!processor.process(channels, 2), "unprepared processing rejects");
    check(data[0] == 1.0F && data[1] == -1.0F, "unprepared rejection preserves audio");
    check(processor.prepare({48000.0, 2, 1}), "valid mono preparation");
    check(processor.latencySamples() == 0, "zero latency");
    check(processor.setGain(0.25F), "set gain before reset");
    processor.reset();
    check(processor.process(channels, 2), "reset preserves preparation");
    check(data[0] == 1.0F && data[1] == -1.0F, "reset restores unity");
    const std::array<darkvoice::ProcessSpec, 8> invalid{
        {{0.0, 2, 1},
         {-1.0, 2, 1},
         {std::numeric_limits<double>::infinity(), 2, 1},
         {std::numeric_limits<double>::quiet_NaN(), 2, 1},
         {48000.0, 0, 1},
         {48000.0, 2, 0},
         {48000.0, 2, 3},
         {48000.0, 2, 99}}};
    for (const auto &spec : invalid) {
        check(processor.prepare({44100.0, 2, 1}), "reprepare before invalid spec");
        check(!processor.prepare(spec), "invalid preparation rejects");
        check(!processor.process(channels, 2), "invalid preparation unprepares");
        check(data[0] == 1.0F && data[1] == -1.0F, "invalid state preserves samples");
    }
    check(processor.prepare({96000.0, 4, 2}), "reprepare with stereo spec");
    std::array<float, 4> left{1, 2, 3, 4};
    std::array<float, 4> right{-1, -2, -3, -4};
    std::array<float *, 2> stereo{left.data(), right.data()};
    check(processor.process(stereo, 4), "new stereo block capacity takes effect");
    check(left[3] == 4 && right[3] == -4, "successful prepare restores unity");
}

void gain() {
    darkvoice::GainProcessor processor;
    check(processor.prepare({48000.0, 4, 1}), "gain test preparation");
    for (float level : {0.0F, 0.25F, 1.0F, 4.0F}) {
        std::array<float, 4> data{1.0F, -0.5F, 0.0F, 0.125F};
        const auto original = data;
        std::array<float *, 1> channels{data.data()};
        check(processor.setGain(level), "valid gain accepted");
        check(processor.process(channels, data.size()), "gain processing succeeds");
        for (std::size_t i = 0; i < data.size(); ++i) {
            check(near(data[i], original[i] * level), "gain matches scalar reference");
        }
    }
    for (float invalid :
         {-0.001F, 4.001F, std::numeric_limits<float>::infinity(),
          -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        check(processor.setGain(0.5F), "establish gain before invalid update");
        check(!processor.setGain(invalid), "invalid gain rejected");
        std::array<float, 1> data{0.8F};
        std::array<float *, 1> channels{data.data()};
        check(processor.process(channels, 1), "process after invalid gain");
        check(near(data[0], 0.4F), "invalid gain preserves previous value");
    }
}

void buffers() {
    darkvoice::GainProcessor processor;
    check(processor.prepare({48000.0, 4, 2}), "buffer test preparation");
    check(processor.setGain(0.5F), "buffer test gain");
    std::array<float, 6> left{99, 1, 2, 3, 4, 98};
    std::array<float, 6> right{97, 0, 0, 0, 0, 96};
    std::array<float *, 2> channels{left.data() + 1, right.data() + 1};
    check(processor.process(channels, 1), "one-frame block accepted");
    check(left[1] == 0.5F && left[2] == 2.0F, "only requested frames changed");
    check(right[1] == 0.0F, "no left-to-right crosstalk");
    const auto savedLeft = left;
    const auto savedRight = right;
    check(processor.process(channels, 0), "zero frames accepted");
    check(!processor.process(channels, 5), "oversized block rejected");
    std::array<float *, 1> wrongCount{left.data() + 1};
    check(!processor.process(wrongCount, 1), "wrong channel count rejected");
    check(!processor.process(std::span<float *>{}, 1), "empty channel span rejected");
    std::array<float *, 2> nullSecond{left.data() + 1, nullptr};
    check(!processor.process(nullSecond, 1), "null later channel rejected atomically");
    std::array<float *, 2> nullFirst{nullptr, right.data() + 1};
    check(!processor.process(nullFirst, 1), "null first channel rejected");
    std::array<float *, 2> duplicate{left.data() + 1, left.data() + 1};
    check(!processor.process(duplicate, 1), "identical channel pointers rejected");
    check(left == savedLeft && right == savedRight, "all rejected blocks preserve samples");
    check(processor.process(channels, 4), "maximum block accepted");
    check(left.front() == 99 && left.back() == 98 && right.front() == 97 && right.back() == 96,
          "guard samples preserved");
    check(processor.prepare({48000.0, 4, 1}), "mono zero-frame preparation");
    std::array<float *, 1> nullMono{nullptr};
    check(processor.process(nullMono, 0), "zero-frame mono does not require sample pointer");
}

void realtime() {
    darkvoice::GainProcessor processor;
    check(processor.prepare({48000.0, 32, 2}), "realtime test preparation");
    std::array<float, 32> left{};
    std::array<float, 32> right{};
    std::array<float *, 2> channels{left.data(), right.data()};
    check(processor.process(channels, 32), "warm-up processing");
    const auto probeBefore = allocation_probe::count.load(std::memory_order_relaxed);
    void *probe = ::operator new(17);
    ::operator delete(probe);
    check(allocation_probe::count.load(std::memory_order_relaxed) == probeBefore + 1,
          "allocation detector observes an actual allocation");
    bool succeeded = true;
    const auto before = allocation_probe::count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 1000; ++iteration) {
        succeeded = processor.setGain(0.5F) && succeeded;
        succeeded = processor.process(channels, 32) && succeeded;
        processor.reset();
    }
    const auto after = allocation_probe::count.load(std::memory_order_relaxed);
    check(succeeded, "all measured callback operations succeed");
    check(after == before, "process/setGain/reset allocate no C++ heap memory");
    // This probe does not certify absence of direct malloc, locks, I/O, or deadlines.
}

void render() {
    constexpr std::size_t samples = 1024;
    constexpr double pi = 3.14159265358979323846;
    std::array<float, samples> whole{};
    for (std::size_t i = 0; i < samples; ++i) {
        const double t = static_cast<double>(i) / 48000.0;
        whole[i] =
            static_cast<float>(0.4 * std::sin(2 * pi * 220 * t) + 0.2 * std::sin(2 * pi * 880 * t));
    }
    auto segmented = whole;
    const auto original = whole;
    darkvoice::GainProcessor fullProcessor;
    darkvoice::GainProcessor blockProcessor;
    check(fullProcessor.prepare({48000.0, samples, 1}), "full render preparation");
    check(blockProcessor.prepare({48000.0, 64, 1}), "segmented render preparation");
    check(fullProcessor.setGain(0.375F) && blockProcessor.setGain(0.375F), "render gains accepted");
    std::array<float *, 1> fullChannel{whole.data()};
    check(fullProcessor.process(fullChannel, samples), "full render succeeds");
    std::size_t offset = 0;
    while (offset < samples) {
        const auto remaining = samples - offset;
        const auto requested = (offset % 63) + 1;
        const auto frames = requested < remaining ? requested : remaining;
        std::array<float *, 1> channel{segmented.data() + offset};
        check(blockProcessor.process(channel, frames), "variable render block succeeds");
        offset += frames;
    }
    for (std::size_t i = 0; i < samples; ++i) {
        check(near(whole[i], original[i] * 0.375F), "render matches independent reference");
        check(whole[i] == segmented[i], "block segmentation preserves output");
    }
}
} // namespace

int main(int argc, char **argv) {
    if (argc > 2) {
        std::cerr << "Usage: core_tests [lifecycle|gain|buffers|realtime|render]\n";
        return 2;
    }
    const std::string_view selected = argc == 2 ? argv[1] : "all";
    bool found = false;
    const auto run = [&](std::string_view name, void (*suite)()) {
        if (selected == "all" || selected == name) {
            found = true;
            suite();
        }
    };
    run("lifecycle", lifecycle);
    run("gain", gain);
    run("buffers", buffers);
    run("realtime", realtime);
    run("render", render);
    if (!found) {
        std::cerr << "Unknown suite: " << selected << '\n';
        return 2;
    }
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
