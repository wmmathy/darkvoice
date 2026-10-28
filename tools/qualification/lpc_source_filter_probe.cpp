// AGPL-3.0-only. Finite stationary developer oracle; never a streaming processor.
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <darkvoice/PitchFormantProcessor.hpp>
#include <numbers>
#include <stdexcept>
#include <vector>
namespace {
constexpr int order = 24;
constexpr std::size_t count = 96000;
constexpr double rate = 48000, pi = std::numbers::pi;
using Signal = std::vector<double>;
using Model = std::array<double, order + 1>;
void require(bool ok, const char *text) {
    if (!ok)
        throw std::runtime_error(text);
}
double maximum(const Signal &x) {
    double p = 0;
    for (double v : x) {
        require(std::isfinite(v), "nonfinite signal");
        p = std::max(p, std::abs(v));
    }
    return p;
}
Model fit(const Signal &x) {
    Model a{}, r{};
    a[0] = 1;
    const double scale = maximum(x);
    if (scale == 0) {
        std::puts("model exact-silence identity");
        return a;
    }
    for (int k = 0; k <= order; ++k)
        for (std::size_t n = static_cast<std::size_t>(k); n < x.size(); ++n)
            r[k] += (x[n] / scale) * (x[n - k] / scale) / static_cast<double>(x.size());
    double energy = r[0], largest = 0, smallest = 1;
    require(std::isfinite(energy) && energy > 0, "rejected initial prediction energy");
    for (int m = 1; m <= order; ++m) {
        double numerator = r[m];
        for (int j = 1; j < m; ++j)
            numerator += a[j] * r[m - j];
        const double reflection = -numerator / energy;
        const double nextEnergy = energy * (1 - reflection * reflection);
        std::printf("stage%d reflection%.17g E%.17g\n", m, reflection, nextEnergy);
        if (!std::isfinite(reflection) || std::abs(reflection) >= 1 || !std::isfinite(nextEnergy) ||
            nextEnergy <= 0) {
            std::printf("REJECT stage%d priorE%.17g numerator%.17g\n", m, energy, numerator);
            throw std::runtime_error("LPC stage rejection");
        }
        auto next = a;
        for (int j = 1; j < m; ++j)
            next[j] += reflection * a[m - j];
        next[m] = reflection;
        a = next;
        energy = nextEnergy;
        largest = std::max(largest, std::abs(reflection));
        smallest = std::min(smallest, std::abs(reflection));
    }
    std::printf("model inputMax%.17g R0%.17g E%.17g EoverR0%.17g conditionProxyR0overE%.17g "
                "reflectionAbsMin%.17g reflectionAbsMax%.17g\ncoefficients",
                scale, r[0], energy, energy / r[0], r[0] / energy, smallest, largest);
    for (double v : a)
        std::printf(" %.17g", v);
    std::puts("");
    return a;
}
Signal inverse(const Signal &x, const Model &a) {
    Signal e(x.size());
    for (std::size_t n = 0; n < x.size(); ++n) {
        e[n] = x[n];
        for (int k = 1; k <= order && static_cast<std::size_t>(k) <= n; ++k)
            e[n] += a[k] * x[n - k];
    }
    return e;
}
Signal synthesis(const Signal &e, const Model &a, double alpha, bool report = true) {
    std::array<double, order + 1> state{}, q{};
    double denominator = 1, power = 1, maxState = 0;
    for (int k = 1; k <= order; ++k) {
        power *= alpha;
        denominator += a[k] * power;
    }
    require(std::isfinite(denominator) && denominator != 0, "invalid feedthrough denominator");
    Signal y(e.size());
    for (std::size_t n = 0; n < e.size(); ++n) {
        double offset = 0;
        q[0] = 0;
        for (int k = 1; k <= order; ++k) {
            q[k] = alpha * q[k - 1] + state[k];
            offset += a[k] * q[k];
        }
        y[n] = (e[n] - offset) / denominator;
        require(std::isfinite(y[n]), "nonfinite resynthesis");
        double previous = y[n];
        for (int k = 1; k <= order; ++k) {
            const double value = alpha * previous + state[k];
            state[k] = previous - alpha * value;
            previous = value;
            require(std::isfinite(state[k]), "nonfinite state");
            maxState = std::max(maxState, std::abs(state[k]));
        }
    }
    if (report)
        std::printf("synthesis alpha%.17g denominator%.17g maxState%.17g outputPeak%.17g\n", alpha,
                    denominator, maxState, maximum(y));
    return y;
}
Signal vowel(double f0, double center, bool even = false) {
    std::vector<float> x(count);
    for (int h = 1; h <= 80; ++h) {
        if (even && h % 2)
            continue;
        const double f = f0 * h, env = std::exp(-.5 * std::pow((f - center) / 100, 2)) + .002 / h;
        for (std::size_t n = 0; n < count; ++n)
            x[n] += static_cast<float>(.025 * env * std::sin(2 * pi * f * n / rate));
    }
    return Signal(x.begin(), x.end());
}
Signal shift(const Signal &x, float pitch) {
    require(x.size() == count, "input length");
    darkvoice::PitchFormantProcessor fx;
    require(fx.prepare({rate, 1024, 1, darkvoice::PitchQuality::Offline}) &&
                fx.setParameters({pitch, 0, false, 0}),
            "backend setup");
    fx.reset();
    std::vector<float> y(x.begin(), x.end());
    double actual = 0;
    for (float v : y)
        actual = std::max(actual, std::abs(static_cast<double>(v)));
    require(actual < 8, "backend input clamp risk");
    std::printf(
        "backend frames%zu latency%zu actualInputPeak%.17g pitch%.0f preserve0 formant0 base0\n",
        y.size(), fx.latencySamples(), actual, pitch);
    for (std::size_t n = 0; n < y.size(); n += 512) {
        float *p = y.data() + n;
        require(fx.process(std::span<float *>(&p, 1), std::min<std::size_t>(512, y.size() - n)),
                "backend render");
    }
    return Signal(y.begin(), y.end());
}
double correlation(const Signal &x, double f0) {
    const std::size_t lag = static_cast<std::size_t>(std::round(rate / f0));
    double ab = 0, aa = 0, bb = 0;
    for (std::size_t n = count / 2 + lag; n < count; ++n) {
        ab += x[n] * x[n - lag];
        aa += x[n] * x[n];
        bb += x[n - lag] * x[n - lag];
    }
    return aa * bb > 0 ? ab / std::sqrt(aa * bb) : 0;
}
double amplitude(const Signal &x, double hz) {
    const auto step = std::polar(1., 2 * pi * hz / rate);
    auto ph = std::polar(1., 2 * pi * hz * (count / 2) / rate);
    std::complex<double> sum{};
    for (std::size_t n = count / 2; n < count; ++n) {
        sum += x[n] * ph;
        ph *= step;
    }
    return 2 * std::abs(sum) / (count / 2);
}
bool metrics(const char *label, const Signal &x, double f0, double target) {
    static int fileNumber = 0;
    char path[128];
    std::snprintf(path, sizeof(path), "/tmp/lpc-source-filter-%d.f64", fileNumber++);
    auto *file = std::fopen(path, "wb");
    require(file != nullptr, "open measurement");
    const auto written = std::fwrite(x.data(), sizeof(double), x.size(), file);
    const auto closed = std::fclose(file);
    require(written == x.size() && closed == 0, "write measurement");
    require(x.size() == count, "output length");
    maximum(x);
    double energy = 0, grid = 0, odd = 0, best = 0, peak = 0;
    for (std::size_t n = count / 2; n < count; ++n)
        energy += x[n] * x[n] / (count / 2);
    require(energy > 0 && std::isfinite(energy), "measurement energy");
    for (int h = 1; h * f0 < rate / 2; ++h) {
        const double amp = amplitude(x, h * f0), e = amp * amp / 2;
        grid += e;
        if (h % 2)
            odd += e;
        if (h * f0 >= 200 && h * f0 <= 3000 && amp > best) {
            best = amp;
            peak = h * f0;
        }
    }
    const double corr = correlation(x, f0), error = std::abs(peak - target) / target;
    std::printf(
        "metrics %s frames%zu finite1 last48000Energy%.17g halfPeriodCorr%.9g fullPeriodCorr%.9g "
        "doublePeriodCorr%.9g gridOverTotal%.9g oddOverTotal%.9g peakHz%.9g targetEnvelopeHz%.9g "
        "envelopeRelativeError%.9g periodicityPass%d envelopePass%d\n",
        label, x.size(), energy, correlation(x, 2 * f0), corr, correlation(x, f0 / 2),
        grid / energy, odd / energy, peak, target, error, corr > .8, error < .2);
    return corr > .8 && error < .2;
}
void reconstruction(const Signal &x, const Model &a) {
    const auto y = synthesis(inverse(x, a), a, 0, false);
    double err = 0, en = 0, peak = 0;
    for (std::size_t n = 0; n < x.size(); ++n) {
        const double d = y[n] - x[n];
        err += d * d;
        en += x[n] * x[n];
        peak = std::max(peak, std::abs(d));
    }
    const double relative = en > 0 ? std::sqrt(err / en) : std::sqrt(err);
    std::printf("reconstruction relativeRMS%.17g peak%.17g\n", relative, peak);
    require(relative <= 2e-7 && peak <= 2e-6, "reconstruction invariant");
}
void invariants() {
    Model a{};
    a[0] = 1;
    a[1] = -1.5;
    a[2] = .7; // conjugate poles radius sqrt(.7)<1
    Signal source(count);
    for (std::size_t n = 0; n < count; ++n)
        source[n] = .01 * std::sin(.071 * n) + .003 * std::cos(.137 * n);
    Model firstOrder{};
    firstOrder[0] = 1;
    firstOrder[1] = -.7; // known real pole .7
    reconstruction(synthesis(source, firstOrder, 0), firstOrder);
    auto known = synthesis(source, a, 0);
    reconstruction(known, a);
    const auto estimated = fit(known);
    reconstruction(known, estimated);
    for (double alpha : {0., .2, -.2}) {
        Signal impulse(count);
        impulse[0] = 1;
        const auto h = synthesis(impulse, a, alpha);
        for (double omega : {.01, .1, .7, 2.}) {
            const auto z = std::polar(1., -omega), d = (z + alpha) / (1. + alpha * z);
            std::complex<double> denominator = 1, power = 1, measured{};
            for (int k = 1; k <= order; ++k) {
                power *= d;
                denominator += a[k] * power;
            }
            for (std::size_t n = 0; n < count; ++n)
                measured += h[n] * std::polar(1., -omega * n);
            const double error = std::abs(measured - 1. / denominator);
            std::printf("warp alpha%.9g omega%.9g complexAbsError%.17g\n", alpha, omega, error);
            require(error < 2e-10, "complex warp response");
        }
        auto scaled = source;
        for (double &v : scaled)
            v *= 4;
        const auto y = synthesis(source, a, alpha, false), sy = synthesis(scaled, a, alpha, false);
        double error = 0;
        for (std::size_t n = 0; n < count; ++n)
            error = std::max(error, std::abs(sy[n] - 4 * y[n]));
        std::printf("levelScaling alpha%.9g gain4 peakError%.17g\n", alpha, error);
        require(error < 2e-12, "linear level scaling");
    }
    Signal silence(count);
    reconstruction(silence, fit(silence));
    auto neutral = shift(source, 0);
    double error = 0;
    constexpr std::size_t latency = 9216;
    for (std::size_t n = latency; n < count; ++n)
        error = std::max(error, std::abs(neutral[n] - source[n - latency]));
    std::printf("neutralBackendIdentityPeak%.17g gate1e-5\n", error);
    require(error <= 1e-5, "neutral backend identity");
    std::puts("invariants PASS");
}
} // namespace
int main() {
    try {
        invariants();
        constexpr std::array<std::array<double, 4>, 3> fixtures{
            {{100, 600, 12, 0}, {70, 600, -12, 0}, {180, 1200, 12, 7}}};
        int passes = 0;
        for (std::size_t c = 0; c < fixtures.size(); ++c) {
            const auto f = fixtures[c];
            std::printf("CASE%zu F0%.0f center%.0f pitch%.0f formant%.0f\n", c, f[0], f[1], f[2],
                        f[3]);
            const auto x = vowel(f[0], f[1]);
            const auto a = fit(x);
            reconstruction(x, a);
            auto e = inverse(x, a);
            const double peak = maximum(e), scale = peak > 0 ? .25 / peak : 1;
            std::printf("residual peak%.17g scale%.17g\n", peak, scale);
            for (double &v : e)
                v *= scale;
            auto shifted = shift(e, static_cast<float>(f[2]));
            for (double &v : shifted)
                v /= scale;
            const double s = std::exp2(f[3] / 12), alpha = (s - 1) / (s + 1),
                         target = rate / pi * std::atan(s * std::tan(pi * f[1] / rate)),
                         f0 = f[0] * std::exp2(f[2] / 12);
            const auto y = synthesis(shifted, a, alpha);
            std::printf("warpMappedCenterHz%.17g linearTargetHz%.17g\n", target, f[1] * s);
            passes += metrics("candidate", y, f0, f[1] * s);
            metrics("uncompensated", shift(x, static_cast<float>(f[2])), f0, f[1] * s);
        }
        metrics("ideal35", vowel(35, 600), 35, 600);
        metrics("unchanged70", vowel(70, 600), 35, 600);
        metrics("evenOnly35", vowel(35, 600, true), 35, 600);
        std::printf("RESULT candidatePasses%d/3; static-oracle only; no suite expansion or "
                    "streaming claim\n",
                    passes);
        return passes == 3 ? 0 : 1;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "FAIL %s\n", e.what());
        return 2;
    }
}
