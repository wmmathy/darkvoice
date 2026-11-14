#include "../src/BandlimitedColourOversampling.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <darkvoice/HarmonicCharacter.hpp>
#include <limits>
#include <new>
#include <numbers>
#include <string_view>
#include <vector>
namespace {
darkvoice::QualityMode mode = darkvoice::QualityMode::Live;
std::size_t latency = 124, boundaryTaps = 193, higherTaps = 65, referenceDelta = 2;
struct Fx : darkvoice::HarmonicCharacter {
    bool prepare(double rate, std::size_t frames, std::size_t channels) noexcept {
        return HarmonicCharacter::prepare(rate, frames, channels, mode);
    }
};
std::atomic<std::size_t> allocations{0};
std::atomic<bool> reject{false};
int checks = 0, failures = 0;
void check(bool pass, const char *message) {
    ++checks;
    if (!pass) {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", message);
    }
}
bool render(Fx &fx, std::vector<float> &left, std::vector<float> *right, std::size_t block) {
    for (std::size_t at = 0; at < left.size(); at += block) {
        std::array<float *, 2> ptr{left.data() + at, right ? right->data() + at : nullptr};
        if (!fx.process(std::span<float *>(ptr.data(), right ? 2U : 1U),
                        std::min(block, left.size() - at)))
            return false;
    }
    return true;
}
std::vector<float> signal(std::size_t count) {
    std::vector<float> data(count);
    for (std::size_t i = 0; i < count; ++i)
        data[i] = static_cast<float>(.2 * std::sin(.103 * i) + .07 * std::cos(.037 * i));
    return data;
}
double energy(const std::vector<float> &x, std::size_t begin = 0) {
    double sum = 0;
    for (std::size_t i = begin; i < x.size(); ++i)
        sum += static_cast<double>(x[i]) * x[i];
    return sum;
}
// Independent reference: explicit zero insertion, generic causal convolution,
// and even-phase sample selection. No production resampling helper is called.
std::vector<double> referenceKernel(std::size_t taps, bool guard = false) {
    std::vector<double> h(taps);
    const int half = static_cast<int>((taps - 1) / 2);
    for (int j = -half; j <= half; ++j) {
        const double phase = 2 * std::numbers::pi * (j + half) / static_cast<double>(taps - 1);
        const double window = .42 - .5 * std::cos(phase) + .08 * std::cos(2 * phase);
        h[static_cast<std::size_t>(j + half)] =
            guard ? (j == 0 ? .45 : std::sin(.45 * std::numbers::pi * j) / (std::numbers::pi * j)) *
                        window
            : j == 0     ? .5
            : j % 2 == 0 ? 0
                         : std::sin(.5 * std::numbers::pi * j) * window / (std::numbers::pi * j);
    }
    double total = 0;
    for (double x : h)
        total += x;
    for (auto &x : h)
        x /= total;
    return h;
}
std::vector<double> convolve(const std::vector<double> &x, const std::vector<double> &h) {
    std::vector<double> result(x.size());
    for (std::size_t tap = 0; tap < h.size(); ++tap)
        if (h[tap] != 0)
            for (std::size_t i = tap; i < x.size(); ++i)
                result[i] += h[tap] * x[i - tap];
    return result;
}
std::vector<double> reference(const std::vector<float> &source, double rate, float amount,
                              int stages) {
    std::vector<double> x(source.begin(), source.end());
    for (int stage = 0; stage < stages; ++stage) {
        auto h = referenceKernel(stage == 0 ? boundaryTaps : higherTaps, stage == 0);
        std::vector<double> inserted(x.size() * 2);
        for (std::size_t i = 0; i < x.size(); ++i)
            inserted[2 * i] = 2 * x[i];
        x = convolve(inserted, h);
    }
    const double normalization = std::tanh(2.);
    for (auto &v : x) {
        const double z = std::tanh(2 * v) / normalization;
        v = amount < 0 ? z * z * z : z * z;
    }
    for (int stage = 0; stage < stages; ++stage) {
        auto filtered = convolve(x, referenceKernel(stage == stages - 1 ? boundaryTaps : higherTaps,
                                                    stage == stages - 1));
        std::vector<double> selected(x.size() / 2);
        for (std::size_t i = 0; i < selected.size(); ++i)
            selected[i] = filtered[2 * i];
        x = std::move(selected);
    }
    double previous = 0, dc = 0;
    const double r = std::exp(-2 * std::numbers::pi * 10 / rate);
    for (auto &v : x) {
        dc = v - previous + r * dc;
        previous = v;
        v = .25 * std::abs(amount) * dc;
    }
    return x;
}
void filters() {
    for (const auto taps : {boundaryTaps, higherTaps}) {
        const bool guard = taps == boundaryTaps;
        const auto h = guard ? darkvoice::bandlimited_colour_detail::guardKernel(taps)
                             : darkvoice::bandlimited_colour_detail::kernel(taps);
        const auto independent = referenceKernel(taps, guard);
        bool symmetric = true, zeros = true;
        double sum = 0, pass = 0, stop = 0;
        for (std::size_t i = 0; i < taps; ++i) {
            symmetric = symmetric && std::abs(h[i] - h[taps - 1 - i]) < 1e-15 &&
                        std::abs(h[i] - independent[i]) < 1e-15;
            zeros = zeros && (guard || i == (taps - 1) / 2 || i % 2 != 0 || h[i] == 0);
            sum += h[i];
        }
        check(symmetric && zeros && std::abs(sum - 1) < 1e-14,
              "independent guarded/higher kernels, DC/symmetry and halfband structural zeros");
        for (int bin = 0; bin <= 4096; ++bin) {
            const double f = .5 * bin / 4096;
            std::complex<double> response{};
            for (std::size_t j = 0; j < taps; ++j)
                response += h[j] * std::exp(std::complex<double>{0, -2 * std::numbers::pi * f * j});
            if (f <= (guard ? .175 : .2))
                pass = std::max(pass, std::abs(std::abs(response) - 1));
            if (f >= (guard ? .275 : .3))
                stop = std::max(stop, std::abs(response));
        }
        std::printf("Kernel %zu pass %.9g stop %.3fdB\n", taps, pass, 20 * std::log10(stop));
        check(pass < .0002, "per-stage passband error below .0002");
        check(20 * std::log10(stop) < -75, "per-stage stopband below -75dB");
    }
    darkvoice::bandlimited_colour_detail::Oversampler16 resampler;
    const auto guard = darkvoice::bandlimited_colour_detail::guardKernel(boundaryTaps);
    resampler.prepare(guard, darkvoice::bandlimited_colour_detail::kernel(higherTaps), guard);
    std::vector<double> impulse(2 * latency + 1);
    for (std::size_t i = 0; i < impulse.size(); ++i)
        impulse[i] = resampler.process(i == 0 ? 1 : 0, [](double x) { return x; });
    const auto peak = static_cast<std::size_t>(std::max_element(impulse.begin(), impulse.end()) -
                                               impulse.begin());
    bool symmetric = true;
    for (std::size_t i = 0; i < impulse.size(); ++i)
        symmetric = symmetric && std::abs(impulse[i] - impulse[2 * latency - i]) < 1e-14;
    check(peak == latency && symmetric,
          "measured identity resampler linear phase and mode latency");
    resampler.reset();
    double settled = 0;
    for (std::size_t i = 0; i < 4 * latency; ++i)
        settled = resampler.process(1, [](double x) { return x; });
    check(std::abs(settled - 1) < 1e-7, "identity resampler settled DC unity");
}
void aliasMatrix() {
    constexpr std::size_t period = 4096, warm = 4096, samples = warm + period + 1024;
    double worst = -1000, least = 1000;
    for (double rate : {44100., 48000., 88200., 96000.})
        for (float amount : {-1.f, -.5f, .5f, 1.f})
            for (float amplitude : {.15f, .5f})
                for (int fixture = 0; fixture < 3; ++fixture) {
                    std::vector<float> source(samples);
                    for (std::size_t i = 0; i < samples; ++i) {
                        double phase = 2 * std::numbers::pi * i / period;
                        source[i] = static_cast<float>(
                            amplitude * (fixture == 0   ? std::sin(719 * phase)
                                         : fixture == 1 ? std::sin(1597 * phase)
                                                        : .5 * (std::sin(683 * phase) +
                                                                std::sin(1129 * phase))));
                    }
                    const auto high = reference(source, rate, amount, 5),
                               native = reference(source, rate, amount, 0);
                    auto actual = source;
                    Fx fx;
                    check(fx.prepare(rate, 257, 1) && fx.setParameters({amount}),
                          "reference matrix setup");
                    fx.reset();
                    check(render(fx, actual, nullptr, 257), "reference matrix render");
                    double error8 = 0, error1 = 0, power = 0;
                    for (std::size_t j = 0; j < period; ++j) {
                        auto at = warm + latency + j;
                        double e8 = actual[at] - high[at + referenceDelta],
                               e1 = native[at - latency] - high[at + referenceDelta];
                        error8 += e8 * e8;
                        error1 += e1 * e1;
                        power += static_cast<double>(source[warm + j]) * source[warm + j];
                    }
                    double residual = 10 * std::log10(error8 / power),
                           improvement = 10 * std::log10(error1 / error8);
                    worst = std::max(worst, residual);
                    least = std::min(least, improvement);
                    std::printf("Reference %.0f amount%.1f amp%.2f fixture%d residual%.6f "
                                "improvement%.6f\n",
                                rate, amount, amplitude, fixture, residual, improvement);
                    check(residual <= -55, "reference residual <=-55dB/inputRMS");
                    check(improvement >= 20, "reference improvement >=20dB vs1x");
                }
    std::printf("Reference worst %.6fdB least improvement %.6fdB\n", worst, least);
}
// Independent full-complex rectangular FFT; neither production kernels nor a
// high-rate filtered reference determines the allowed absolute harmonic mask.
std::vector<std::complex<double>> spectrum(const std::vector<float> &x, std::size_t begin,
                                           std::size_t n) {
    std::vector<std::complex<double>> z(n);
    for (std::size_t i = 0; i < n; ++i)
        z[i] = x[begin + i];
    for (std::size_t i = 1, j = 0; i < n; ++i) {
        std::size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(z[i], z[j]);
    }
    for (std::size_t width = 2; width <= n; width *= 2) {
        const auto w = std::exp(std::complex<double>{0, -2 * std::numbers::pi / width});
        for (std::size_t at = 0; at < n; at += width) {
            std::complex<double> phase = 1;
            for (std::size_t j = 0; j < width / 2; ++j) {
                const auto u = z[at + j], v = phase * z[at + j + width / 2];
                z[at + j] = u + v;
                z[at + j + width / 2] = u - v;
                phase *= w;
            }
        }
    }
    return z;
}
double offMaskPower(const std::vector<std::complex<double>> &bins, std::size_t fundamental) {
    const auto n = bins.size();
    double power = 0;
    for (std::size_t bin = 1; bin <= n / 2; ++bin)
        if (bin % fundamental != 0)
            power += (bin == n / 2 ? 1 : 2) * std::norm(bins[bin]) / (n * n);
    return power;
}
void absoluteAlias(int group = 0) {
    constexpr std::size_t n = 4096;
    // Parseval and special-bin calibration prevent DC/Nyquist weight mistakes.
    for (int fixture = 0; fixture < 3; ++fixture) {
        std::vector<float> x(n);
        for (std::size_t i = 0; i < n; ++i)
            x[i] = fixture == 0   ? 1
                   : fixture == 1 ? (i % 2 ? -1 : 1)
                                  : static_cast<float>(std::sin(2 * std::numbers::pi * 17 * i / n));
        const auto bins = spectrum(x, 0, n);
        double power = 0;
        for (const auto &z : bins)
            power += std::norm(z) / (n * n);
        check(std::abs(power - energy(x) / n) < 1e-12,
              "absolute analyser full-FFT Parseval/DC/Nyquist calibration");
    }
    // A folded third harmonic of bin719 is bin1939; it must be counted,
    // while a genuine below-Nyquist second harmonic (1438) must be allowed.
    for (std::size_t bin : {0U, 1438U, 1939U, 2048U}) {
        std::vector<float> x(n);
        for (std::size_t i = 0; i < n; ++i)
            x[i] = static_cast<float>(std::cos(2 * std::numbers::pi * bin * i / n));
        const double off = offMaskPower(spectrum(x, 0, n), 719);
        check(bin == 0 || bin == 1438 ? off < 1e-12 : std::abs(off - (bin == 2048 ? 1 : .5)) < 1e-7,
              "absolute mask excludes DC, accepts true harmonic, counts folded harmonic and "
              "Nyquist correctly");
    }
    const std::vector<std::size_t> fundamentals =
        group == 0   ? std::vector<std::size_t>{719, 1597}
        : group == 1 ? std::vector<std::size_t>{683, 1025}
                     : std::vector<std::size_t>{1720, 1721, 1722, 1723, 1724};
    double worst = -1000;
    for (double rate : {44100., 48000., 88200., 96000.})
        for (float amount : {-1.f, -.5f, .5f, 1.f})
            for (float amplitude : {.15f, .5f})
                for (std::size_t fundamental : fundamentals) {
                    const auto warm = static_cast<std::size_t>(std::ceil(rate / n)) * n;
                    std::vector<float> source(warm + n);
                    for (std::size_t i = 0; i < source.size(); ++i)
                        source[i] = static_cast<float>(
                            amplitude * std::sin(2 * std::numbers::pi * fundamental * (i % n) / n));
                    const auto inputBins = spectrum(source, warm, n);
                    const double inputPower = energy(source, warm) / n;
                    double unwantedInput = 0;
                    for (std::size_t bin = 0; bin < n; ++bin)
                        if (bin != fundamental && bin != n - fundamental)
                            unwantedInput += std::norm(inputBins[bin]) / (n * n);
                    check(unwantedInput / inputPower < 1e-12 &&
                              std::abs(inputPower / (amplitude * amplitude * .5) - 1) < 1e-6,
                          "actual absolute-fixture input is nominal RMS and declared single "
                          "frequency");
                    auto output = source;
                    Fx fx;
                    check(fx.prepare(rate, 257, 1) && fx.setParameters({amount}),
                          "absolute alias setup");
                    fx.reset();
                    check(render(fx, output, nullptr, 257) &&
                              std::all_of(output.begin(), output.end(),
                                          [](float v) { return std::isfinite(v); }),
                          "absolute alias finite public output render");
                    const auto bins = spectrum(output, warm, n);
                    const double off = offMaskPower(bins, fundamental);
                    const double db = 10 * std::log10(off / inputPower);
                    worst = std::max(worst, db);
                    std::printf(
                        "Absolute %.0fHz amount%.1f amp%.2f bin%zu off-mask %.6fdB/inputRMS\n",
                        rate, amount, amplitude, fundamental, db);
                    check(db <= -55, "absolute off-mask energy <= -55dB/inputRMS");
                }
    std::printf("Worst absolute group%d alias %.6fdB/inputRMS\n", group, worst);
}
// Supplemental uniform scan, predeclared before guarded-bank rendering. All bins
// remain reported; near-input-Nyquist image mixing must not hide behind the
// final lowpass or a same-filter reference comparison.
void uniformScan(std::size_t first = 17, std::size_t last = 2031) {
    constexpr std::size_t n = 4096, warm = 12 * n;
    constexpr float amplitude = .5f;
    double worst = -1000;
    std::size_t worstBin = 0, caseCount = 0;
    float worstAmount = 0;
    const auto started = std::chrono::steady_clock::now();
    for (std::size_t fundamental = first; fundamental <= last; ++fundamental) {
        std::vector<float> source(warm + n);
        for (std::size_t i = 0; i < source.size(); ++i)
            source[i] = static_cast<float>(
                amplitude * std::sin(2 * std::numbers::pi * fundamental * (i % n) / n));
        const auto inputBins = spectrum(source, warm, n);
        const double inputPower = energy(source, warm) / n;
        double unwanted = 0;
        for (std::size_t bin = 0; bin < n; ++bin)
            if (bin != fundamental && bin != n - fundamental)
                unwanted += std::norm(inputBins[bin]) / (n * n);
        check(unwanted / inputPower < 1e-12 &&
                  std::abs(inputPower / (amplitude * amplitude * .5) - 1) < 1e-6,
              "scan actual input frequency/RMS");
        for (float amount : {-1.f, 1.f}) {
            auto actual = source;
            Fx fx;
            check(fx.prepare(48000, 257, 1) && fx.setParameters({amount}), "uniform scan setup");
            fx.reset();
            check(render(fx, actual, nullptr, 257) &&
                      std::all_of(actual.begin(), actual.end(),
                                  [](float v) { return std::isfinite(v); }),
                  "uniform scan finite render");
            const double off = offMaskPower(spectrum(actual, warm, n), fundamental);
            const double db = 10 * std::log10(off / inputPower);
            if (db > worst) {
                worst = db;
                worstBin = fundamental;
                worstAmount = amount;
            }
            std::printf("Scan48k amount%.1f bin%zu absolute%.9fdB/inputRMS\n", amount, fundamental,
                        db);
            check(db <= -55, "uniform scan absolute <=-55dB/inputRMS");
            ++caseCount;
        }
    }
    check(caseCount == 2 * (last - first + 1),
          "scan includes both signs for every bin in declared inclusive range");
    std::printf("Scan range%zu..%zu worst%.9fdB bin%zu amount%.1f cases%zu wall%.6fseconds(no "
                "deadline claim)\n",
                first, last, worst, worstBin, worstAmount, caseCount,
                std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count());
}
double harmonic(const std::vector<float> &x, std::size_t begin, int bin) {
    std::complex<double> result{};
    const auto n = x.size() - begin;
    for (std::size_t i = 0; i < n; ++i)
        result += static_cast<double>(x[begin + i]) *
                  std::exp(std::complex<double>{0, -2 * std::numbers::pi * bin * i / n});
    return 2 * std::abs(result) / n;
}
void validation() {
    Fx fx;
    std::array<float, 8> l{}, r{};
    std::array<float *, 2> ptr{l.data(), r.data()};
    check(!fx.qualityMode() && fx.latencySamples() == 0 && !fx.process(ptr, 8), "unprepared");
    for (double rate : {0., 22050., std::numeric_limits<double>::quiet_NaN()})
        check(!fx.prepare(rate, 8, 2), "unsupported rate");
    check(!fx.prepare(48000, 0, 2) && !fx.prepare(48000, 65537, 2) && !fx.prepare(48000, 8, 0) &&
              !fx.prepare(48000, 8, 3),
          "capacity/layout bounds");
    reject = true;
    bool ok = fx.prepare(48000, 8, 2);
    reject = false;
    check(!ok && !fx.qualityMode(), "prepare allocation failure");
    check(fx.prepare(48000, 8, 2) && fx.qualityMode() == mode && fx.latencySamples() == latency,
          "prepare/mode/latency");
    std::array<float *, 2> null{l.data(), nullptr}, alias{l.data(), l.data()};
    l.fill(.125f);
    const auto original = l;
    check(!fx.process(null, 8) && l == original, "null atomic");
    check(!fx.process(alias, 8) && l == original, "alias atomic");
    check(!fx.process(ptr, 9) && l == original, "capacity atomic");
    check(!fx.process(std::span<float *>(ptr.data(), 1), 8), "wrong layout");
    check(fx.process(null, 0), "empty ignores pointers");
    for (float bad : {-1.001f, 1.001f, std::numeric_limits<float>::quiet_NaN(),
                      std::numeric_limits<float>::infinity()})
        check(!fx.setParameters({bad}), "invalid amount rejected");
    check(!fx.HarmonicCharacter::prepare(48000, 8, 2, static_cast<darkvoice::QualityMode>(3)) &&
              !fx.qualityMode() && fx.latencySamples() == 0,
          "invalid mode unprepares");
    darkvoice::HarmonicCharacter defaults;
    check(defaults.prepare(48000, 1, 1) && defaults.qualityMode() == darkvoice::QualityMode::Live &&
              defaults.latencySamples() == 124,
          "default mode");
    check(defaults.prepare(96000, 65536, 2, mode), "maximum capacity");
}
void behavior(double rate, std::size_t channels) {
    Fx a, b;
    check(a.prepare(rate, 257, channels) && b.prepare(rate, 257, channels), "behavior setup");
    auto left = signal(4096), right = left, source = left;
    check(render(a, left, channels == 2 ? &right : nullptr, 257) &&
              std::all_of(left.begin(), left.end(),
                          [](float x) { return x == 0 && !std::signbit(x); }),
          "default exact positive zero");
    check(a.setParameters({-.8f}) && b.setParameters({-.8f}), "amount accepts");
    a.reset();
    b.reset();
    left = source;
    right = source;
    auto other = source, otherRight = source;
    check(render(a, left, channels == 2 ? &right : nullptr, 257) &&
              render(b, other, channels == 2 ? &otherRight : nullptr, 13) && left == other &&
              (channels == 1 || right == otherRight),
          "partition bit identity");
    a.reset();
    other = source;
    otherRight = source;
    check(render(a, other, channels == 2 ? &otherRight : nullptr, 257) && left == other,
          "reset deterministic settled target");
    // Full-stream amount1 renders provide independent native-frame weight oracle.
    Fx negative, positive, automated;
    check(negative.prepare(rate, 257, 1) && positive.prepare(rate, 257, 1) &&
              automated.prepare(rate, 257, 1),
          "automation setup");
    negative.setParameters({-1});
    positive.setParameters({1});
    negative.reset();
    positive.reset();
    auto n = source, p = source, actual = source;
    check(render(negative, n, nullptr, 257) && render(positive, p, nullptr, 257),
          "automation endpoint renders");
    const double alpha = 1 - std::exp(-1 / (.010 * rate));
    double wn = 0, wp = 0, worst = 0;
    const auto step = [&](double &w, double target) {
        w += alpha * (target - w);
        if (std::abs(w - target) < 1e-7)
            w = target;
    };
    for (std::size_t i = 0; i < actual.size(); ++i) {
        float target = i < 1000 ? -.7f : i < 2700 ? .9f : 0.f;
        automated.setParameters({target});
        std::array<float *, 1> ptr{actual.data() + i};
        check(automated.process(ptr, 1), "automation one frame");
        step(wn, std::max(-static_cast<double>(target), 0.));
        step(wp, std::max(static_cast<double>(target), 0.));
        worst = std::max(worst, std::abs(actual[i] - (wn * n[i] + wp * p[i])));
    }
    check(worst < 3e-8, "independent smoothed weight oracle");
    a.reset();
    left.assign(1024, std::numeric_limits<float>::max());
    right = left;
    for (std::size_t i = 0; i < left.size(); ++i)
        left[i] = i % 4 == 0   ? std::numeric_limits<float>::quiet_NaN()
                  : i % 4 == 1 ? std::numeric_limits<float>::infinity()
                  : i % 4 == 2 ? std::numeric_limits<float>::denorm_min()
                               : -std::numeric_limits<float>::max();
    auto before = allocations.load();
    const bool processed = render(a, left, channels == 2 ? &right : nullptr, 257);
    auto after = allocations.load();
    check(processed && before == after, "allocation-free process");
    check(
        std::all_of(left.begin(), left.end(),
                    [](float x) { return std::isfinite(x) && std::fpclassify(x) != FP_SUBNORMAL; }),
        "extreme/nonfinite/subnormal safety");
    if (channels == 2) {
        a.reset();
        left = source;
        right.assign(source.size(), 0);
        check(render(a, left, &right, 257) &&
                  std::all_of(right.begin(), right.end(), [](float x) { return x == 0; }),
              "channel isolation");
    }
    // Empty process and invalid snapshots cannot alter active stream state.
    a.reset();
    b.reset();
    left = source;
    right = source;
    other = source;
    otherRight = source;
    std::array<float *, 2> empty{nullptr, nullptr};
    a.process(std::span<float *>(empty.data(), channels), 0);
    a.setParameters({2});
    check(render(a, left, channels == 2 ? &right : nullptr, 257) &&
              render(b, other, channels == 2 ? &otherRight : nullptr, 257) && left == other,
          "empty freeze and invalid snapshot atomic");
    a.setParameters({1});
    a.reset();
    left.assign(static_cast<std::size_t>(rate), .5f);
    right = left;
    check(render(a, left, channels == 2 ? &right : nullptr, 257) && std::abs(left.back()) < 1e-12,
          "DC rejected after one second");
    const std::size_t dcBegin = 4 * latency + 512, dcEnd = dcBegin + 1024;
    const double decay = std::exp(-2 * std::numbers::pi * 10 * 1024 / rate);
    check(left[dcBegin] > 0 && std::abs(left[dcEnd] / left[dcBegin] - decay) < 5e-6,
          "independent native10Hz DC tail exponential after finite FIR step settles");
    a.setParameters({0});
    left = signal(static_cast<std::size_t>(rate));
    right = left;
    check(render(a, left, channels == 2 ? &right : nullptr, 257) &&
              std::all_of(left.end() - 1024, left.end(),
                          [](float x) { return x == 0 && !std::signbit(x); }),
          "neutral transition settles exact positive zero without freezing histories");
    check(a.prepare(rate, 257, channels), "reprepare default");
    left = source;
    right = source;
    check(render(a, left, channels == 2 ? &right : nullptr, 257) &&
              std::all_of(left.begin(), left.end(), [](float x) { return x == 0; }),
          "reprepare neutral clears histories and targets");
}
void parity() {
    // Harmonic purity is a steady-state measurement. One coherent second
    // settles the native10Hz DC blocker; startup is tested separately.
    constexpr std::size_t n = 4096, warm = 12 * n;
    constexpr int bin = 19;
    std::vector<float> source(n + warm);
    for (std::size_t i = 0; i < source.size(); ++i)
        source[i] = static_cast<float>(.5 * std::sin(2 * std::numbers::pi * bin * (i % n) / n));
    for (float amount : {-1.f, 1.f}) {
        auto plus = source, minus = source;
        for (auto &v : minus)
            v = -v;
        Fx a, b;
        a.prepare(48000, 257, 1);
        b.prepare(48000, 257, 1);
        a.setParameters({amount});
        b.setParameters({amount});
        a.reset();
        b.reset();
        check(render(a, plus, nullptr, 257) && render(b, minus, nullptr, 257), "parity renders");
        double worst = 0;
        for (std::size_t i = 0; i < plus.size(); ++i)
            worst = std::max(worst, std::abs(static_cast<double>(plus[i]) -
                                             (amount < 0 ? -minus[i] : minus[i])));
        check(worst == 0, "exact odd/even input inversion parity");
        const double h2 = harmonic(plus, warm, 2 * bin), h3 = harmonic(plus, warm, 3 * bin);
        std::printf("Parity amount%.1f H2 %.12g H3 %.12g\n", amount, h2, h3);
        check(amount < 0 ? h3 > 1e-3 && h2 < 1e-7 : h2 > 1e-3 && h3 < 1e-7,
              "odd H3/even H2 contrast");
    }
}
void boundsAndCost() {
    // Conservative L1 product through all four decimators and the DC blocker.
    double l1 = 1;
    for (auto taps : {higherTaps, higherTaps, higherTaps, boundaryTaps}) {
        double sum = 0;
        for (double v : referenceKernel(taps, taps == boundaryTaps))
            sum += std::abs(v);
        l1 *= sum;
    }
    // |odd| <= coth(2)^3 and |even| <= coth(2)^2. DC has L1=2.
    // Independent smoothing targets satisfy n+p<=1; allow both1e-7 endpoint
    // snaps and floating-point rounding conservatively in the peak bound.
    const double bound = .5 * std::pow(1 / std::tanh(2.), 3) * l1 * 1.0000003;
    for (double rate : {44100., 48000., 88200., 96000.})
        for (float amount : {-1.f, 1.f}) {
            Fx fx;
            fx.prepare(rate, 257, 2);
            fx.setParameters({amount});
            fx.reset();
            auto l = signal(static_cast<std::size_t>(rate)), r = l;
            l[0] = std::numeric_limits<float>::max();
            r[0] = -l[0];
            const auto begin = std::chrono::steady_clock::now();
            check(render(fx, l, &r, 257), "cost render");
            const double seconds =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
            double peak = 0;
            for (float v : l)
                peak = std::max(peak, std::abs(static_cast<double>(v)));
            for (float v : r)
                peak = std::max(peak, std::abs(static_cast<double>(v)));
            check(peak <= bound, "conservative bounded-shape/downsample/DC peak bound");
            std::printf(
                "Bound %.9g rate%.0f measured peak %.9g amount%.1f stereo audio1sec wall %.6f (no "
                "deadline claim)\n",
                bound, rate, peak, amount, seconds);
        }
}
} // namespace
#if defined(__GNUC__) || defined(__clang__)
#define DV_NOINLINE __attribute__((noinline))
#else
#define DV_NOINLINE
#endif
DV_NOINLINE void *operator new(std::size_t size) {
    ++allocations;
    if (reject)
        throw std::bad_alloc();
    if (void *value = std::malloc(size == 0 ? 1 : size))
        return value;
    throw std::bad_alloc();
}
DV_NOINLINE void *operator new[](std::size_t size) {
    return ::operator new(size);
}
DV_NOINLINE void operator delete(void *p) noexcept {
    std::free(p);
}
DV_NOINLINE void operator delete[](void *p) noexcept {
    std::free(p);
}
DV_NOINLINE void operator delete(void *p, std::size_t) noexcept {
    std::free(p);
}
DV_NOINLINE void operator delete[](void *p, std::size_t) noexcept {
    std::free(p);
}
#undef DV_NOINLINE
int main(int argc, char **argv) {
    std::vector<darkvoice::QualityMode> modes{darkvoice::QualityMode::Live,
                                              darkvoice::QualityMode::Studio,
                                              darkvoice::QualityMode::Offline};
    bool edgeOnly = false, scanOnly = false, coreOnly = false, safetyOnly = false;
    std::size_t scanFirst = 17, scanLast = 2031;
    const auto parseBin = [](std::string_view value, std::size_t &bin) {
        const auto result = std::from_chars(value.data(), value.data() + value.size(), bin);
        return !value.empty() && result.ec == std::errc{} &&
               result.ptr == value.data() + value.size() && bin >= 17 && bin <= 2031;
    };
    for (int argument = 1; argument < argc; ++argument) {
        const std::string_view flag(argv[argument]);
        if (flag == "--scan-range") {
            if (argument + 2 >= argc || !parseBin(argv[argument + 1], scanFirst) ||
                !parseBin(argv[argument + 2], scanLast) || scanFirst > scanLast)
                return 2;
            argument += 2;
            scanOnly = true;
            continue;
        }
        if (flag == "--edge-only") {
            edgeOnly = true;
            continue;
        }
        if (flag == "--scan-only" || flag == "--scan") {
            scanOnly = true;
            continue;
        }
        if (flag == "--safety") {
            safetyOnly = true;
            continue;
        }
        if (flag == "--core-only" || flag == "--core") {
            coreOnly = true;
            continue;
        }
        if (flag != "--quality" || argument + 1 >= argc)
            return 2;
        const std::string_view name(argv[++argument]);
        if (name == "live")
            modes = {darkvoice::QualityMode::Live};
        else if (name == "studio")
            modes = {darkvoice::QualityMode::Studio};
        else if (name == "offline")
            modes = {darkvoice::QualityMode::Offline};
        else
            return 2;
    }
    if (static_cast<int>(edgeOnly) + static_cast<int>(scanOnly) + static_cast<int>(coreOnly) +
            static_cast<int>(safetyOnly) >
        1)
        return 2;
    for (auto selected : modes) {
        const auto started = std::chrono::steady_clock::now();
        mode = selected;
        latency = darkvoice::HarmonicCharacter::latencyForQuality(mode);
        boundaryTaps = mode == darkvoice::QualityMode::Live     ? 193
                       : mode == darkvoice::QualityMode::Studio ? 257
                                                                : 513;
        higherTaps = mode == darkvoice::QualityMode::Live     ? 65
                     : mode == darkvoice::QualityMode::Studio ? 129
                                                              : 257;
        referenceDelta = mode == darkvoice::QualityMode::Live     ? 2
                         : mode == darkvoice::QualityMode::Studio ? 4
                                                                  : 8;
        std::printf("Character quality%d latency%zu\n", static_cast<int>(mode), latency);
        if (scanOnly) {
            uniformScan(scanFirst, scanLast);
            continue;
        }
        if (edgeOnly) {
            absoluteAlias(1);
            continue;
        }
        validation();
        if (!safetyOnly)
            filters();
        for (double rate : {44100., 48000., 88200., 96000.})
            for (std::size_t channels : {1U, 2U})
                behavior(rate, channels);
        if (!safetyOnly) {
            parity();
            aliasMatrix();
            absoluteAlias();
            absoluteAlias(1);
            absoluteAlias(2);
            boundsAndCost();
            if (!coreOnly)
                uniformScan();
        }
        std::printf(
            "Character mode%d scope%s wall%.6fseconds\n", static_cast<int>(mode),
            safetyOnly ? "safety"
            : coreOnly ? "core"
                       : "all",
            std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count());
    }
    std::printf("HarmonicCharacter checks %d failures %d\n", checks, failures);
    return failures ? 1 : 0;
}
