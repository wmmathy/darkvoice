#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <darkvoice/PitchFormantProcessor.hpp>
#include <numbers>
#include <numeric>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;
constexpr std::array<darkvoice::PitchFormantParameters, 4> targets{
    {{7, -3, true, 100}, {-12, 7, false, 0}, {12, -12, true, 200}, {}}};

// Deterministic, asymmetric channels with voiced/noise/transient sections.
void signal(std::vector<float> &left, std::vector<float> &right, double rate) {
    std::uint32_t state = 12345;
    for (std::size_t i = 0; i < left.size(); ++i) {
        state = state * 1664525U + 1013904223U;
        const double noise = static_cast<double>(state >> 8) / 8388608. - 1;
        const double t = i / rate;
        const auto section = static_cast<int>(t * 4) % 3;
        const double harmonic = std::sin(2 * std::numbers::pi * 110 * t) +
                                .4 * std::sin(2 * std::numbers::pi * 660 * t);
        left[i] = static_cast<float>(.1 * (section == 1 ? noise : harmonic));
        right[i] = static_cast<float>(
            .08 * (section == 1 ? -noise : std::sin(2 * std::numbers::pi * 147 * t)));
        if (i % static_cast<std::size_t>(rate / 8) == 0) {
            left[i] += .2f;
            right[i] -= .15f;
        }
    }
}

bool measure(double rate, darkvoice::PitchQuality quality, std::size_t block, bool comma) {
    darkvoice::PitchFormantProcessor fx;
    if (!fx.prepare({rate, block, 2, quality}))
        return false;
    // Round durations to whole callbacks; actual durations are reported below.
    const auto warmCount = static_cast<std::size_t>(std::ceil(rate / block));
    const auto count = static_cast<std::size_t>(std::ceil(3 * rate / block));
    std::vector<float> left((warmCount + count) * block), right(left.size());
    std::vector<double> times(count);
    signal(left, right, rate);
    std::size_t overruns = 0;
    bool finite = true;
    const double budgetUs = 1e6 * block / rate;
    for (std::size_t callback = 0; callback < warmCount + count; ++callback) {
        const auto offset = callback * block;
        std::array<float *, 2> ptr{left.data() + offset, right.data() + offset};
        // Alternate targets every ~0.25 seconds, independent of host block size.
        const auto target = static_cast<std::size_t>(offset / (rate * .25)) % targets.size();
        const auto begin = Clock::now();
        const bool ok = fx.setParameters(targets[target]) && fx.process(ptr, block);
        const auto end = Clock::now();
        if (!ok)
            return false;
        if (callback >= warmCount) {
            const auto us = std::chrono::duration<double, std::micro>(end - begin).count();
            times[callback - warmCount] = us;
            overruns += us > budgetUs;
        }
        // Inspect samples outside the measured callback interval.
        for (std::size_t i = offset; i < offset + block; ++i)
            finite = finite && std::isfinite(left[i]) && std::isfinite(right[i]);
    }
    if (!finite)
        return false;
    const double totalUs = std::accumulate(times.begin(), times.end(), 0.);
    std::sort(times.begin(), times.end());
    const auto percentile = [&](double fraction) {
        return times[static_cast<std::size_t>(std::ceil(fraction * count)) - 1];
    };
    const double seconds = count * block / rate;
    const char *name = quality == darkvoice::PitchQuality::Live     ? "Live"
                       : quality == darkvoice::PitchQuality::Studio ? "Studio"
                                                                    : "Offline";
    std::printf("%s{\"rate_hz\":%.0f,\"profile\":\"%s\",\"channels\":2,\"block_frames\":%zu,"
                "\"callbacks\":%zu,\"audio_seconds\":%.9f,\"warmup_seconds\":%.9f,"
                "\"latency_samples\":%zu,\"callback_budget_us\":%.6f,\"total_process_us\":%.6f,"
                "\"median_us\":%.6f,\"p95_us\":%.6f,\"p99_us\":%.6f,\"max_us\":%.6f,"
                "\"processing_audio_ratio\":%.9f,\"observed_budget_overruns\":%zu,\"finite\":true}",
                comma ? ",\n" : "", rate, name, block, count, seconds, warmCount * block / rate,
                fx.latencySamples(), budgetUs, totalUs, percentile(.5), percentile(.95),
                percentile(.99), times.back(), totalUs / (seconds * 1e6), overruns);
    return true;
}
} // namespace

int main(int argc, char **) {
    if (argc != 1) {
        std::fprintf(stderr, "Usage: darkvoice_sculptor_bench (no arguments)\n");
        return 2;
    }
    std::printf("{\"schema\":1,\"compiler\":\"%s\",\"build_type\":\"%s\","
                "\"clock\":\"steady_clock\",\"timing\":\"wall_time\",\"measurements\":[\n",
                DARKVOICE_BENCH_COMPILER, DARKVOICE_BENCH_BUILD_TYPE);
    bool comma = false;
    for (double rate : {44100., 48000., 88200., 96000.})
        for (auto quality : {darkvoice::PitchQuality::Live, darkvoice::PitchQuality::Studio,
                             darkvoice::PitchQuality::Offline})
            for (std::size_t block : {64U, 512U}) {
                if (!measure(rate, quality, block, comma)) {
                    std::fprintf(stderr, "Benchmark failed: rate %.0f profile %d block %zu\n", rate,
                                 static_cast<int>(quality), block);
                    return 1;
                }
                comma = true;
            }
    std::printf("\n]}\n");
    return 0;
}
