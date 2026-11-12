#pragma once
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
namespace darkvoice::colour_detail {
using Kernel = std::array<double, 65>;
inline Kernel kernel() noexcept {
    Kernel h{};
    double sum = 0;
    for (int n = 0; n < 65; ++n) {
        const int m = n - 32;
        const double window = .42 - .5 * std::cos(2 * std::numbers::pi * n / 64) +
                              .08 * std::cos(4 * std::numbers::pi * n / 64);
        h[static_cast<std::size_t>(n)] =
            m == 0       ? .5
            : m % 2 == 0 ? 0
                         : std::sin(std::numbers::pi * m / 2) / (std::numbers::pi * m) * window;
        sum += h[static_cast<std::size_t>(n)];
    }
    for (auto &value : h)
        value /= sum;
    return h;
}
struct Interpolator {
    Kernel h{};
    std::array<double, 33> history{};
    std::size_t position = 0;
    void prepare(const Kernel &coefficients) noexcept {
        h = coefficients;
        reset();
    }
    void reset() noexcept {
        history = {};
        position = 0;
    }
    double read(std::size_t age) const noexcept {
        return history[position >= age ? position - age : position + history.size() - age];
    }
    std::array<double, 2> process(double x) noexcept {
        history[position] = x;
        const double even = 2 * h[32] * read(16);
        double odd = 0;
        for (std::size_t j = 0; j < 32; ++j)
            odd += 2 * h[2 * j + 1] * read(j);
        position = position + 1 == history.size() ? 0 : position + 1;
        return {even, odd};
    }
};
struct Decimator {
    Kernel h{};
    std::array<double, 65> history{};
    std::size_t position = 0;
    void prepare(const Kernel &coefficients) noexcept {
        h = coefficients;
        reset();
    }
    void reset() noexcept {
        history = {};
        position = 0;
    }
    double read(std::size_t age) const noexcept {
        return history[position >= age ? position - age : position + history.size() - age];
    }
    double process(double even, double odd) noexcept {
        history[position] = even;
        double value = h[32] * read(32);
        for (std::size_t j = 0; j < 32; ++j)
            value += h[2 * j + 1] * read(2 * j + 1);
        position = position + 1 == history.size() ? 0 : position + 1;
        history[position] = odd;
        position = position + 1 == history.size() ? 0 : position + 1;
        return value;
    }
};
struct Oversampler8 {
    std::array<Interpolator, 3> up;
    std::array<Decimator, 3> down;
    void prepare(const Kernel &h) noexcept {
        for (auto &stage : up)
            stage.prepare(h);
        for (auto &stage : down)
            stage.prepare(h);
    }
    void reset() noexcept {
        for (auto &stage : up)
            stage.reset();
        for (auto &stage : down)
            stage.reset();
    }
    template <class Shape> double process(double x, Shape &&shape) noexcept {
        const auto a = up[0].process(x);
        std::array<double, 4> b{};
        for (std::size_t i = 0; i < 2; ++i) {
            const auto values = up[1].process(a[i]);
            b[2 * i] = values[0];
            b[2 * i + 1] = values[1];
        }
        std::array<double, 4> c{};
        for (std::size_t i = 0; i < 4; ++i) {
            const auto values = up[2].process(b[i]);
            c[i] = down[2].process(shape(values[0]), shape(values[1]));
        }
        const double first = down[1].process(c[0], c[1]);
        const double second = down[1].process(c[2], c[3]);
        return down[0].process(first, second);
    }
};
} // namespace darkvoice::colour_detail
