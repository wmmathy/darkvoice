// Developer-only observations: a successful exit validates rendering/I/O, not quality.
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <darkvoice/PitchFormantProcessor.hpp>
#include <limits>
#include <numbers>
#include <span>
#include <string_view>
#ifdef DARKVOICE_COUPLING_INSTRUMENTED
#include <signalsmith-stretch.h>
#endif
#include <vector>
namespace {
constexpr double rate = 48000;
constexpr std::size_t frames = 96000;
struct Fixture {
    double f0, center;
    float pitch, formant;
};
std::vector<float> vowel(double f0, double center, float gain, bool evenOnly = false) {
    std::vector<float> result(frames);
    // Preserve the retained fixture's float accumulation and multiplication order.
    for (int h = 1; h <= 80; ++h) {
        if (evenOnly && h % 2)
            continue;
        const double f = h * f0;
        const double envelope = std::exp(-.5 * std::pow((f - center) / 100, 2)) + .002 / h;
        for (std::size_t i = 0; i < result.size(); ++i)
            result[i] +=
                static_cast<float>(.025 * envelope * std::sin(2 * std::numbers::pi * f * i / rate));
    }
    for (auto &sample : result)
        sample *= gain;
    return result;
}
double correlation(const std::vector<float> &x, double f0) {
    const auto lag = static_cast<std::size_t>(std::round(rate / f0));
    double ab = 0, aa = 0, bb = 0;
    for (std::size_t i = x.size() / 2 + lag; i < x.size(); ++i) {
        ab += static_cast<double>(x[i]) * x[i - lag];
        aa += static_cast<double>(x[i]) * x[i];
        bb += static_cast<double>(x[i - lag]) * x[i - lag];
    }
    return aa * bb > 0 ? ab / std::sqrt(aa * bb) : 0;
}
bool save(const std::vector<float> &x, const char *name) {
    double energy = 0;
    for (float sample : x) {
        if (!std::isfinite(sample))
            return false;
        energy += static_cast<double>(sample) * sample;
    }
    if (x.size() != frames || !(energy > 0))
        return false;
    auto *file = std::fopen(name, "wb");
    if (!file)
        return false;
    const auto written = std::fwrite(x.data(), sizeof(float), x.size(), file);
    const auto closed = std::fclose(file);
    return written == x.size() && closed == 0;
}
} // namespace

#ifdef DARKVOICE_COUPLING_INSTRUMENTED
int matrix() {
    constexpr std::array fixtures{Fixture{100, 600, 12, 0}, Fixture{70, 600, -12, 0},
                                  Fixture{180, 1200, 12, 7}};
    for (std::size_t c = 0; c < fixtures.size(); ++c)
        for (int mode = 0; mode < 4; ++mode) {
            const auto &f = fixtures[c];
            darkvoice_coupling::gainMode = mode;
            darkvoice_coupling::sourceF0 = f.f0;
            darkvoice_coupling::centerHz = f.center;
            darkvoice_coupling::pitchScale = std::exp2(f.pitch / 12.);
            darkvoice_coupling::formantScale = std::exp2(f.formant / 12.);
            darkvoice_coupling::clear();
            darkvoice::PitchFormantProcessor processor;
            if (!processor.prepare({rate, 1024, 1, darkvoice::PitchQuality::Offline}) ||
                !processor.setParameters({f.pitch, f.formant, true, static_cast<float>(f.f0)}))
                return 1;
            processor.reset();
            auto x = vowel(f.f0, f.center, 1);
            for (std::size_t offset = 0; offset < x.size(); offset += 512) {
                float *pointer = x.data() + offset;
                if (!processor.process(std::span<float *>(&pointer, 1),
                                       std::min<std::size_t>(512, x.size() - offset)))
                    return 1;
            }
            char path[160];
            std::snprintf(path, sizeof(path), "/tmp/pitch-matrix-s%d-c%zu-m%d.raw",
                          DARKVOICE_COUPLING_SPLIT, c, mode);
            if (!save(x, path))
                return 1;
            const auto target = f.f0 * std::exp2(f.pitch / 12.);
            const auto measured = correlation(x, target);
            const auto energy = darkvoice_coupling::predictionEnergy;
            std::printf("split%d case%zu mode%d latency%zu targetHz%.9g retainedCorrelation%.9f "
                        "periodicityPass%d predictions%llu fallbacks%llu fallbackEnergyShare%.9f "
                        "maximumEnergyRatio%.9g\n",
                        DARKVOICE_COUPLING_SPLIT, c, mode, processor.latencySamples(), target,
                        measured, measured > .8, darkvoice_coupling::predictions,
                        darkvoice_coupling::fallbacks,
                        energy > 0 ? darkvoice_coupling::fallbackEnergy / energy : 0,
                        darkvoice_coupling::maximumRatio);
        }
    return 0;
}
#endif

int main(int argc, char **argv) {
    static_assert(std::endian::native == std::endian::little);
    static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
#ifdef DARKVOICE_COUPLING_INSTRUMENTED
    if (argc == 2 && std::string_view(argv[1]) == "--matrix")
        return matrix();
#else
    (void)argv;
#endif
    if (argc != 1) {
        std::fprintf(stderr, "usage: pitch-coupling-probe\n");
        return 2;
    }
    constexpr std::array fixtures{Fixture{100, 600, 12, 0}, Fixture{70, 600, -12, 0},
                                  Fixture{180, 1200, 12, 7}};
    for (std::size_t c = 0; c < fixtures.size(); ++c)
        for (float gain : {1.f, .25f, 4.f})
            for (bool compensation : {true, false}) {
                const auto &f = fixtures[c];
                darkvoice::PitchFormantProcessor processor;
                if (!processor.prepare({rate, 1024, 1, darkvoice::PitchQuality::Offline}) ||
                    !processor.setParameters({f.pitch, compensation ? f.formant : 0.f, compensation,
                                              static_cast<float>(f.f0)}))
                    return 1;
                processor.reset();
                auto x = vowel(f.f0, f.center, gain);
                const auto maximum = *std::max_element(
                    x.begin(), x.end(), [](float a, float b) { return std::abs(a) < std::abs(b); });
                // The wrapper's FFT clamp is +/-8. These observations exclude clipping.
                if (!(std::abs(maximum) < 8))
                    return 1;
                for (std::size_t offset = 0; offset < x.size(); offset += 512) {
                    float *pointer = x.data() + offset;
                    if (!processor.process(std::span<float *>(&pointer, 1),
                                           std::min<std::size_t>(512, x.size() - offset)))
                        return 1;
                }
                const auto target = f.f0 * std::exp2(f.pitch / 12.);
                const auto measured = correlation(x, target);
                char path[160];
                std::snprintf(path, sizeof(path), "/tmp/pitch-coupling-c%zu-g%.2f-m%d.raw", c, gain,
                              compensation);
                if (!save(x, path))
                    return 1;
                std::printf("case%zu gain%.2f compensation%d latency%zu inputPeak%.9g "
                            "targetHz%.9g retainedCorrelation%.9f periodicityPass%d\n",
                            c, gain, compensation, processor.latencySamples(), std::abs(maximum),
                            target, measured, measured > .8);
            }
    for (int c = 0; c < 3; ++c) {
        auto x = vowel(c == 1 ? 70 : 35, 600, 1, c == 2);
        char path[160];
        std::snprintf(path, sizeof(path), "/tmp/pitch-coupling-control%d.raw", c);
        if (!save(x, path))
            return 1;
    }
    return 0;
}
