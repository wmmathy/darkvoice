#pragma once
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
namespace darkvoice::bandlimited_colour_detail {
struct Kernel {
    std::array<double, 513> values{};
    std::size_t count = 0;
    double &operator[](std::size_t i) noexcept {
        return values[i];
    }
    double operator[](std::size_t i) const noexcept {
        return values[i];
    }
};
inline Kernel kernel(std::size_t taps) noexcept {
    Kernel h{};
    h.count = taps;
    const int half = static_cast<int>((taps - 1) / 2);
    double sum = 0;
    for (int n = 0; n < static_cast<int>(taps); ++n) {
        const int m = n - half;
        const double window =
            .42 - .5 * std::cos(2 * std::numbers::pi * n / static_cast<double>(taps - 1)) +
            .08 * std::cos(4 * std::numbers::pi * n / static_cast<double>(taps - 1));
        h[static_cast<std::size_t>(n)] =
            m == 0       ? .5
            : m % 2 == 0 ? 0
                         : std::sin(std::numbers::pi * m / 2) / (std::numbers::pi * m) * window;
        sum += h[static_cast<std::size_t>(n)];
    }
    for (std::size_t i = 0; i < taps; ++i)
        h[i] /= sum;
    return h;
}
struct Interpolator {
    Kernel h{};
    std::array<double, 257> history{};
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
        const auto half = (h.count - 1) / 2;
        const double even = 2 * h[half] * read(half / 2);
        double odd = 0;
        for (std::size_t j = 0; j < (h.count - 1) / 2; ++j)
            odd += 2 * h[2 * j + 1] * read(j);
        position = position + 1 == history.size() ? 0 : position + 1;
        return {even, odd};
    }
};
struct Decimator {
    Kernel h{};
    std::array<double, 513> history{};
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
        const auto half = (h.count - 1) / 2;
        double value = h[half] * read(half);
        for (std::size_t j = 0; j < (h.count - 1) / 2; ++j)
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
    void prepare(const Kernel &boundary, const Kernel &higher) noexcept {
        for (std::size_t i = 0; i < 3; ++i) {
            up[i].prepare(i == 0 ? boundary : higher);
            down[i].prepare(i == 0 ? boundary : higher);
        }
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
            const double even = shape(values[0]);
            const double odd = shape(values[1]);
            c[i] = down[2].process(even, odd);
        }
        const double first = down[1].process(c[0], c[1]);
        const double second = down[1].process(c[2], c[3]);
        return down[0].process(first, second);
    }
};
} // namespace darkvoice::bandlimited_colour_detail
