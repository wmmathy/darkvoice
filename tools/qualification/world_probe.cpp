// SPDX-License-Identifier: AGPL-3.0-only
// Developer-only offline experiment; successful exit means valid measurements,
// not a qualified pitch/formant processor. Uses no upstream FFT implementation.
#include "epoch_tracker/epoch_tracker.h"
#include "world/cheaptrick.h"
#include "world/common.h"
#include "world/d4c.h"
#include "world/dio.h"
#include "world/fft.h"
#include "world/harvest.h"
#include "world/stonemask.h"
#include "world/synthesis.h"
#include <chrono>
#include <darkvoice/PitchDetector.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <new>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
std::atomic<bool> watching{false};
std::atomic<std::size_t> allocations{0};
void *allocate(std::size_t n) {
    if (watching.load())
        allocations.fetch_add(1);
    if (void *p = std::malloc(n ? n : 1))
        return p;
    throw std::bad_alloc();
}
} // namespace
void *operator new(std::size_t n) {
    return allocate(n);
}
void *operator new[](std::size_t n) {
    return allocate(n);
}
void operator delete(void *p) noexcept {
    std::free(p);
}
void operator delete[](void *p) noexcept {
    std::free(p);
}
void operator delete(void *p, std::size_t) noexcept {
    std::free(p);
}
void operator delete[](void *p, std::size_t) noexcept {
    std::free(p);
}

namespace {
std::size_t checkCount = 0;
void require(bool condition, const char *message) {
    ++checkCount;
    if (!condition)
        throw std::runtime_error(message);
}
void execute(fft_plan p) {
    watching.store(true);
    fft_execute(p);
    watching.store(false);
}
void selfTest() {
    const auto startChecks = checkCount;
    for (int n = 2; n <= 131072; n *= 2) {
        std::vector<double> x(n), recovered(n);
        std::vector<std::complex<double>> z(n), saved(n), transformed(n);
        for (int i = 0; i < n; ++i) {
            x[i] = .3 + .2 * (i % 2 ? -1 : 1) + .7 * std::sin(2 * std::numbers::pi * i / n);
            z[i] = {x[i], .13 * std::cos(4 * std::numbers::pi * i / n)};
        }
        saved = z;
        auto *a = reinterpret_cast<fft_complex *>(z.data());
        auto *b = reinterpret_cast<fft_complex *>(transformed.data());
        auto forward = fft_plan_dft_1d(n, a, b, FFT_FORWARD, FFT_ESTIMATE);
        auto inverse = fft_plan_dft_1d(n, b, a, FFT_BACKWARD, FFT_ESTIMATE);
        execute(forward);
        const auto complexExpected = transformed;
        double originalPower = 0, transformedPower = 0;
        for (int i = 0; i < n; ++i) {
            originalPower += std::norm(saved[i]);
            transformedPower += std::norm(transformed[i]) / n;
            if (n <= 32) {
                std::complex<double> expected{};
                for (int j = 0; j < n; ++j)
                    expected += saved[j] * std::polar(1.0, -2 * std::numbers::pi * i * j / n);
                require(std::abs(expected - transformed[i]) < 1e-10 * n, "direct DFT sign/value");
            }
        }
        require(std::abs(originalPower - transformedPower) < 1e-9 * n, "Parseval");
        execute(inverse);
        for (int i = 0; i < n; ++i)
            require(std::abs(z[i] / double(n) - saved[i]) < 1e-10, "complex roundtrip");
        fft_destroy_plan(forward);
        fft_destroy_plan(inverse);
        auto realForward = fft_plan_dft_r2c_1d(n, x.data(), b, FFT_ESTIMATE);
        auto realInverse = fft_plan_dft_c2r_1d(n, b, recovered.data(), FFT_ESTIMATE);
        execute(realForward);
        require(std::abs(transformed[0].real() / n - .3) < 1e-10, "DC amplitude");
        const double nyquist = .2;
        require(std::abs(transformed[n / 2].real() / n - nyquist) < 1e-10, "Nyquist amplitude");
        if (n > 2)
            require(std::abs(transformed[1] / double(n) - std::complex<double>(0, -.35)) < 1e-10,
                    "analytic sine amplitude/sign");
        execute(realInverse);
        for (int i = 0; i < n; ++i)
            require(std::abs(recovered[i] / n - x[i]) < 1e-10, "real roundtrip");
        fft_destroy_plan(realForward);
        fft_destroy_plan(realInverse);
        // C2C in-place, independently compared to out-of-place mathematical result.
        z = saved;
        auto inplace = fft_plan_dft_1d(n, a, a, FFT_FORWARD, FFT_ESTIMATE);
        execute(inplace);
        fft_destroy_plan(inplace);
        for (int i = 0; i < n; ++i)
            require(std::abs(z[i] - complexExpected[i]) < 1e-10 * n, "inplace C2C");
        // Real in-place buffers need N+2 doubles for the packed half-spectrum.
        std::vector<double> packed(n + 2);
        std::copy(x.begin(), x.end(), packed.begin());
        auto *packedComplex = reinterpret_cast<fft_complex *>(packed.data());
        auto packedForward = fft_plan_dft_r2c_1d(n, packed.data(), packedComplex, FFT_ESTIMATE);
        auto packedInverse = fft_plan_dft_c2r_1d(n, packedComplex, packed.data(), FFT_ESTIMATE);
        execute(packedForward);
        for (int i = 0; i <= n / 2; ++i)
            require(std::abs(std::complex<double>(packed[2 * i], packed[2 * i + 1]) -
                             transformed[i]) < 1e-10 * n,
                    "inplace R2C");
        execute(packedInverse);
        for (int i = 0; i < n; ++i)
            require(std::abs(packed[i] / n - x[i]) < 1e-10, "inplace C2R");
        fft_destroy_plan(packedForward);
        fft_destroy_plan(packedInverse);
    }
    // Exercise WORLD's complex FFT caller against an analytic stable AR(1)
    // minimum-phase response, not merely an FFT roundtrip.
    MinimumPhaseAnalysis minimum{};
    InitializeMinimumPhaseAnalysis(4096, &minimum);
    for (int i = 0; i <= 2048; ++i) {
        const auto h = 1.0 / (1.0 - .7 * std::polar(1.0, -2 * std::numbers::pi * i / 4096));
        minimum.log_spectrum[i] = std::log(std::abs(h));
    }
    GetMinimumPhaseSpectrum(&minimum);
    for (int i = 0; i <= 2048; ++i) {
        const auto h = 1.0 / (1.0 - .7 * std::polar(1.0, -2 * std::numbers::pi * i / 4096));
        const std::complex<double> measured(minimum.minimum_phase_spectrum[i][0],
                                            minimum.minimum_phase_spectrum[i][1]);
        require(std::abs(h - measured) < 1e-10, "WORLD minimum-phase AR1 compatibility");
    }
    DestroyMinimumPhaseAnalysis(&minimum);
    require(allocations.load() == 0, "FFT execution allocated");
    std::printf("FFT invariants: %zu checks, allocations during execute: %zu\n",
                checkCount - startChecks, allocations.load());
}

enum class Signal { Gaussian, Sine, Multi };
struct Case {
    double f0, center, pitch, formant;
    int rate = 48000;
    Signal kind = Signal::Gaussian;
    bool preserve = true;
};
const std::vector<Case> narrow{{100, 600, 12, 0}, {70, 600, -12, 0}, {180, 1200, 12, 7}};
std::vector<Case> retained() {
    std::vector<Case> cases;
    for (int rate : {44100, 48000, 88200, 96000})
        for (double pitch : {-12., -7., 7., 12.})
            cases.push_back({110, 0, pitch, 0, rate, Signal::Sine, false});
    for (auto shifts : {std::pair{7., 0.}, std::pair{0., 7.}, std::pair{0., -7.}})
        cases.push_back({100, 0, shifts.first, shifts.second, 48000, Signal::Multi, true});
    for (double center : {600., 1200.})
        for (double shift : {-7., 7.})
            cases.push_back({100, center, 0, shift});
    for (int rate : {48000, 96000})
        for (double f0 : {70., 180.})
            for (double center : {600., 1200.})
                for (double shift : {-7., 7.})
                    cases.push_back({f0, center, 0, shift, rate});
    cases.insert(cases.end(), narrow.begin(), narrow.end());
    require(cases.size() == 42, "retained manifest size");
    return cases;
}
constexpr double period = 5;
void write(const std::filesystem::path &p, const void *data, std::size_t bytes) {
    std::ofstream f(p, std::ios::binary);
    if (bytes)
        f.write(static_cast<const char *>(data), static_cast<std::streamsize>(bytes));
    if (!f)
        throw std::runtime_error("measurement write failed");
}
struct Trajectory {
    std::vector<double> time, f0;
    std::vector<float> reaperRaw;
    std::size_t pcmClips = 0;
    int32_t backendStatus = 0;
};
Trajectory estimateTrajectory(int method, const std::vector<float> &original,
                              const std::vector<double> &x, int fs, int fftSize,
                              bool allowUnavailable = false) {
    const int frames = static_cast<int>(original.size());
    const int count = GetSamplesForDIO(fs, frames, period);
    Trajectory trajectory;
    trajectory.time.resize(count);
    trajectory.f0.resize(count);
    auto &time = trajectory.time;
    auto &f0 = trajectory.f0;
    for (int i = 0; i < count; ++i)
        time[i] = i * period / 1000.;
    std::vector<double> refined(count);
    const auto status = [&](bool ok, int32_t code, const char *message) {
        if (!ok) {
            if (!allowUnavailable)
                require(false, message);
            trajectory.backendStatus = code;
        }
        return ok;
    };
    if (method == 0) {
        DioOption options{};
        InitializeDioOption(&options);
        options.f0_floor = 50;
        options.f0_ceil = 800;
        options.frame_period = period;
        Dio(x.data(), frames, fs, &options, time.data(), f0.data());
        StoneMask(x.data(), frames, fs, time.data(), f0.data(), count, refined.data());
        f0 = refined;
    } else if (method == 1) {
        HarvestOption options{};
        InitializeHarvestOption(&options);
        options.f0_floor = 50;
        options.f0_ceil = 800;
        options.frame_period = period;
        Harvest(x.data(), frames, fs, &options, time.data(), f0.data());
    } else if (method == 2) {
        darkvoice::PitchDetector detector;
        require(detector.prepare(fs, static_cast<std::size_t>(fftSize), 50, 800),
                "fixedYIN prepare");
        std::vector<float> window(static_cast<std::size_t>(fftSize));
        for (int frame = 0; frame < count; ++frame) {
            time[frame] = frame * period / 1000.;
            const int start = static_cast<int>(std::floor(time[frame] * fs)) - fftSize / 2;
            for (int sample = 0; sample < fftSize; ++sample) {
                const int input = start + sample;
                window[static_cast<std::size_t>(sample)] =
                    input >= 0 && input < frames ? original[static_cast<std::size_t>(input)] : 0.f;
            }
            const auto estimate = detector.analyze(window);
            f0[frame] = estimate.voiced ? estimate.frequencyHz : 0.;
        }
    } else {
        std::vector<int16_t> pcm(original.size());
        for (std::size_t i = 0; i < original.size(); ++i) {
            const double rounded = std::round(double(original[i]) * 32768.);
            if (rounded < -32768 || rounded > 32767)
                ++trajectory.pcmClips;
            pcm[i] = static_cast<int16_t>(std::clamp(rounded, -32768., 32767.));
        }
        EpochTracker tracker;
        if (!status(
                tracker.Init(pcm.data(), frames, static_cast<float>(fs), 50.f, 800.f, false, false),
                1, "REAPER Init"))
            return trajectory;
        if (!status(tracker.ComputeFeatures(), 2, "REAPER ComputeFeatures"))
            return trajectory;
        if (!status(tracker.TrackEpochs(), 3, "REAPER TrackEpochs"))
            return trajectory;
        std::vector<float> result, correlations;
        if (!status(tracker.ResampleAndReturnResults(.005f, &result, &correlations), 4,
                    "REAPER resample"))
            return trajectory;
        trajectory.reaperRaw = result;
        for (float value : result)
            require(std::isfinite(value), "nonfinite REAPER contour");
        for (int frame = 0; frame < count; ++frame) {
            time[frame] = frame * period / 1000.;
            f0[frame] = static_cast<std::size_t>(frame) < result.size() && result[frame] > 0
                            ? result[frame]
                            : 0.;
        }
    }
    return trajectory;
}

double integerNccf(const std::vector<double> &window, int lag) {
    if (lag < 1 || lag >= static_cast<int>(window.size()))
        return -1.;
    double ab = 0, aa = 0, bb = 0;
    for (std::size_t i = 0; i + static_cast<std::size_t>(lag) < window.size(); ++i) {
        const double a = window[i], b = window[i + static_cast<std::size_t>(lag)];
        ab += a * b;
        aa += a * a;
        bb += b * b;
    }
    const double den = std::sqrt(aa * bb);
    return den > 0 && std::isfinite(den) ? ab / den : -1.;
}
double fractionalNccf(const std::vector<double> &window, double lag) {
    if (!std::isfinite(lag) || lag < 1 || lag >= window.size())
        return -1.;
    const auto low = static_cast<std::size_t>(std::floor(lag)),
               ceil = static_cast<std::size_t>(std::ceil(lag));
    const double fraction = lag - low;
    double ab = 0, aa = 0, bb = 0;
    for (std::size_t i = 0; i + ceil < window.size(); ++i) {
        const double a = window[i],
                     b = window[i + low] +
                         fraction *
                             (window[std::min(i + low + 1, window.size() - 1)] - window[i + low]);
        ab += a * b;
        aa += a * a;
        bb += b * b;
    }
    const double den = std::sqrt(aa * bb);
    return den > 0 && std::isfinite(den) ? ab / den : -1.;
}
Trajectory periodicityTrajectory(const std::vector<float> &original, const std::vector<double> &x,
                                 int fs, int fftSize, const std::filesystem::path &directory,
                                 const std::string &name) {
    std::array<Trajectory, 4> candidates;
    for (int method = 0; method < 4; ++method)
        candidates[method] = estimateTrajectory(method, original, x, fs, fftSize, true);
    Trajectory result;
    result.time = candidates[0].time;
    result.f0.resize(result.time.size());
    result.pcmClips = candidates[3].pcmClips;
    result.reaperRaw = candidates[3].reaperRaw;
    result.backendStatus = candidates[3].backendStatus;
    const auto count = result.time.size();
    std::vector<double> periods(count * 4), scores(count * 4, -1.);
    std::vector<int32_t> selected(count, -1), statuses;
    std::vector<double> window(static_cast<std::size_t>(fftSize));
    for (int method = 0; method < 4; ++method) {
        statuses.push_back(candidates[method].backendStatus);
        write(directory / (name + "-candidate-" + std::to_string(method) + ".f64"),
              candidates[method].f0.data(), count * sizeof(double));
    }
    for (std::size_t frame = 0; frame < count; ++frame) {
        const int start =
            std::clamp(static_cast<int>(std::floor(result.time[frame] * fs)) - fftSize / 2, 0,
                       static_cast<int>(original.size()) - fftSize);
        double mean = 0;
        for (int i = 0; i < fftSize; ++i) {
            window[static_cast<std::size_t>(i)] = original[static_cast<std::size_t>(start + i)];
            mean += window[static_cast<std::size_t>(i)];
        }
        mean /= fftSize;
        for (auto &value : window)
            value -= mean;
        double maximum = -1.;
        for (int method = 0; method < 4; ++method) {
            const double frequency = candidates[method].f0[frame];
            if (!std::isfinite(frequency) || frequency < 50 || frequency > 800)
                continue;
            double lag = fs / frequency;
            const auto k = static_cast<int>(std::round(lag));
            const double left = integerNccf(window, k - 1), middle = integerNccf(window, k),
                         right = integerNccf(window, k + 1);
            const double curvature = left - 2 * middle + right;
            if (middle >= left && middle >= right && curvature < 0)
                lag = k + std::clamp(.5 * (left - right) / curvature, -1., 1.);
            const auto pos = frame * 4 + static_cast<std::size_t>(method);
            periods[pos] = lag;
            scores[pos] = fractionalNccf(window, lag);
            maximum = std::max(maximum, scores[pos]);
        }
        double highest = 0.;
        for (int method = 0; method < 4; ++method) {
            const auto pos = frame * 4 + static_cast<std::size_t>(method);
            if (periods[pos] > 0 && scores[pos] >= maximum - 1e-6 && fs / periods[pos] > highest) {
                selected[frame] = method;
                highest = fs / periods[pos];
            }
        }
        if (selected[frame] >= 0 &&
            scores[frame * 4 + static_cast<std::size_t>(selected[frame])] >= .9 && highest >= 50 &&
            highest <= 800)
            result.f0[frame] = highest;
    }
    write(directory / (name + "-periods.f64"), periods.data(), periods.size() * sizeof(double));
    write(directory / (name + "-scores.f64"), scores.data(), scores.size() * sizeof(double));
    write(directory / (name + "-selected.i32"), selected.data(), selected.size() * sizeof(int32_t));
    write(directory / (name + "-backend-status.i32"), statuses.data(),
          statuses.size() * sizeof(int32_t));
    return result;
}

void periodicityControls(const std::filesystem::path &directory) {
    constexpr int fs = 48000, fftSize = 4096, frames = 96000;
    for (const std::string name : {"ideal35", "unchanged70", "evenOnly35", "noise", "silence"}) {
        std::vector<float> original(frames);
        if (name == "noise") {
            uint32_t random = 0x6139f8b7u;
            for (auto &sample : original) {
                random ^= random << 13;
                random ^= random >> 17;
                random ^= random << 5;
                sample = static_cast<float>(.03 * (double(random >> 8) / 8388608. - 1.));
            }
        } else if (name != "silence") {
            const double fundamental = name == "unchanged70" ? 70. : 35.;
            for (int h = 1; h <= 80; ++h) {
                if (name == "evenOnly35" && h % 2)
                    continue;
                const double frequency = h * fundamental;
                const double envelope =
                    std::exp(-.5 * std::pow((frequency - 600) / 100, 2)) + .002 / h;
                for (int i = 0; i < frames; ++i)
                    original[static_cast<std::size_t>(i)] += static_cast<float>(
                        .025 * envelope * std::sin(2 * std::numbers::pi * frequency * i / fs));
            }
        }
        std::vector<double> x(original.begin(), original.end());
        const auto result = periodicityTrajectory(original, x, fs, fftSize, directory, name);
        write(directory / (name + "-input.raw"), original.data(), original.size() * sizeof(float));
        write(directory / (name + "-time.f64"), result.time.data(),
              result.time.size() * sizeof(double));
        write(directory / (name + "-f0.f64"), result.f0.data(), result.f0.size() * sizeof(double));
        write(directory / (name + "-reaper-f0.raw"), result.reaperRaw.data(),
              result.reaperRaw.size() * sizeof(float));
        std::ofstream metadata(directory / (name + "-reaper.txt"));
        metadata << "pcmClips " << result.pcmClips << "\nsourceFrames " << result.reaperRaw.size()
                 << "\nworldFrames " << result.f0.size() << '\n';
        require(bool(metadata), "control metadata write");
        std::printf("control%s frames%zu reaperStatus%d pcmClips%zu\n", name.c_str(),
                    result.f0.size(), result.backendStatus, result.pcmClips);
        std::fflush(stdout);
    }
}

void render(const std::filesystem::path &directory, bool expanded, int selectedMethod = 0) {
    const auto cases = expanded ? retained() : narrow;
    for (int method = (expanded || selectedMethod >= 2) ? selectedMethod : 0;
         method < ((expanded || selectedMethod >= 2) ? selectedMethod + 1 : 2); ++method) {
        for (std::size_t index = 0; index < cases.size(); ++index) {
            const auto started = std::chrono::steady_clock::now();
            const auto c = cases[index];
            const int fs = c.rate;
            const double seconds = c.kind == Signal::Sine ? 1.5 : 2.;
            const int frames = static_cast<int>(seconds * fs);
            const int fftSize = fs <= 48000 ? 4096 : 8192;
            std::vector<float> original(frames);
            if (c.kind == Signal::Sine) {
                for (std::size_t i = 0; i < original.size(); ++i)
                    original[i] =
                        static_cast<float>(.1 * std::sin(2 * std::numbers::pi * c.f0 * i / fs));
            } else {
                for (int h = 1; h <= 80; ++h) {
                    const double f = c.f0 * h;
                    const double envelope =
                        c.kind == Signal::Multi
                            ? std::exp(-.5 * std::pow((f - 600) / 90, 2)) +
                                  .8 * std::exp(-.5 * std::pow((f - 1200) / 140, 2)) +
                                  .5 * std::exp(-.5 * std::pow((f - 2500) / 200, 2)) + .002 / h
                            : std::exp(-.5 * std::pow((f - c.center) / 100, 2)) + .002 / h;
                    for (std::size_t i = 0; i < original.size(); ++i)
                        original[i] += static_cast<float>(
                            .025 * envelope * std::sin(2 * std::numbers::pi * f * i / fs));
                }
            }
            std::vector<double> x(original.begin(), original.end());
            const int count = GetSamplesForDIO(fs, frames, period);
            require(count == GetSamplesForHarvest(fs, frames, period), "frame-grid mismatch");
            const std::string methodName =
                method == 0
                    ? "dio"
                    : (method == 1
                           ? "harvest"
                           : (method == 2 ? "yin" : (method == 3 ? "reaper" : "periodicity")));
            const std::string name =
                (expanded ? (method == 0 ? "retained" : "retained-" + methodName) : methodName) +
                "-" + std::to_string(index);
            auto trajectory = method == 4
                                  ? periodicityTrajectory(original, x, fs, fftSize, directory, name)
                                  : estimateTrajectory(method, original, x, fs, fftSize);
            auto time = trajectory.time, f0 = trajectory.f0;
            std::vector<double> output(frames);
            const auto pcmClips = trajectory.pcmClips, sourceFrames = trajectory.reaperRaw.size();
            if (method >= 3) {
                write(directory / (name + "-reaper-f0.raw"), trajectory.reaperRaw.data(),
                      trajectory.reaperRaw.size() * sizeof(float));
                std::ofstream mapping(directory / (name + "-reaper.txt"));
                mapping << "pcmClips " << pcmClips << "\nsourceFrames " << sourceFrames
                        << "\nworldFrames " << count << '\n';
                require(bool(mapping), "REAPER mapping write failed");
                std::printf("%s pcmClips%zu reaperSourceFrames%zu worldFrames%d firstTimestamp0 "
                            "stepSeconds.005\n",
                            name.c_str(), pcmClips, sourceFrames, count);
            }
            std::vector<double> estimates;
            int observed = 0;
            for (int i = 0; i < count; ++i) {
                require(std::isfinite(f0[i]) && f0[i] >= 0, "invalid estimated F0");
                if (time[i] >= seconds * .5 && time[i] < seconds) {
                    ++observed;
                    if (f0[i] > 0)
                        estimates.push_back(1200 * std::log2(f0[i] / c.f0));
                }
            }
            std::sort(estimates.begin(), estimates.end());
            const double median =
                estimates.empty()
                    ? 1e30
                    : (estimates[estimates.size() / 2] + estimates[(estimates.size() - 1) / 2]) *
                          .5;
            const double coverage = double(estimates.size()) / observed;
            write(directory / (name + "-f0.f64"), f0.data(), f0.size() * sizeof(double));
            write(directory / (name + "-time.f64"), time.data(), time.size() * sizeof(double));
            std::vector<std::vector<double>> spectrum(count, std::vector<double>(fftSize / 2 + 1));
            auto aperiodicity = spectrum;
            auto warped = spectrum;
            std::vector<double *> sp(count), ap(count), wp(count);
            for (int i = 0; i < count; ++i) {
                sp[i] = spectrum[i].data();
                ap[i] = aperiodicity[i].data();
                wp[i] = warped[i].data();
            }
            CheapTrickOption ct{};
            InitializeCheapTrickOption(fs, &ct);
            ct.f0_floor = 50;
            ct.fft_size = fftSize;
            CheapTrick(x.data(), frames, fs, time.data(), f0.data(), count, &ct, sp.data());
            D4COption dc{};
            InitializeD4COption(&dc);
            D4C(x.data(), frames, fs, time.data(), f0.data(), count, fftSize, &dc, ap.data());
            const double formant = std::exp2((c.preserve ? c.formant : c.pitch) / 12);
            for (int i = 0; i < count; ++i) {
                for (int bin = 0; bin <= fftSize / 2; ++bin) {
                    const double pos = std::min(double(fftSize / 2), bin / formant);
                    const int lo = static_cast<int>(pos), hi = std::min(lo + 1, fftSize / 2);
                    warped[i][bin] =
                        spectrum[i][lo] + (pos - lo) * (spectrum[i][hi] - spectrum[i][lo]);
                }
                f0[i] *= std::exp2(c.pitch / 12);
            }
            Synthesis(f0.data(), count, wp.data(), ap.data(), fftSize, period, fs, frames,
                      output.data());
            std::vector<float> converted(frames);
            bool nonzero = false;
            for (int i = 0; i < frames; ++i) {
                converted[i] = static_cast<float>(output[i]);
                require(std::isfinite(converted[i]), "nonfinite output");
                nonzero |= converted[i] != 0;
            }
            require(nonzero, "zero output");
            write(directory / (name + ".raw"), converted.data(), converted.size() * sizeof(float));
            write(directory / (name + "-input.raw"), original.data(),
                  original.size() * sizeof(float));
            std::ofstream metadata(directory / (name + ".txt"));
            metadata.precision(17);
            metadata << "voicedCoverage " << coverage << "\nmedianCents " << median << '\n';
            require(bool(metadata), "metadata write failed");
            std::printf(
                "%s frames%d inputVoicedCoverage%.9f inputMedianCents%.9f finite1 nonzero1\n",
                name.c_str(), frames, coverage, median);
            std::printf(
                "%s offlineWallSeconds%.6f\n", name.c_str(),
                std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count());
            std::fflush(stdout);
        }
    }
}
} // namespace
int main(int argc, char **argv) {
    static_assert(std::endian::native == std::endian::little && sizeof(float) == 4);
    try {
        if (argc == 2 && std::string(argv[1]) == "--fft-self-test") {
            selfTest();
        } else if (argc == 2 && std::filesystem::is_directory(argv[1])) {
            selfTest();
            render(argv[1], false);
        } else if (argc == 3 && std::string(argv[1]) == "--periodicity-controls" &&
                   std::filesystem::is_directory(argv[2])) {
            selfTest();
            periodicityControls(argv[2]);
        } else if (argc == 4 && std::string(argv[1]) == "--suite" &&
                   (std::string(argv[2]) == "retained" ||
                    std::string(argv[2]) == "retained-harvest" ||
                    std::string(argv[2]) == "retained-yin" ||
                    std::string(argv[2]) == "narrow-yin" ||
                    std::string(argv[2]) == "retained-reaper" ||
                    std::string(argv[2]) == "narrow-reaper" ||
                    std::string(argv[2]) == "retained-periodicity" ||
                    std::string(argv[2]) == "narrow-periodicity") &&
                   std::filesystem::is_directory(argv[3])) {
            selfTest();
            const std::string suite = argv[2];
            const int method =
                suite == "retained"
                    ? 0
                    : (suite == "retained-harvest"
                           ? 1
                           : ((suite == "retained-yin" || suite == "narrow-yin")
                                  ? 2
                                  : ((suite == "retained-reaper" || suite == "narrow-reaper")
                                         ? 3
                                         : 4)));
            render(argv[3], suite.starts_with("retained"), method);
        } else {
            std::fprintf(
                stderr,
                "usage: world_probe --fft-self-test | existing-output-directory | "
                "--suite "
                "retained|retained-harvest|retained-yin|narrow-yin|retained-reaper|narrow-reaper|"
                "retained-periodicity|narrow-periodicity existing-output-directory | "
                "--periodicity-controls existing-output-directory\n");
            return 2;
        }
    } catch (const std::exception &e) {
        watching.store(false);
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
