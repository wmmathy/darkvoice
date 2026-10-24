// Developer-only bounded observations, not a qualified processor or quality-pass exit status.
#include "rubberband/RubberBandStretcher.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <numbers>
#include <vector>
using RubberBand::RubberBandStretcher;
int main(int argc, char **argv) {
    static_assert(std::endian::native == std::endian::little,
                  "measurement files use little-endian float32");
    static_assert(sizeof(float) == 4);
    if (argc > 2 || (argc == 2 && std::strcmp(argv[1], "--realtime") != 0)) {
        std::fprintf(stderr, "usage: probe [--realtime]\n");
        return 2;
    }
    const bool rt = argc == 2;
    struct Case {
        double f0, center, pitch, formant;
    };
    int caseNo = 0;
    for (auto c :
         {Case{100, 600, 12, 0}, Case{70, 600, -12, 0}, Case{180, 1200, 12, 7},
          Case{110, 0, -12, 0}, Case{110, 0, -7, 0}, Case{110, 0, 7, 0}, Case{110, 0, 12, 0}}) {
        const std::size_t n = 96000, block = 512;
        const int flags = (rt ? RubberBandStretcher::OptionProcessRealTime
                              : RubberBandStretcher::OptionProcessOffline) |
                          RubberBandStretcher::OptionEngineFiner |
                          RubberBandStretcher::OptionThreadingNever |
                          (c.center > 0 ? RubberBandStretcher::OptionFormantPreserved
                                        : RubberBandStretcher::OptionFormantShifted);
        RubberBandStretcher fx(48000, 1, flags, 1.0, std::exp2(c.pitch / 12));
        if (c.formant != 0)
            fx.setFormantScale(std::exp2((c.formant - c.pitch) / 12));
        fx.setExpectedInputDuration(n);
        fx.setMaxProcessSize(block);
        std::vector<float> in(n), out;
        out.reserve(n + 8192);
        if (c.center > 0) {
            for (int h = 1; h <= 80; ++h) {
                double f = h * c.f0;
                double amp = .025 * (std::exp(-.5 * std::pow((f - c.center) / 100, 2)) + .002 / h);
                for (std::size_t i = 0; i < n; ++i)
                    in[i] += float(amp * std::sin(2 * std::numbers::pi * f * i / 48000));
            }
        } else
            for (std::size_t i = 0; i < n; ++i)
                in[i] = float(.1 * std::sin(2 * std::numbers::pi * c.f0 * i / 48000));
        const auto pad = fx.getPreferredStartPad(), delay = fx.getStartDelay();
        if (rt) {
            std::vector<float> silence(pad);
            for (std::size_t off = 0; off < pad; off += block) {
                const float *a[] = {silence.data() + off};
                fx.process(a, std::min(block, pad - off), false);
                while (fx.available() > 0) {
                    int avail = fx.available();
                    auto old = out.size();
                    out.resize(old + avail);
                    float *b[] = {out.data() + old};
                    const auto got = fx.retrieve(b, avail);
                    out.resize(old + got);
                    if (got == 0) {
                        std::fprintf(stderr, "stalled padding retrieval\n");
                        return 1;
                    }
                }
            }
        } else {
            const float *whole[] = {in.data()};
            fx.study(whole, n, true);
        }
        bool invalid = false;
        for (std::size_t off = 0; off < n; off += block) {
            const float *a[] = {in.data() + off};
            const auto count = std::min(block, n - off);
            fx.process(a, count, off + count == n);
            while (fx.available() > 0) {
                int avail = fx.available();
                std::size_t old = out.size();
                out.resize(old + std::size_t(avail));
                float *b[] = {out.data() + old};
                auto got = fx.retrieve(b, std::size_t(avail));
                out.resize(old + got);
                if (got == 0) {
                    invalid = true;
                    break;
                }
            }
        }
        const auto rawCount = out.size();
        if (rt) {
            out.erase(out.begin(), out.begin() + std::min(delay, out.size()));
            if (out.size() > n)
                out.resize(n);
        }
        std::printf("mode%s rawCount%zu trimmed%zu startPad%zu startDiscard%zu ",
                    rt ? "RT" : "Offline", rawCount, out.size(), pad, delay);
        char name[100];
        std::snprintf(name, sizeof name, "/tmp/r3-%s-case-%d.raw", rt ? "rt" : "off", caseNo);
        auto *f = std::fopen(name, "wb");
        if (!f)
            return 2;
        const bool written = std::fwrite(out.data(), sizeof(float), out.size(), f) == out.size();
        const bool closed = std::fclose(f) == 0;
        if (!written || !closed)
            return 2;
        double aa = 0, bb = 0, ab = 0;
        auto lag = std::size_t(std::round(48000 / (c.f0 * std::exp2(c.pitch / 12))));
        for (std::size_t i = out.size() / 2 + lag; i < out.size(); ++i) {
            aa += double(out[i]) * out[i];
            bb += double(out[i - lag]) * out[i - lag];
            ab += double(out[i]) * out[i - lag];
        }
        double first = -1, last = 0;
        int crossings = 0;
        for (std::size_t i = out.size() / 2 + 1; i < out.size(); ++i)
            if (out[i - 1] <= 0 && out[i] > 0) {
                double t = i - 1 - double(out[i - 1]) / (out[i] - out[i - 1]);
                if (first < 0)
                    first = t;
                last = t;
                ++crossings;
            }
        double hz = crossings > 1 ? (crossings - 1) * 48000 / (last - first) : 0;
        bool finite = std::all_of(out.begin(), out.end(), [](float x) { return std::isfinite(x); });
        std::printf("case%d engine%d F0%.0f center%.0f pitch%.0f formant%.0f "
                    "explicitScale%.9f output%zu expected%zu pad%zu delay%zu "
                    "finalAvail%d corr%.9f crossingsHz%.6f finite%d stalled%d",
                    caseNo, fx.getEngineVersion(), c.f0, c.center, c.pitch, c.formant,
                    fx.getFormantScale(), out.size(), n, fx.getPreferredStartPad(),
                    fx.getStartDelay(), fx.available(), ab / std::sqrt(aa * bb), hz, finite,
                    invalid);
        if (c.center == 0)
            std::printf(" sineCents%.6f", 1200 * std::log2(hz / (c.f0 * std::exp2(c.pitch / 12))));
        std::printf("\n");
        if (!finite || invalid || out.size() != n || aa <= 0 || bb <= 0)
            return 1;
        ++caseNo;
    }
}
