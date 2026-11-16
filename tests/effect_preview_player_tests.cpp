#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <darkvoice/EffectPreviewPlayer.hpp>
#include <iostream>
#include <limits>
#include <new>
#include <thread>

namespace {
thread_local bool callback = false;
std::atomic<unsigned> allocations{0}, deallocations{0};
unsigned checks = 0;
void check(bool good, const char *message) {
    ++checks;
    if (!good) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}
} // namespace
void *operator new(std::size_t size) {
    if (callback)
        ++allocations;
    if (auto *memory = std::malloc(size == 0 ? 1 : size))
        return memory;
    throw std::bad_alloc();
}
void *operator new[](std::size_t size) {
    return ::operator new(size);
}
void operator delete(void *memory) noexcept {
    if (callback && memory)
        ++deallocations;
    std::free(memory);
}
void operator delete[](void *memory) noexcept {
    ::operator delete(memory);
}
void operator delete(void *memory, std::size_t) noexcept {
    ::operator delete(memory);
}
void operator delete[](void *memory, std::size_t) noexcept {
    ::operator delete(memory);
}
void *operator new(std::size_t size, std::align_val_t alignment) {
    if (callback)
        ++allocations;
    void *memory = nullptr;
    if (posix_memalign(&memory, static_cast<std::size_t>(alignment), size == 0 ? 1 : size) == 0)
        return memory;
    throw std::bad_alloc();
}
void *operator new[](std::size_t size, std::align_val_t alignment) {
    return ::operator new(size, alignment);
}
void operator delete(void *memory, std::align_val_t) noexcept {
    ::operator delete(memory);
}
void operator delete[](void *memory, std::align_val_t alignment) noexcept {
    ::operator delete(memory, alignment);
}
void operator delete(void *memory, std::size_t, std::align_val_t alignment) noexcept {
    ::operator delete(memory, alignment);
}
void operator delete[](void *memory, std::size_t, std::align_val_t alignment) noexcept {
    ::operator delete(memory, alignment);
}
namespace {
using namespace darkvoice;
struct Fixture {
    PreviewExchange exchange;
    EffectPreviewPlayer player;
    PreviewFormat format;
    std::array<float, 2048> left{}, right{};
    std::array<float *, 2> pointers{left.data(), right.data()};
    explicit Fixture(double rate = 48000, std::size_t channels = 2,
                     QualityMode quality = QualityMode::Live)
        : format{rate, channels, 2048, quality} {
        check(player.prepare(format, exchange), "prepare");
    }
    void publish(std::uint64_t generation, std::size_t frames = 4000, int index = 0) {
        auto &slot = exchange.slots[static_cast<std::size_t>(index)];
        slot.state.store(PreviewSlotState::Writing);
        slot.clip = {};
        slot.clip.generation = generation;
        slot.clip.frames = frames;
        slot.clip.channels = format.channels;
        slot.clip.sampleRate = format.sampleRate;
        slot.clip.quality = format.quality;
        for (std::size_t channel = 0; channel < format.channels; ++channel) {
            slot.clip.dry[channel].assign(frames, .25f);
            slot.clip.processed[channel].assign(frames, .75f);
        }
        exchange.requestedGeneration.store(generation);
        exchange.renderGeneration.store(generation);
        slot.state.store(PreviewSlotState::Ready, std::memory_order_release);
        exchange.readyIndex.store(index, std::memory_order_release);
    }
    PreviewProcessResult run(std::size_t frames, bool blocked = false) {
        callback = true;
        auto result =
            player.process(std::span<float *>(pointers.data(), format.channels), frames, blocked);
        callback = false;
        return result;
    }
};
void basic() {
    Fixture f;
    const auto nan = std::bit_cast<float>(std::uint32_t{0x7fc01234});
    f.left.fill(nan);
    f.right.fill(-0.0f);
    check(!f.run(512).replaced, "idle replaced");
    check(std::bit_cast<std::uint32_t>(f.left[0]) == 0x7fc01234, "idle NaN payload");
    check(std::signbit(f.right[0]), "idle signed zero");
    f.publish(1);
    check(!f.run(0).replaced && f.exchange.readyIndex.load() == 0, "empty consumed mailbox");
    check(f.run(480).replaced, "preview not activated");
    check(f.exchange.positionFrames.load() == 480 && f.exchange.playingGeneration.load() == 1,
          "cursor");
    check(std::isfinite(f.left[0]) && std::abs(f.left[479] - .375f) < 1e-6f,
          "attack/nonfinite input");
    f.exchange.processed.store(false);
    f.left.fill(0);
    f.right.fill(0);
    f.run(480);
    check(std::abs(f.left[479] - .125f) < 1e-6f && f.left[0] > .125f, "A/B smoothing");
    check(f.exchange.positionFrames.load() == 960, "A/B restarted cursor");
    f.exchange.level.store(1);
    f.run(480);
    check(std::abs(f.left[479] - .25f) < 1e-6f, "level smoothing");
    f.exchange.requestedGeneration.store(0);
    f.exchange.renderGeneration.store(0);
    f.left.fill(nan);
    f.run(512);
    check(f.exchange.slots[0].state.load() == PreviewSlotState::Retired, "stop not retired");
    check(std::bit_cast<std::uint32_t>(f.left[479]) == 0x7fc01234 &&
              std::bit_cast<std::uint32_t>(f.left[511]) == 0x7fc01234,
          "zero blend changed raw bits");
    check(f.exchange.completedGeneration.load() == 0, "stop claimed completion");
    f.publish(2, 1000, 1);
    f.run(1000);
    check(f.exchange.completedGeneration.load() == 2 && f.exchange.playingGeneration.load() == 0,
          "natural completion");
    check(f.exchange.slots[1].state.load() == PreviewSlotState::Retired, "natural retirement");
    check(!f.run(512).replaced, "natural replay");
}
void validationAndBlocking() {
    {
        Fixture raced;
        raced.exchange.requestedGeneration.store(10);
        raced.exchange.renderGeneration.store(11);
        raced.run(0, true);
        check(raced.exchange.blockedThroughGeneration.load() == 11 &&
                  raced.exchange.requestedGeneration.load() == 10 &&
                  raced.exchange.renderGeneration.load() == 0,
              "racing render generation not blocked");
    }
    Fixture f;
    f.publish(10);
    f.left.fill(.2f);
    f.pointers[1] = f.left.data() + 1;
    check(!f.run(512).replaced && f.exchange.readyIndex.load() == 0, "overlap accepted");
    f.pointers[1] = nullptr;
    check(!f.run(512).replaced && f.exchange.readyIndex.load() == 0, "null accepted");
    f.pointers[1] = f.right.data();
    check(!f.run(2049).replaced && f.exchange.readyIndex.load() == 0, "oversize accepted");
    f.run(128);
    const auto cursor = f.exchange.positionFrames.load();
    f.pointers[1] = nullptr;
    f.run(128);
    check(f.exchange.positionFrames.load() == cursor && f.left[0] != .2f,
          "invalid advanced cursor");
    f.left[0] = std::bit_cast<float>(std::uint32_t{0x7fa12345});
    f.run(128, true);
    check(std::bit_cast<std::uint32_t>(f.left[0]) == 0x7fa12345, "blocked payload");
    check(f.exchange.requestedGeneration.load() == 10 && f.exchange.renderGeneration.load() == 0 &&
              f.exchange.blockedThroughGeneration.load() == 10,
          "blocked invalidation");
    check(f.exchange.slots[0].state.load() == PreviewSlotState::Retired,
          "blocked active retirement");
    f.pointers[1] = f.right.data();
    f.publish(10, 4000, 1);
    check(!f.run(128).replaced, "blocked generation replayed");
    f.publish(11, 4000, 2);
    check(f.run(128).replaced, "new postblocked generation rejected");
    f.publish(12, 4000, 0);
    f.run(480);
    check(f.exchange.slots[2].state.load() == PreviewSlotState::Retired &&
              f.exchange.slots[0].state.load() == PreviewSlotState::Ready,
          "new clip overlapped old fade");
    check(f.run(128).replaced && f.exchange.playingGeneration.load() == 12,
          "new generation activation");
}
void staleAndMalformed() {
    for (int fault = 0; fault != 8; ++fault) {
        Fixture f;
        f.publish(2);
        auto &clip = f.exchange.slots[0].clip;
        switch (fault) {
        case 0:
            f.exchange.requestedGeneration.store(3);
            break;
        case 1:
            f.exchange.renderGeneration.store(3);
            break;
        case 2:
            clip.frames = 0;
            break;
        case 3:
            clip.frames = 480001;
            break;
        case 4:
            clip.channels = 1;
            break;
        case 5:
            clip.sampleRate = 44100;
            break;
        case 6:
            clip.quality = static_cast<QualityMode>(255);
            break;
        case 7:
            clip.processed[1].resize(2);
            break;
        }
        f.left.fill(.2f);
        check(!f.run(512).replaced && f.left[0] == .2f, "malformed/stale accepted");
        check(f.exchange.slots[0].state.load() == PreviewSlotState::Retired,
              "invalid slot retained");
    }
}
void modes() {
    for (const double rate : {44100., 48000., 88200., 96000.})
        for (const auto channels : {1u, 2u})
            for (int quality = 0; quality != 3; ++quality) {
                Fixture f(rate, channels, static_cast<QualityMode>(quality));
                f.publish(1, 4000);
                std::size_t left = 4000;
                while (left != 0) {
                    const auto block = std::min<std::size_t>(left, 317);
                    check(f.run(block).replaced || left <= 317, "mode preview silent");
                    left -= block;
                }
                check(f.exchange.completedGeneration.load() == 1, "mode completion");
            }
}
void concurrentProducer() {
    Fixture f;
    std::atomic<unsigned> turn{0};
    constexpr unsigned iterations = 150;
    std::thread producer([&] {
        for (unsigned generation = 1; generation <= iterations; ++generation) {
            while (turn.load(std::memory_order_acquire) != (generation - 1) * 2)
                std::this_thread::yield();
            const auto index = static_cast<int>(generation % previewSlotCount);
            auto &slot = f.exchange.slots[static_cast<std::size_t>(index)];
            const auto state = slot.state.load(std::memory_order_acquire);
            if (state != PreviewSlotState::Free && state != PreviewSlotState::Retired)
                std::abort();
            f.publish(generation, 1000, index);
            turn.store(generation * 2 - 1, std::memory_order_release);
        }
    });
    for (unsigned generation = 1; generation <= iterations; ++generation) {
        while (turn.load(std::memory_order_acquire) != generation * 2 - 1)
            std::this_thread::yield();
        check(f.run(1000).replaced, "concurrent activation");
        check(f.exchange.completedGeneration.load() == generation, "concurrent completion");
        turn.store(generation * 2, std::memory_order_release);
    }
    producer.join();
}
void overlappingProducer() {
    Fixture f;
    f.publish(1, 48000);
    f.run(1);
    std::atomic<bool> started{false}, finished{false};
    std::thread producer([&] {
        started.store(true, std::memory_order_release);
        for (unsigned generation = 2; generation <= 300; ++generation) {
            int index = -1;
            while (index < 0) {
                for (std::size_t candidate = 0; candidate < previewSlotCount; ++candidate) {
                    auto expected =
                        f.exchange.slots[candidate].state.load(std::memory_order_acquire);
                    if ((expected == PreviewSlotState::Retired ||
                         expected == PreviewSlotState::Free) &&
                        f.exchange.slots[candidate].state.compare_exchange_strong(
                            expected, PreviewSlotState::Writing, std::memory_order_acq_rel)) {
                        index = static_cast<int>(candidate);
                        break;
                    }
                }
                if (index < 0)
                    std::this_thread::yield();
            }
            auto &slot = f.exchange.slots[static_cast<std::size_t>(index)];
            slot.clip = {};
            slot.clip.generation = generation;
            slot.clip.frames = 2400;
            slot.clip.channels = 2;
            slot.clip.sampleRate = 48000;
            for (unsigned channel = 0; channel < 2; ++channel) {
                slot.clip.dry[channel].assign(2400, .25f);
                slot.clip.processed[channel].assign(2400, .75f);
            }
            f.exchange.requestedGeneration.store(generation, std::memory_order_release);
            f.exchange.renderGeneration.store(generation, std::memory_order_release);
            slot.state.store(PreviewSlotState::Ready, std::memory_order_release);
            const auto previous = f.exchange.readyIndex.exchange(index, std::memory_order_acq_rel);
            if (previous >= 0)
                f.exchange.slots[static_cast<std::size_t>(previous)].state.store(
                    PreviewSlotState::Retired, std::memory_order_release);
        }
        finished.store(true, std::memory_order_release);
    });
    while (!started.load(std::memory_order_acquire))
        std::this_thread::yield();
    do {
        f.left.fill(.1f);
        f.right.fill(.1f);
        const auto result = f.run(31);
        if (result.replaced && (!std::isfinite(result.outputPeak) || result.outputPeak > 1))
            std::abort();
    } while (!finished.load(std::memory_order_acquire));
    producer.join();
    for (unsigned block = 0; block < 200 && f.exchange.completedGeneration.load() != 300; ++block)
        f.run(127);
    check(f.exchange.completedGeneration.load() == 300, "concurrent latest generation lost");
    for (const auto &slot : f.exchange.slots)
        check(slot.state.load() != PreviewSlotState::Active, "concurrent active not retired");
}
void lifecycleAndAdversarial() {
    Fixture f;
    f.publish(1, 20);
    f.run(20);
    check(f.exchange.completedGeneration.load() == 1 && f.left[9] > .3f, "short clip fade");
    f.publish(2);
    f.exchange.level.store(std::numeric_limits<float>::quiet_NaN());
    f.run(500);
    check(std::isfinite(f.left[0]) && std::isfinite(f.left[499]), "adversarial level");
    const auto position = f.exchange.positionFrames.load();
    f.run(2049);
    check(f.exchange.positionFrames.load() == position, "oversize advanced");
    callback = true;
    std::span<float *> noChannels;
    const auto malformed = f.player.process(noChannels, 4, false);
    callback = false;
    check(!malformed.replaced && f.exchange.positionFrames.load() == position, "layout advanced");
    f.run(0, true);
    check(f.exchange.slots[0].state.load() == PreviewSlotState::Retired,
          "empty blocked did not retire");
    f.player.release();
    check(f.exchange.audioState.load() == PreviewState::Unavailable, "release state");
    check(!f.run(512).replaced, "released process");
    check(f.player.prepare(f.format, f.exchange), "reprepare");
    f.publish(3);
    check(f.run(1000).replaced, "reprepared process");
}
void guardSelfTest() {
    callback = true;
    void *normal = ::operator new(19);
    ::operator delete(normal);
    void *aligned = ::operator new(64, std::align_val_t{64});
    ::operator delete(aligned, std::align_val_t{64});
    callback = false;
    check(allocations.load() == 2 && deallocations.load() == 2, "allocation guards ineffective");
    allocations.store(0);
    deallocations.store(0);
}

} // namespace
int main() {
    guardSelfTest();
    basic();
    validationAndBlocking();
    staleAndMalformed();
    modes();
    concurrentProducer();
    overlappingProducer();
    lifecycleAndAdversarial();
    check(allocations.load() == 0, "callback allocation");
    check(deallocations.load() == 0, "callback deallocation");
    std::cout << checks << " preview player checks passed; callback allocations/deallocations 0\n";
}
