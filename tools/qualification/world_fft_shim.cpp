// SPDX-License-Identifier: AGPL-3.0-only
// Independently authored radix-2 DFT adapter for WORLD's BSD-licensed FFT API.
// No implementation from WORLD src/fft.cpp or Ooura FFT is used.
#include "world/fft.h"

#include <cmath>
#include <memory>
#include <numbers>
#include <stdexcept>

namespace {
fft_plan makePlan(int n, int sign, unsigned flags) {
    if (n < 2 || (n & (n - 1)) != 0 || (sign != FFT_FORWARD && sign != FFT_BACKWARD))
        throw std::invalid_argument("FFT requires power-of-two N >= 2 and valid direction");
    fft_plan p{};
    p.n = n;
    p.sign = sign;
    p.flags = flags;
    auto scratch = std::make_unique<double[]>(2 * static_cast<std::size_t>(n));
    auto permutation = std::make_unique<int[]>(static_cast<std::size_t>(n));
    auto twiddles = std::make_unique<double[]>(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        int value = i, reverse = 0;
        for (int width = n; width > 1; width >>= 1) {
            reverse = (reverse << 1) | (value & 1);
            value >>= 1;
        }
        permutation[i] = reverse;
    }
    for (int i = 0; i < n / 2; ++i) {
        const double angle = 2 * std::numbers::pi * i / n;
        twiddles[2 * i] = std::cos(angle);
        twiddles[2 * i + 1] = std::sin(angle);
    }
    p.input = scratch.release();
    p.ip = permutation.release();
    p.w = twiddles.release();
    return p;
}
} // namespace

extern "C" {
fft_plan fft_plan_dft_1d(int n, fft_complex *in, fft_complex *out, int sign, unsigned flags) {
    auto p = makePlan(n, sign, flags);
    p.c_in = in;
    p.c_out = out;
    return p;
}
fft_plan fft_plan_dft_r2c_1d(int n, double *in, fft_complex *out, unsigned flags) {
    auto p = makePlan(n, FFT_FORWARD, flags);
    p.in = in;
    p.c_out = out;
    return p;
}
fft_plan fft_plan_dft_c2r_1d(int n, fft_complex *in, double *out, unsigned flags) {
    auto p = makePlan(n, FFT_BACKWARD, flags);
    p.c_in = in;
    p.out = out;
    return p;
}
void fft_execute(fft_plan p) {
    // Fully copy into permuted scratch before emitting anything (in-place safe).
    for (int i = 0; i < p.n; ++i) {
        double real = 0, imag = 0;
        if (p.in) {
            real = p.in[i];
        } else if (p.out && i > p.n / 2) {
            real = p.c_in[p.n - i][0];
            imag = -p.c_in[p.n - i][1];
        } else {
            real = p.c_in[i][0];
            imag = p.c_in[i][1];
            if (p.out && (i == 0 || i == p.n / 2))
                imag = 0;
        }
        p.input[2 * p.ip[i]] = real;
        p.input[2 * p.ip[i] + 1] = imag;
    }
    for (int width = 2;; width *= 2) {
        const int half = width / 2, step = p.n / width;
        for (int start = 0; start < p.n; start += width) {
            for (int j = 0; j < half; ++j) {
                const int a = 2 * (start + j), b = 2 * (start + j + half);
                const double wr = p.w[2 * j * step];
                const double wi = (p.sign == FFT_FORWARD ? -1 : 1) * p.w[2 * j * step + 1];
                const double tr = wr * p.input[b] - wi * p.input[b + 1];
                const double ti = wr * p.input[b + 1] + wi * p.input[b];
                const double ar = p.input[a], ai = p.input[a + 1];
                p.input[a] = ar + tr;
                p.input[a + 1] = ai + ti;
                p.input[b] = ar - tr;
                p.input[b + 1] = ai - ti;
            }
        }
        if (width == p.n)
            break;
    }
    const int count = p.out ? p.n : (p.in ? p.n / 2 + 1 : p.n);
    for (int i = 0; i < count; ++i) {
        if (p.out)
            p.out[i] = p.input[2 * i];
        else {
            p.c_out[i][0] = p.input[2 * i];
            p.c_out[i][1] = p.input[2 * i + 1];
        }
    }
}
void fft_destroy_plan(fft_plan p) {
    delete[] p.input;
    delete[] p.ip;
    delete[] p.w;
}
}
