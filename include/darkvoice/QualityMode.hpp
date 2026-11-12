#pragma once
namespace darkvoice {
enum class QualityMode : int { Live = 0, Studio = 1, Offline = 2 };
constexpr bool validQualityMode(QualityMode mode) noexcept {
    return mode == QualityMode::Live || mode == QualityMode::Studio || mode == QualityMode::Offline;
}
} // namespace darkvoice
