#pragma once
#include <algorithm>
#include <cstddef>
#include <string_view>

namespace darkvoice {
// Validate small decimal integers without floating-point rounding or underflow.
// Callers bound input length and retain their enclosing JSON/XML grammar checks.
inline bool exactSmallIntegerNumber(std::string_view text, int maximum) noexcept {
    if (maximum < 0 || maximum > 9)
        return false;
    if (text.size() > 1024 * 1024)
        return false;
    std::size_t position = 0;
    bool negative = false;
    if (!text.empty() && (text.front() == '-' || text.front() == '+')) {
        negative = text.front() == '-';
        ++position;
    }
    long digits = 0, beforePoint = -1, onePosition = -1;
    bool multipleNonzeroDigits = false;
    int nonzeroDigit = 0;
    while (position < text.size()) {
        const auto c = text[position];
        if (c >= '0' && c <= '9') {
            if (c != '0') {
                if (onePosition != -1)
                    multipleNonzeroDigits = true;
                nonzeroDigit = c - '0';
                onePosition = digits;
            }
            ++digits;
        } else if (c == '.' && beforePoint == -1) {
            beforePoint = digits;
        } else {
            break;
        }
        ++position;
    }
    if (digits == 0)
        return false;
    if (beforePoint == -1)
        beforePoint = digits;
    long exponent = 0;
    bool exponentNegative = false;
    if (position < text.size() && (text[position] == 'e' || text[position] == 'E')) {
        ++position;
        if (position < text.size() && (text[position] == '+' || text[position] == '-')) {
            exponentNegative = text[position] == '-';
            ++position;
        }
        const auto start = position;
        while (position < text.size() && text[position] >= '0' && text[position] <= '9') {
            // Inputs are at most 1 MiB; a larger exponent cannot turn a nonzero
            // mantissa into exactly one. Saturation avoids integer overflow.
            exponent = std::min(2'000'000L, exponent * 10 + text[position] - '0');
            ++position;
        }
        if (position == start)
            return false;
    }
    if (position != text.size() || multipleNonzeroDigits)
        return false;
    if (onePosition == -1)
        return true;
    return !negative && nonzeroDigit <= maximum &&
           beforePoint - onePosition - 1 + (exponentNegative ? -exponent : exponent) == 0;
}
inline bool exactBooleanNumber(std::string_view text) noexcept {
    return exactSmallIntegerNumber(text, 1);
}
} // namespace darkvoice
