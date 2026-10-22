// Developer-only measurement probe; see docs/RUBBERBAND_AUDIT.md.
#include "rubberband/RubberBandLiveShifter.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <vector>
using RubberBand::RubberBandLiveShifter;
int main() {
    struct Case {
        double f0, center, pitch, formant;
    };
    for (bool medium : {false, true})
        for (auto c : {Case{100, 600, 12, 0}, Case{70, 600, -12, 0}, Case{180, 1200, 12, 7}}) {
            int flags = RubberBandLiveShifter::OptionFormantPreserved;
            if (medium)
                flags |= RubberBandLiveShifter::OptionWindowMedium;
            RubberBandLiveShifter fx(48000, 1, flags);
            fx.setPitchScale(std::exp2(c.pitch / 12));
            if (c.formant != 0)
                fx.setFormantScale(std::exp2((c.formant - c.pitch) / 12));
            const auto block = fx.getBlockSize();
            const auto n = ((96000 + block - 1) / block) * block;
            std::vector<float> in(n), out(n);
            for (int h = 1; h <= 80; ++h) {
                double f = h * c.f0;
                double amp = .025 * (std::exp(-.5 * std::pow((f - c.center) / 100, 2)) + .002 / h);
                for (size_t i = 0; i < 96000; ++i)
                    in[i] +=
                        static_cast<float>(amp * std::sin(2 * std::numbers::pi * f * i / 48000));
            }
            for (size_t i = 0; i < n; i += block) {
                const float *a[] = {in.data() + i};
                float *b[] = {out.data() + i};
                fx.shift(a, b);
            }
            char file[160];
            std::snprintf(file, sizeof(file), "/tmp/darkvoice-rb-spectrum-%d-%.0f.raw", medium,
                          c.f0);
            auto f = std::fopen(file, "wb");
            if (!f) {
                std::fprintf(stderr, "Cannot write measurement output\n");
                return 1;
            }
            const bool written =
                std::fwrite(out.data(), sizeof(float), out.size(), f) == out.size();
            const bool closed = std::fclose(f) == 0;
            if (!written || !closed) {
                std::fprintf(stderr, "Measurement output failed\n");
                return 1;
            }
            auto lag = static_cast<size_t>(std::round(48000 / (c.f0 * std::exp2(c.pitch / 12))));
            double aa = 0, bb = 0, ab = 0, peak = 0;
            bool finite = std::all_of(out.begin(), out.end(),
                                      [](float value) { return std::isfinite(value); });
            for (size_t i = n / 2 + lag; i < n; ++i) {
                aa += double(out[i]) * out[i];
                bb += double(out[i - lag]) * out[i - lag];
                ab += double(out[i]) * out[i - lag];
                peak = std::max(peak, std::abs(double(out[i])));
                finite = finite && std::isfinite(out[i]);
            }
            if (!finite || aa <= 0 || bb <= 0) {
                std::fprintf(stderr, "Invalid measurement signal\n");
                return 1;
            }
            std::printf("medium %d F0 %.0f center %.0f pitch %.0f formant %.0f block %zu delay %zu "
                        "correlation %.9f peak %.6f finite %d\n",
                        medium, c.f0, c.center, c.pitch, c.formant, block, fx.getStartDelay(),
                        ab / std::sqrt(aa * bb), peak, finite);
        }
}
