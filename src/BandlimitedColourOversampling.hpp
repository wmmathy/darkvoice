#pragma once
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
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
// Native Nyquist guard: .225 cycles per 2x sample = .45 native Fs.
// This is intentionally NOT halfband: every coefficient participates.
inline Kernel guardKernel(std::size_t taps) noexcept {
    Kernel h{};
    h.count = taps;
    const int half = static_cast<int>((taps - 1) / 2);
    double sum = 0;
    for (std::size_t n = 0; n < taps; ++n) {
        const int k = static_cast<int>(n) - half;
        const double phase = 2 * std::numbers::pi * n / static_cast<double>(taps - 1);
        const double window = .42 - .5 * std::cos(phase) + .08 * std::cos(2 * phase);
        h[n] =
            (k == 0 ? .45 : std::sin(.45 * std::numbers::pi * k) / (std::numbers::pi * k)) * window;
        sum += h[n];
    }
    for (std::size_t n = 0; n < taps; ++n)
        h[n] /= sum;
    return h;
}
// Prepared sparse lists retain every nonzero coefficient in ascending tap order.
// Structural halfband zeros are skipped without a per-tap callback branch.
struct Interpolator {
    std::array<double, 257> evenCoefficients{}, oddCoefficients{}, history{};
    std::array<std::uint16_t, 257> evenAges{}, oddAges{};
    std::size_t evenCount = 0, oddCount = 0, position = 0;
    void prepare(const Kernel &h) noexcept {
        evenCount = oddCount = 0;
        for (std::size_t tap = 0; tap < h.count; ++tap) {
            if (h[tap] != 0) {
                if (tap % 2 == 0) {
                    evenCoefficients[evenCount] = 2 * h[tap];
                    evenAges[evenCount++] = static_cast<std::uint16_t>(tap / 2);
                } else {
                    oddCoefficients[oddCount] = 2 * h[tap];
                    oddAges[oddCount++] = static_cast<std::uint16_t>(tap / 2);
                }
            }
        }
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
        double even = 0, odd = 0;
        for (std::size_t tap = 0; tap < evenCount; ++tap)
            even += evenCoefficients[tap] * read(evenAges[tap]);
        for (std::size_t tap = 0; tap < oddCount; ++tap)
            odd += oddCoefficients[tap] * read(oddAges[tap]);
        position = position + 1 == history.size() ? 0 : position + 1;
        return {even, odd};
    }
};
struct Decimator {
    std::array<double, 513> coefficients{}, history{};
    std::array<std::uint16_t, 513> ages{};
    std::size_t count = 0, position = 0;
    void prepare(const Kernel &h) noexcept {
        count = 0;
        for (std::size_t tap = 0; tap < h.count; ++tap) {
            if (h[tap] != 0) {
                coefficients[count] = h[tap];
                ages[count++] = static_cast<std::uint16_t>(tap);
            }
        }
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
        double value = 0;
        for (std::size_t tap = 0; tap < count; ++tap)
            value += coefficients[tap] * read(ages[tap]);
        position = position + 1 == history.size() ? 0 : position + 1;
        history[position] = odd;
        position = position + 1 == history.size() ? 0 : position + 1;
        return value;
    }
};
struct Oversampler16 {
    std::array<Interpolator, 4> up;
    std::array<Decimator, 4> down;
    void prepare(const Kernel &boundary, const Kernel &higher, const Kernel &guard) noexcept {
        for (std::size_t i = 0; i < 4; ++i) {
            up[i].prepare(i == 0 ? boundary : higher);
            down[i].prepare(i == 0 ? guard : higher);
        }
    }
    void reset() noexcept {
        for (auto &stage : up)
            stage.reset();
        for (auto &stage : down)
            stage.reset();
    }
    template <class Shape> double process(double x, Shape &&shape) noexcept {
        std::array<double, 16> values{};
        values[0] = x;
        std::size_t count = 1;
        for (std::size_t stage = 0; stage < up.size(); ++stage) {
            std::array<double, 16> expanded{};
            for (std::size_t i = 0; i < count; ++i) {
                const auto pair = up[stage].process(values[i]);
                expanded[2 * i] = pair[0];
                expanded[2 * i + 1] = pair[1];
            }
            values = expanded;
            count *= 2;
        }
        for (auto &value : values)
            value = shape(value);
        for (std::size_t stage = down.size(); stage-- > 0;) {
            count /= 2;
            for (std::size_t i = 0; i < count; ++i)
                values[i] = down[stage].process(values[2 * i], values[2 * i + 1]);
        }
        return values[0];
    }
};
} // namespace darkvoice::bandlimited_colour_detail
