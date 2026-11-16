#include "EffectPreviewService.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>

namespace {
void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
darkvoice::PreviewStatus ready(darkvoice::EffectPreviewService &service) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(90);
    for (;;) {
        const auto status = service.status();
        require(status.state != darkvoice::PreviewState::Error,
                "Preview worker failed decoding/rendering bundled source");
        if (status.state == darkvoice::PreviewState::WaitingForAudio)
            return status;
        require(std::chrono::steady_clock::now() < deadline, "Preview worker timeout");
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}
void playToEnd(darkvoice::EffectPreviewService &service) {
    std::array<float, 512> left{}, right{};
    std::array<float *, 2> channels{left.data(), right.data()};
    float peak = 0;
    bool replaced = false, played = false;
    for (int block = 0; block < 2000; ++block) {
        left.fill(0);
        right.fill(0);
        const auto result = service.process(channels, left.size(), false);
        replaced |= result.replaced;
        for (auto sample : left) {
            require(std::isfinite(sample), "Nonfinite preview output");
            peak = std::max(peak, std::abs(sample));
        }
        const auto status = service.status();
        played |= status.state == darkvoice::PreviewState::Playing;
        if (status.state == darkvoice::PreviewState::Idle && played)
            break;
    }
    require(played && replaced && peak > 0.0001f, "Bundled clean vocal never played");
    require(service.status().state == darkvoice::PreviewState::Idle, "Preview did not finish");
    for (int block = 0; block < 4; ++block) {
        left.fill(.123f);
        right.fill(.123f);
        require(!service.process(channels, left.size(), false).replaced,
                "Completed generation replayed");
        require(left[100] == .123f, "Completed generation changed live input");
    }
}
} // namespace
int main() {
    try {
        using namespace darkvoice;
        EffectPreviewService service;
        require(service.status().state == PreviewState::Unavailable, "Unprepared state");
        require(!previewCatalog().empty(), "Empty effect catalog");
        const auto id = previewCatalog().front().id;
        require(!service.request(id, PreviewVoice::Male, PreviewSampleKind::Speech),
                "Unprepared request accepted");
        PreviewFormat format;
        require(service.prepare(format), "Prepare failed");
        require(service.status().state == PreviewState::Idle, "Prepared state");
        require(!service.setLevel(std::numeric_limits<float>::quiet_NaN()), "NaN level accepted");
        require(!service.setLevel(-.01f) && !service.setLevel(1.01f),
                "Out of range level accepted");
        require(service.setLevel(.5f), "Valid level rejected");
        service.setProcessed(false);
        for (auto voice : {PreviewVoice::Male, PreviewVoice::Female})
            for (auto kind : {PreviewSampleKind::Speech, PreviewSampleKind::Singing}) {
                require(service.request(id, voice, kind), "Valid asset request rejected");
                const auto waiting = ready(service);
                require(waiting.voice == voice && waiting.sampleKind == kind,
                        "Source metadata mismatch");
                require(waiting.state == PreviewState::WaitingForAudio,
                        "Suspended callbacks falsely playing");
                require(!service.request(id, static_cast<PreviewVoice>(255), kind),
                        "Invalid voice accepted");
                require(service.status().voice == voice,
                        "Invalid request changed accepted request");
                playToEnd(service);
            }
        require(service.request(id, PreviewVoice::Male, PreviewSampleKind::Speech),
                "Request failed");
        ready(service);
        service.stop();
        std::array<float, 512> left{}, right{};
        std::array<float *, 2> channels{left.data(), right.data()};
        for (int i = 0; i < 4; ++i)
            require(!service.process(channels, 512, false).replaced,
                    "Stopped ready generation played");
        require(service.status().state == PreviewState::Idle, "Stop did not idle");
        require(service.request(id, PreviewVoice::Female, PreviewSampleKind::Singing),
                "Request failed");
        ready(service);
        service.process(channels, 512, true);
        require(service.status().blocked && service.status().state == PreviewState::Idle,
                "Blocked generation not cancelled");
        for (int i = 0; i < 8; ++i)
            require(!service.process(channels, 512, false).replaced, "Blocked generation replayed");
        // Reprepare cancels an in-flight decode/render and retains monotonic generations.
        require(service.request(id, PreviewVoice::Male, PreviewSampleKind::Singing),
                "Request failed");
        format.sampleRate = 96000;
        require(service.prepare(format), "Reprepare failed");
        require(service.status().state == PreviewState::Idle, "Reprepare retained request");
        require(service.request(id, PreviewVoice::Female, PreviewSampleKind::Speech),
                "Reprepared request failed");
        ready(service);
        playToEnd(service);
        // Concurrent callback/UI work: the callback never touches UI/worker locks.
        std::jthread audio([&](std::stop_token stop) {
            std::array<float, 512> a{}, b{};
            std::array<float *, 2> pointers{a.data(), b.data()};
            while (!stop.stop_requested()) {
                service.process(pointers, 512, false);
                std::this_thread::yield();
            }
        });
        for (int i = 0; i < 100; ++i) {
            service.request(id, i % 2 ? PreviewVoice::Male : PreviewVoice::Female,
                            PreviewSampleKind::Speech);
            service.setProcessed(i % 2 != 0);
            service.setLevel(static_cast<float>(i % 10) / 10);
            (void)service.status();
            if (i % 3 == 0)
                service.stop();
        }
        audio.request_stop();
        audio.join();
        service.stop();
        require(service.setLevel(.5f), "Restore preview level");
        service.setProcessed(false);
        require(service.request(id, PreviewVoice::Female, PreviewSampleKind::Singing),
                "Final latest-generation request failed");
        const auto latest = ready(service);
        require(latest.voice == PreviewVoice::Female &&
                    latest.sampleKind == PreviewSampleKind::Singing,
                "Superseded generation metadata survived");
        playToEnd(service);
        for (double rate : {44100.0, 88200.0}) {
            format.sampleRate = rate;
            require(service.prepare(format), "Supported SRC rate rejected");
            require(service.request(id, PreviewVoice::Male, PreviewSampleKind::Speech),
                    "SRC speech request failed");
            ready(service);
            playToEnd(service);
        }
        format.channels = 1;
        require(service.prepare(format), "Mono prepare failed");
        for (const auto &definition : previewCatalog())
            if (definition.stereoDemonstration)
                require(
                    !service.request(definition.id, PreviewVoice::Male, PreviewSampleKind::Speech),
                    "Stereo demonstration accepted in mono");
        // Hosts may release/reprepare while the editor issues requests. No audio
        // callback is running during lifecycle changes; UI requests must not
        // create or inspect a worker while shutdown joins and reclaims its slots.
        std::atomic<unsigned> attempts{0};
        std::jthread requester([&](std::stop_token stop) {
            while (!stop.stop_requested()) {
                (void)service.request(id, PreviewVoice::Male, PreviewSampleKind::Speech);
                attempts.fetch_add(1);
                std::this_thread::yield();
            }
        });
        while (attempts.load() == 0)
            std::this_thread::yield();
        for (int cycle = 0; cycle < 30; ++cycle) {
            service.release();
            require(!service.request(id, PreviewVoice::Female, PreviewSampleKind::Speech),
                    "Request accepted after release");
            require(service.prepare(format), "Concurrent UI/lifecycle prepare failed");
        }
        requester.request_stop();
        requester.join();
        service.release();
        require(service.status().state == PreviewState::Unavailable, "Release state");
        format.sampleRate = 123;
        require(!service.prepare(format), "Invalid format accepted");
        require(service.status().error == PreviewError::UnsupportedFormat,
                "Unsupported format not reported");
        require(effectPreviewCredits().find("Carnegie Mellon") != std::string::npos &&
                    effectPreviewCredits().find("CC BY 4.0") != std::string::npos,
                "Local recording credits absent");
        std::cout << "Preview service bundled-source, lifecycle, completion, cancellation and "
                     "concurrency checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
