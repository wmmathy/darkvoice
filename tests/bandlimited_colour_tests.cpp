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
#include <darkvoice/BandlimitedColour.hpp>
#include <limits>
#include <new>
#include <numbers>
#include <string_view>
#include <vector>
namespace {
darkvoice::QualityMode mode = darkvoice::QualityMode::Live;
std::size_t latency = 124, boundaryTaps = 193, higherTaps = 65, referenceDelta = 2;
struct Fx : darkvoice::BandlimitedColour {
    bool prepare(double rate, std::size_t frames, std::size_t channels) noexcept {
        return BandlimitedColour::prepare(rate, frames, channels, mode);
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
std::vector<double> reference(const std::vector<float> &source, double rate, int model, float drive,
                              float tone, int stages, bool emphasis = true) {
    const double a = std::exp(-2 * std::numbers::pi * 1500 / rate);
    const double r = std::exp(-2 * std::numbers::pi * 10 / rate);
    const double pole = std::exp(-2 * std::numbers::pi * tone / rate);
    const double d = 1 + 5 * static_cast<double>(drive), k = model == 0 && emphasis ? .5 : 0;
    const double normalization = std::tanh(d), offset = std::tanh(.12 * d);
    std::vector<double> x(source.size());
    double pre = 0;
    for (std::size_t i = 0; i < x.size(); ++i) {
        pre = a * pre + (1 - a) * source[i];
        x[i] = source[i] + k * (source[i] - pre);
    }
    for (int stage = 0; stage < stages; ++stage) {
        const auto h = referenceKernel(stage == 0 ? boundaryTaps : higherTaps, stage == 0);
        std::vector<double> inserted(2 * x.size());
        for (std::size_t i = 0; i < x.size(); ++i)
            inserted[2 * i] = 2 * x[i];
        x = convolve(inserted, h);
    }
    for (auto &value : x)
        value = model == 0 ? std::tanh(d * value) / normalization
                           : (std::tanh(d * (value + .12)) - offset) / normalization;
    for (int stage = 0; stage < stages; ++stage) {
        const auto h =
            referenceKernel(stage == stages - 1 ? boundaryTaps : higherTaps, stage == stages - 1);
        const auto filtered = convolve(x, h);
        std::vector<double> selected(x.size() / 2);
        for (std::size_t i = 0; i < selected.size(); ++i)
            selected[i] = filtered[2 * i];
        x = std::move(selected);
    }
    double previousS = 0, previousY = 0, previousDc = 0, dc = 0, filtered = 0;
    for (auto &value : x) {
        const double y = (value - a * previousS + (1 + k) * a * previousY) / (1 + k * a);
        previousS = value;
        previousY = y;
        dc = y - previousDc + r * dc;
        previousDc = y;
        filtered = pole * filtered + (1 - pole) * dc;
        value = filtered;
    }
    return x;
}
void filters() {
    for (const auto taps : {boundaryTaps, higherTaps}) {
        const auto h = darkvoice::bandlimited_colour_detail::kernel(taps);
        const auto independent = referenceKernel(taps);
        bool symmetric = true, zeros = true;
        double sum = 0, pass = 0, stop = 0;
        for (std::size_t i = 0; i < taps; ++i) {
            symmetric = symmetric && std::abs(h[i] - h[taps - 1 - i]) < 1e-15 &&
                        std::abs(h[i] - independent[i]) < 1e-15;
            zeros = zeros && (i == (taps - 1) / 2 || i % 2 != 0 || h[i] == 0);
            sum += h[i];
        }
        check(symmetric && zeros && std::abs(sum - 1) < 1e-14,
              "independent halfband/DC/symmetry/structural zeros");
        for (int bin = 0; bin <= 4096; ++bin) {
            const double f = .5 * bin / 4096;
            std::complex<double> response{};
            for (std::size_t j = 0; j < taps; ++j)
                response += h[j] * std::exp(std::complex<double>{0, -2 * std::numbers::pi * f * j});
            if (f <= .2)
                pass = std::max(pass, std::abs(std::abs(response) - 1));
            if (f >= .3)
                stop = std::max(stop, std::abs(response));
        }
        std::printf("Kernel %zu pass %.9g stop %.3fdB\n", taps, pass, 20 * std::log10(stop));
        check(pass < .0002, "per-stage passband error below .0002");
        check(20 * std::log10(stop) < -75, "per-stage stopband below -75dB");
    }
    darkvoice::bandlimited_colour_detail::Oversampler16 resampler;
    resampler.prepare(darkvoice::bandlimited_colour_detail::guardKernel(boundaryTaps),
                      darkvoice::bandlimited_colour_detail::kernel(higherTaps),
                      darkvoice::bandlimited_colour_detail::guardKernel(boundaryTaps));
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
    // PREDECLARED before first run: N4096, single bins719/1597, two-tone683+1129;
    // aggregate amplitudes.15/.5, Tape+Tube drive1 tone16000, all4rates. Warm4096.
    constexpr std::size_t period = 4096, warm = 4096, samples = warm + period + 1024;
    for (double rate : {44100., 48000., 88200., 96000.})
        for (int model : {0, 1})
            for (float amplitude : {.15f, .5f})
                for (int fixture = 0; fixture < 3; ++fixture) {
                    std::vector<float> source(samples);
                    for (std::size_t i = 0; i < source.size(); ++i) {
                        const double phase = 2 * std::numbers::pi * i / period;
                        source[i] = static_cast<float>(
                            amplitude * (fixture == 0   ? std::sin(719 * phase)
                                         : fixture == 1 ? std::sin(1597 * phase)
                                                        : .5 * (std::sin(683 * phase) +
                                                                std::sin(1129 * phase))));
                    }
                    const auto high = reference(source, rate, model, 1, 16000, 5);
                    const auto native = reference(source, rate, model, 1, 16000, 0);
                    auto actual = source;
                    Fx fx;
                    check(fx.prepare(rate, 257, 1) && fx.setParameters({model, 1, 16000, 1}),
                          "alias matrix setup");
                    fx.reset();
                    check(render(fx, actual, nullptr, 257), "alias matrix render");
                    double error8 = 0, error1 = 0, inputPower = 0;
                    for (std::size_t j = 0; j < period; ++j) {
                        const std::size_t at = warm + latency + j;
                        const double e8 = actual[at] - high[at + referenceDelta];
                        const double e1 = native[at - latency] - high[at + referenceDelta];
                        error8 += e8 * e8;
                        error1 += e1 * e1;
                        inputPower += static_cast<double>(source[warm + j]) * source[warm + j];
                    }
                    const double residual = 10 * std::log10(error8 / inputPower);
                    const double improvement = 10 * std::log10(error1 / error8);
                    std::printf("Alias %.0fHz model%d amp%.2f fixture%d residual %.3fdB "
                                "improvement %.3fdB\n",
                                rate, model, amplitude, fixture, residual, improvement);
                    check(residual < -55, "16x residual below -55dB relative input");
                    check(improvement >= 20, "16x residual improves at least20dB against1x");
                }
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
void absoluteAlias() {
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
    double worst = -1000;
    for (double rate : {44100., 48000., 88200., 96000.})
        for (int model : {0, 1})
            for (float amplitude : {.15f, .5f})
                for (std::size_t fundamental :
                     {683U, 719U, 1025U, 1597U, 1720U, 1721U, 1722U, 1723U, 1724U}) {
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
                    check(fx.prepare(rate, 257, 1) && fx.setParameters({model, 1, 16000, 1}),
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
                    std::printf("Absolute %.0fHz model%d amp%.2f bin%zu off-mask %.6fdB/inputRMS\n",
                                rate, model, amplitude, fundamental, db);
                    check(db <= -55, "absolute off-mask energy <= -55dB/inputRMS");
                }
    std::printf("Worst absolute alias %.6fdB/inputRMS\n", worst);
}
std::complex<double> kernelResponse(const std::vector<double> &h, double radians) {
    std::complex<double> value{};
    for (std::size_t tap = 0; tap < h.size(); ++tap)
        value += h[tap] * std::exp(std::complex<double>{0, -radians * tap});
    return value;
}
std::complex<double> linearResamplerResponse(double radians) {
    const auto first = referenceKernel(boundaryTaps, true);
    const auto last = referenceKernel(boundaryTaps, true);
    const auto higher = referenceKernel(higherTaps);
    std::complex<double> response{};
    // Up16 insertion and down16 selection: sum ALL sixteen image paths.
    // Interpolation gain16 and decimation1/16 cancel. No production helper.
    for (int image = 0; image < 16; ++image) {
        const double phase = radians + 2 * std::numbers::pi * image;
        const auto mid = kernelResponse(higher, phase / 4);
        const auto high = kernelResponse(higher, phase / 8);
        const auto highest = kernelResponse(higher, phase / 16);
        response += kernelResponse(first, phase / 2) * kernelResponse(last, phase / 2) * mid * mid *
                    high * high * highest * highest;
    }
    return response;
}
void complexPartialMix() {
    constexpr double rate = 48000, amplitude = 1e-5;
    constexpr std::size_t n = 4800, warm = 48000;
    const auto guard = darkvoice::bandlimited_colour_detail::guardKernel(boundaryTaps);
    const auto guardReference = referenceKernel(boundaryTaps, true);
    double dc = 0, error = 0;
    bool symmetric = true;
    for (std::size_t tap = 0; tap < boundaryTaps; ++tap) {
        dc += guard[tap];
        error = std::max(error, std::abs(guard[tap] - guardReference[tap]));
        symmetric = symmetric && std::abs(guard[tap] - guard[boundaryTaps - 1 - tap]) < 1e-15;
    }
    check(std::abs(dc - 1) < 1e-14 && error < 1e-15 && symmetric,
          "guard coefficient independent DC/symmetry oracle");
    const double nyquistAttenuation =
        20 * std::log10(std::abs(kernelResponse(guardReference, std::numbers::pi / 2)));
    std::printf("Native guard attenuation at Nyquist %.6fdB\n", nyquistAttenuation);
    check(nyquistAttenuation < -75,
          "guard independently measured native Nyquist stopband below -75dB");
    for (int model : {0, 1})
        for (float mix : {0.f, .5f, 1.f})
            for (double frequency : {100., 1000., 8000., 16000., 20000., 22000., 23000.}) {
                const double omega = 2 * std::numbers::pi * frequency / rate;
                std::vector<float> source(warm + n);
                for (std::size_t i = 0; i < source.size(); ++i)
                    source[i] = static_cast<float>(amplitude * std::cos(omega * (i % n)));
                Fx fx;
                check(fx.prepare(rate, 257, 1) && fx.setParameters({model, 1, 16000, mix}),
                      "complex partial-mix setup");
                fx.reset();
                auto output = source;
                check(render(fx, output, nullptr, 257), "complex partial-mix render");
                std::complex<double> in{}, out{};
                for (std::size_t i = 0; i < n; ++i) {
                    const auto basis = std::exp(std::complex<double>{0, -omega * i});
                    in += static_cast<double>(source[warm + i]) * basis;
                    out += static_cast<double>(output[warm + i]) * basis;
                }
                const auto z = std::exp(std::complex<double>{0, -omega});
                const double r = std::exp(-2 * std::numbers::pi * 10 / rate);
                const double t = std::exp(-2 * std::numbers::pi * 16000 / rate);
                const double slope =
                    6 / std::tanh(6) * (model == 0 ? 1 : 1 / std::pow(std::cosh(.72), 2));
                const auto wet = slope * linearResamplerResponse(omega) * (1. - z) / (1. - r * z) *
                                 (1 - t) / (1. - t * z);
                const auto alignedDry = std::exp(std::complex<double>{0, -omega * latency});
                const auto expected = (1. - mix) * alignedDry + static_cast<double>(mix) * wet;
                const auto actual = out / in;
                std::printf("Complex mode%d model%d mix%.1f %.0fHz actual %.7fdB phase %.7frad "
                            "expected %.7fdB phase %.7frad error %.9g\n",
                            static_cast<int>(mode), model, mix, frequency,
                            20 * std::log10(std::abs(actual)), std::arg(actual),
                            20 * std::log10(std::abs(expected)), std::arg(expected),
                            std::abs(actual - expected));
                check(std::abs(actual - expected) < 3e-6,
                      "small-signal partial-mix amplitude and phase match independent complex "
                      "multirate+IIR response");
            }
}
void aliasScan(std::size_t first = 17, std::size_t last = 2031) {
    constexpr std::size_t n = 4096, warm = 49152;
    double worst = -1000;
    int above = 0;
    for (int model : {0, 1})
        for (std::size_t fundamental = first; fundamental <= last; ++fundamental) {
            std::vector<float> source(warm + n);
            for (std::size_t i = 0; i < source.size(); ++i)
                source[i] = static_cast<float>(
                    .5 * std::sin(2 * std::numbers::pi * fundamental * (i % n) / n));
            const double inputPower = energy(source, warm) / n;
            const auto inputBins = spectrum(source, warm, n);
            double unwanted = 0;
            for (std::size_t bin = 0; bin < n; ++bin)
                if (bin != fundamental && bin != n - fundamental)
                    unwanted += std::norm(inputBins[bin]) / (n * n);
            check(unwanted / inputPower < 1e-12 && std::abs(inputPower / .125 - 1) < 1e-6,
                  "scan fixture actual input frequency and RMS");
            auto output = source;
            Fx fx;
            check(fx.prepare(48000, 257, 1) && fx.setParameters({model, 1, 16000, 1}),
                  "scan setup");
            fx.reset();
            check(render(fx, output, nullptr, 257) &&
                      std::all_of(output.begin(), output.end(),
                                  [](float v) { return std::isfinite(v); }),
                  "scan finite render");
            const double db =
                10 * std::log10(offMaskPower(spectrum(output, warm, n), fundamental) / inputPower);
            worst = std::max(worst, db);
            above += db > -55 ? 1 : 0;
            std::printf("Scan mode%d 48000Hz model%d amp.5 bin%zu off-mask %.6fdB/inputRMS\n",
                        static_cast<int>(mode), model, fundamental, db);
        }
    std::printf("Scan summary mode%d bins%zu..%zu %zucases worst %.6fdB/inputRMS above -55dB: %d\n",
                static_cast<int>(mode), first, last, 2 * (last - first + 1), worst, above);
    check(above == 0 && worst <= -55, "every declared scan-range case below -55dB/inputRMS");
    // This fixed supplemental grid does not establish a universal alias bound.
}
double harmonic(const std::vector<float> &x, std::size_t begin, int bin) {
    std::complex<double> sum{};
    constexpr std::size_t period = 4096;
    for (std::size_t i = 0; i < period; ++i)
        sum += static_cast<double>(x[begin + i]) *
               std::exp(std::complex<double>{0, -2 * std::numbers::pi * bin * i / period});
    return 2 * std::abs(sum) / period;
}
std::vector<float> processedTone(int model, float drive, double amplitude, int bin) {
    constexpr std::size_t period = 4096, warm = 16384;
    std::vector<float> data(warm + period);
    for (std::size_t i = 0; i < data.size(); ++i)
        data[i] = static_cast<float>(amplitude * std::sin(2 * std::numbers::pi * bin * i / period));
    Fx fx;
    check(fx.prepare(48000, 257, 1) && fx.setParameters({model, drive, 20000, 1}),
          "character setup");
    fx.reset();
    check(render(fx, data, nullptr, 257), "character tone render");
    return data;
}
void character() {
    std::array<double, 2> compression{};
    for (std::size_t j = 0; j < 2; ++j) {
        const int bin = j == 0 ? 23 : 719;
        const auto low = processedTone(0, 1, .01, bin), high = processedTone(0, 1, .5, bin);
        compression[j] = (harmonic(high, 16384, bin) / .5) / (harmonic(low, 16384, bin) / .01);
    }
    std::printf("Tape amplitude compression low %.9g high %.9g\n", compression[0], compression[1]);
    check(compression[1] < .9 * compression[0],
          "Tape level-dependent HF compression stronger than low band");
    const auto tape = processedTone(0, .7f, .15, 53), tube = processedTone(1, .7f, .15, 53);
    const double oddEven = harmonic(tape, 16384, 106), even = harmonic(tube, 16384, 106);
    std::printf("Second harmonics Tape %.9g Tube %.9g\n", oddEven, even);
    check(oddEven < 1e-6 && even > .001,
          "Tube asymmetry creates actual even harmonics unlike odd Tape shaper");
    double delta = 0;
    for (std::size_t i = 16384; i < tape.size(); ++i)
        delta += std::pow(static_cast<double>(tape[i]) - tube[i], 2);
    check(delta / 4096 > .0001, "styles measurably differ on same material");
    Fx fx;
    check(fx.prepare(48000, 257, 1) && fx.setParameters({1, 1, 16000, 1}), "DC setup");
    fx.reset();
    std::vector<float> dc(96000, .3f);
    check(render(fx, dc, nullptr, 257), "DC settling render");
    const double remaining = energy(dc, dc.size() - 4096) / 4096;
    check(remaining < 1e-12 && energy(dc) > .01,
          "fixed DC blocker removes settled asymmetric DC output");
}
void alignmentOracle(double rate) {
    auto source = signal(2048);
    source[0] = .5f;
    const auto wet = reference(source, rate, 1, .7f, 8000, 4);
    for (float mix : {0.f, .25f, .75f, 1.f}) {
        Fx fx;
        check(fx.prepare(rate, 257, 1) && fx.setParameters({1, .7f, 8000, mix}),
              "parallel oracle setup");
        fx.reset();
        auto actual = source;
        check(render(fx, actual, nullptr, 113), "parallel oracle render");
        bool exact = true;
        for (std::size_t i = 0; i < source.size(); ++i) {
            const double dry = i < latency ? 0 : source[i - latency];
            const double expected = (1 - static_cast<double>(mix)) * dry + mix * wet[i];
            exact = exact && std::abs(actual[i] - expected) < 1e-7;
        }
        check(exact, "independent convolution oracle verifies partial-mix delayed dry alignment");
    }
    // The published native recursive emphasis/de-emphasis is a memory process,
    // independently reconstructed here and distinguishable from its removal.
    const auto tape = reference(source, rate, 0, .8f, 16000, 4);
    const auto noMemory = reference(source, rate, 0, .8f, 16000, 4, false);
    Fx fx;
    check(fx.prepare(rate, 257, 1) && fx.setParameters({0, .8f, 16000, 1}), "memory oracle setup");
    fx.reset();
    auto actual = source;
    check(render(fx, actual, nullptr, 257), "memory oracle render");
    double error = 0, difference = 0;
    for (std::size_t i = 0; i < source.size(); ++i) {
        error = std::max(error, std::abs(actual[i] - tape[i]));
        difference += (tape[i] - noMemory[i]) * (tape[i] - noMemory[i]);
    }
    check(error < 1e-7 && difference / source.size() > 1e-7,
          "Tape native filter memory matches independent recurrence and affects nonlinear audio");
}
void benchmark() {
    for (double rate : {48000., 96000.})
        for (int model : {0, 1}) {
            Fx fx;
            check(fx.prepare(rate, 257, 2) && fx.setParameters({model, 1, 16000, 1}),
                  "cost measurement setup");
            fx.reset();
            auto left = signal(static_cast<std::size_t>(rate)), right = left;
            const auto start = std::chrono::steady_clock::now();
            const bool rendered = render(fx, left, &right, 257);
            const double seconds =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            check(rendered, "cost measurement render");
            std::printf("Cost %.0fHz stereo model%d one audio second: %.6f wall seconds (no "
                        "deadline claim)\n",
                        rate, model, seconds);
        }
}
void validation() {
    Fx fx;
    check(!fx.qualityMode(), "unprepared quality mode absent");
    check(!darkvoice::validQualityMode(static_cast<darkvoice::QualityMode>(-1)) &&
              !darkvoice::validQualityMode(static_cast<darkvoice::QualityMode>(3)) &&
              darkvoice::BandlimitedColour::latencyForQuality(
                  static_cast<darkvoice::QualityMode>(3)) == 0,
          "invalid quality values rejected and have zero latency");
    check(!fx.BandlimitedColour::prepare(48000, 8, 2, static_cast<darkvoice::QualityMode>(3)) &&
              !fx.qualityMode(),
          "invalid prepare quality unprepares");
    std::array<float, 8> left{}, right{};
    std::array<float *, 2> ptr{left.data(), right.data()};
    check(fx.latencySamples() == 0 && !fx.process(ptr, 8),
          "unprepared reports zero latency/rejects");
    for (double rate : {0., 22050., std::numeric_limits<double>::quiet_NaN()})
        check(!fx.prepare(rate, 8, 2), "unsupported rates rejected");
    check(!fx.prepare(48000, 0, 2) && !fx.prepare(48000, 65537, 2), "bounded capacity");
    check(!fx.prepare(48000, 8, 0) && !fx.prepare(48000, 8, 3), "mono/stereo only");
    reject = true;
    const bool prepared = fx.prepare(48000, 8, 2);
    reject = false;
    check(!prepared && !fx.process(ptr, 8), "allocation failure leaves unprepared");
    check(fx.prepare(48000, 8, 2) && fx.latencySamples() == latency,
          "prepare recovers fixed latency");
    check(fx.qualityMode() == mode, "active prepared mode reported");
    const auto copy = left;
    std::array<float *, 2> null{left.data(), nullptr}, alias{left.data(), left.data()};
    check(!fx.process(null, 8) && left == copy, "null rejects atomically");
    check(!fx.process(alias, 8) && left == copy, "alias rejects atomically");
    check(!fx.process(ptr, 9) && left == copy, "oversized rejects atomically");
    check(!fx.process(std::span<float *>(ptr.data(), 1), 8), "wrong layout rejected");
    check(fx.process(null, 0), "zero frames no-op");
    for (auto member : {&darkvoice::ColourParameters::drive, &darkvoice::ColourParameters::toneHz,
                        &darkvoice::ColourParameters::mix})
        for (float bad :
             {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -1.f,
              std::numeric_limits<float>::max()}) {
            darkvoice::ColourParameters p;
            p.*member = bad;
            check(!fx.setParameters(p), "invalid scalar snapshot rejected");
        }
    check(!fx.setParameters({-1, .2f, 16000, 0}) && !fx.setParameters({2, .2f, 16000, 0}),
          "invalid style rejected");
    check(fx.qualityMode() == mode, "parameter updates do not change prepared quality");
    check(!fx.prepare(12345, 8, 2) && fx.latencySamples() == 0 && !fx.qualityMode() &&
              !fx.process(ptr, 8),
          "invalid prepare unprepares");
}
void lifecycle() {
    darkvoice::BandlimitedColour fx;
    check(fx.prepare(48000, 1, 1) && fx.qualityMode() == darkvoice::QualityMode::Live &&
              fx.latencySamples() == 124,
          "three-argument prepare defaults to Live");
    check(fx.prepare(96000, 65536, 2, mode) && fx.qualityMode() == mode &&
              fx.latencySamples() == latency,
          "maximum capacity prepares selected quality");
    check(fx.setParameters({1, 1, 1000, 1}), "lifecycle old active target accepted");
    fx.reset();
    check(fx.prepare(48000, 257, 1, mode), "reprepare same mode resets parameters to defaults");
    auto x = signal(2048), source = x;
    bool rendered = true;
    for (std::size_t at = 0; at < x.size(); at += 257) {
        std::array<float *, 1> ptr{x.data() + at};
        rendered = rendered && fx.process(ptr, std::min<std::size_t>(257, x.size() - at));
    }
    bool neutral = rendered;
    for (std::size_t i = 0; i < x.size(); ++i)
        neutral = neutral && x[i] == (i < latency ? 0 : source[i - latency]);
    check(neutral, "reprepare clears old wet mix and dry history exactly");
    check(!fx.prepare(48000, 8, 1, static_cast<darkvoice::QualityMode>(-1)) && !fx.qualityMode() &&
              fx.latencySamples() == 0,
          "invalid mode on prepared object releases active state");
}
void behavior(double rate, std::size_t channels) {
    Fx fx, other;
    check(fx.prepare(rate, 257, channels) && other.prepare(rate, 257, channels),
          "behavior prepare");
    auto left = signal(4096), right = left;
    left[0] = 1e-21f;
    left[1] = -std::numeric_limits<float>::min();
    const auto dry = left, dryRight = right;
    check(render(fx, left, channels == 2 ? &right : nullptr, 257), "neutral render");
    bool exact = true;
    for (std::size_t i = 0; i < left.size(); ++i) {
        exact = exact && left[i] == (i < latency ? 0 : dry[i - latency]);
        if (channels == 2)
            exact = exact && right[i] == (i < latency ? 0 : dryRight[i - latency]);
    }
    check(exact, "neutral exact sanitized normal dry delayed by mode latency samples");
    fx.reset();
    left.assign(latency + 256, 0);
    left[0] = std::numeric_limits<float>::quiet_NaN();
    left[1] = std::numeric_limits<float>::infinity();
    left[2] = -std::numeric_limits<float>::infinity();
    left[3] = std::numeric_limits<float>::denorm_min();
    left[4] = -std::numeric_limits<float>::denorm_min();
    left[5] = std::numeric_limits<float>::max();
    left[6] = std::numeric_limits<float>::min();
    left[7] = -std::numeric_limits<float>::min();
    left[8] = -std::numeric_limits<float>::max();
    right = left;
    auto sanitizedInput = left;
    std::fill(sanitizedInput.begin(), sanitizedInput.begin() + 5, 0);
    bool sanitized = render(fx, left, channels == 2 ? &right : nullptr, 113);
    for (std::size_t i = 0; i < left.size(); ++i) {
        const float expected = i < latency ? 0 : sanitizedInput[i - latency];
        sanitized = sanitized && left[i] == expected && (channels == 1 || right[i] == expected);
    }
    check(sanitized, "neutral delayed path sanitizes only nonfinite/true subnormal values");
    check(fx.setParameters({1, .9f, 6000, .7f}) && other.setParameters({1, .9f, 6000, .7f}),
          "active set");
    fx.reset();
    other.reset();
    left = signal(8192);
    right = left;
    auto split = left, splitRight = right;
    check(render(fx, left, channels == 2 ? &right : nullptr, 257) &&
              render(other, split, channels == 2 ? &splitRight : nullptr, 37) && left == split &&
              (channels == 1 || right == splitRight),
          "active reset/partition equality");
    check(energy(left) > .001, "real processed energy");
    for (int stage = 0; stage < 4; ++stage) {
        const darkvoice::ColourParameters p{stage % 2, .2f * stage, 2000.f + 3000 * stage,
                                            .2f * stage};
        check(fx.setParameters(p) && other.setParameters(p), "automation set");
        left = signal(1024);
        right = left;
        split = left;
        splitRight = right;
        check(render(fx, left, channels == 2 ? &right : nullptr, 257) &&
                  render(other, split, channels == 2 ? &splitRight : nullptr, 11) &&
                  left == split && (channels == 1 || right == splitRight),
              "automated four controls partition invariant");
    }
    std::array<float *, 2> emptyNull{};
    check(fx.process(std::span<float *>(emptyNull.data(), channels), 0),
          "empty warmed call accepts null pointers and freezes histories");
    check(!fx.setParameters({3, .5f, 8000, .5f}) && !fx.process(std::span<float *>{}, 1),
          "invalid warmed calls reject");
    left = signal(1024);
    right = left;
    split = left;
    splitRight = right;
    check(render(fx, left, channels == 2 ? &right : nullptr, 257) &&
              render(other, split, channels == 2 ? &splitRight : nullptr, 13) && left == split &&
              (channels == 1 || right == splitRight),
          "invalid calls preserve warmed histories");
    fx.reset();
    std::array<float, 257> block{};
    std::array<float *, 2> ptr{block.data(), nullptr};
    std::array<float, 257> second{};
    ptr[1] = second.data();
    const auto before = allocations.load();
    reject = true;
    bool safe = fx.setParameters({0, 1, 20000, 1});
    for (int i = 0; i < 8; ++i)
        safe = safe && fx.process(std::span<float *>(ptr.data(), channels), 257);
    fx.reset();
    reject = false;
    check(safe && allocations.load() == before, "process/set/reset allocate nothing");
    check(fx.setParameters({1, 1, 1000, 1}), "extreme setup");
    fx.reset();
    left.assign(2048, std::numeric_limits<float>::max());
    right = left;
    left[0] = std::numeric_limits<float>::quiet_NaN();
    left[1] = std::numeric_limits<float>::infinity();
    left[2] = std::numeric_limits<float>::denorm_min();
    check(render(fx, left, channels == 2 ? &right : nullptr, 257) &&
              std::all_of(left.begin(), left.end(), [](float x) { return std::isfinite(x); }) &&
              std::all_of(right.begin(), right.end(), [](float x) { return std::isfinite(x); }),
          "hostile floats remain finite");
    if (channels == 2) {
        fx.reset();
        left = signal(2048);
        right.assign(left.size(), 0);
        check(render(fx, left, &right, 113) &&
                  std::all_of(right.begin(), right.end(), [](float value) { return value == 0; }),
              "independent channels preserve silent opposite channel");
        fx.reset();
        left = signal(2048);
        right = left;
        check(render(fx, left, &right, 257) && left == right,
              "identical dualmono has coherent channel processing");
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
    bool scanOnly = false, coreOnly = false, safetyOnly = false;
    std::size_t scanFirst = 17, scanLast = 2031;
    std::vector<darkvoice::QualityMode> modes{darkvoice::QualityMode::Live,
                                              darkvoice::QualityMode::Studio,
                                              darkvoice::QualityMode::Offline};
    if (argc != 1) {
        const std::string_view option(argv[1]);
        if (!((argc == 3 && (option == "--quality" || option == "--scan" || option == "--core" ||
                             option == "--safety")) ||
              (argc == 5 && option == "--scan-range")))
            return 2;
        scanOnly = option == "--scan" || option == "--scan-range";
        coreOnly = option == "--core";
        safetyOnly = option == "--safety";
        if (option == "--scan-range") {
            const auto parse = [](const char *text, std::size_t &value) {
                const std::string_view token(text);
                const auto result =
                    std::from_chars(token.data(), token.data() + token.size(), value);
                return result.ec == std::errc{} && result.ptr == token.data() + token.size();
            };
            if (!parse(argv[3], scanFirst) || !parse(argv[4], scanLast) || scanFirst < 17 ||
                scanLast > 2031 || scanFirst > scanLast)
                return 2;
        }
        const std::string_view name(argv[2]);
        if (name == "live")
            modes = {darkvoice::QualityMode::Live};
        else if (name == "studio")
            modes = {darkvoice::QualityMode::Studio};
        else if (name == "offline")
            modes = {darkvoice::QualityMode::Offline};
        else
            return 2;
    }
    for (auto selected : modes) {
        mode = selected;
        latency = darkvoice::BandlimitedColour::latencyForQuality(mode);
        boundaryTaps = mode == darkvoice::QualityMode::Live     ? 193
                       : mode == darkvoice::QualityMode::Studio ? 257
                                                                : 513;
        higherTaps = mode == darkvoice::QualityMode::Live     ? 65
                     : mode == darkvoice::QualityMode::Studio ? 129
                                                              : 257;
        referenceDelta = mode == darkvoice::QualityMode::Live     ? 2
                         : mode == darkvoice::QualityMode::Studio ? 4
                                                                  : 8;
        std::printf("Quality mode %d latency %zu\n", static_cast<int>(mode), latency);
        if (scanOnly) {
            aliasScan(scanFirst, scanLast);
            continue;
        }
        validation();
        lifecycle();
        if (!safetyOnly)
            filters();
        for (double rate : {44100., 48000., 88200., 96000.})
            for (std::size_t channels : {1U, 2U})
                behavior(rate, channels);
        if (safetyOnly)
            continue;
        for (double rate : {44100., 48000., 88200., 96000.})
            alignmentOracle(rate);
        character();
        aliasMatrix();
        absoluteAlias();
        if (!coreOnly)
            aliasScan();
        complexPartialMix();
        benchmark();
    }
    std::printf("BandlimitedColour checks: %d, failures: %d\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
