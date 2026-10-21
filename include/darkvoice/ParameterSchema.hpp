#pragma once
#include <array>
#include <cstddef>
namespace darkvoice {
struct ParameterSpec {
    const char *id;
    const char *name;
    float minimum, maximum, defaultValue, step, skew;
    bool boolean;
};
inline constexpr std::size_t legacyParameterCount = 8;
inline constexpr std::size_t effectsParameterCount = 21;
inline constexpr std::array<ParameterSpec, 24> parameterSchema{{
    {"inputDb", "Input", -24.0f, 24.0f, 0.0f, 0.1f, 1.0f, false},
    {"outputDb", "Output", -24.0f, 24.0f, 0.0f, 0.1f, 1.0f, false},
    {"highPassHz", "High-pass", 20.0f, 400.0f, 20.0f, 1.0f, 0.5f, false},
    {"drive", "Warmth drive", 0.0f, 1.0f, 0.0f, 0.001f, 1.0f, false},
    {"thresholdDb", "Compressor threshold", -60.0f, 0.0f, -18.0f, 0.1f, 1.0f, false},
    {"ratio", "Compressor ratio", 1.0f, 20.0f, 1.0f, 0.1f, 1.0f, false},
    {"mix", "Dry / wet", 0.0f, 1.0f, 1.0f, 0.001f, 1.0f, false},
    {"bypass", "Bypass", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, true},
    {"eqLowHz", "Low shelf frequency", 60.0f, 600.0f, 150.0f, 1.0f, 0.5f, false},
    {"eqLowGainDb", "Low shelf gain", -12.0f, 12.0f, 0.0f, 0.1f, 1.0f, false},
    {"eqMidHz", "Mid frequency", 200.0f, 5000.0f, 1200.0f, 1.0f, 0.4f, false},
    {"eqMidGainDb", "Mid gain", -12.0f, 12.0f, 0.0f, 0.1f, 1.0f, false},
    {"eqMidQ", "Mid Q", 0.2f, 6.0f, 0.7f, 0.01f, 0.5f, false},
    {"eqHighHz", "High shelf frequency", 1500.0f, 12000.0f, 6000.0f, 1.0f, 0.6f, false},
    {"eqHighGainDb", "High shelf gain", -12.0f, 12.0f, 0.0f, 0.1f, 1.0f, false},
    {"delayMs", "Delay time", 20.0f, 1000.0f, 180.0f, 1.0f, 0.5f, false},
    {"delayFeedback", "Delay feedback", 0.0f, 0.8f, 0.25f, 0.001f, 1.0f, false},
    {"delayMix", "Delay mix", 0.0f, 0.5f, 0.0f, 0.001f, 1.0f, false},
    {"reverbSize", "Reverb size", 0.0f, 1.0f, 0.5f, 0.001f, 1.0f, false},
    {"reverbDamping", "Reverb damping", 0.0f, 1.0f, 0.5f, 0.001f, 1.0f, false},
    {"reverbMix", "Reverb mix", 0.0f, 0.5f, 0.0f, 0.001f, 1.0f, false},
    {"gateThresholdDb", "Gate threshold", -80.0f, 0.0f, -50.0f, 0.1f, 1.0f, false},
    {"gateRatio", "Gate ratio (1 = off)", 1.0f, 20.0f, 1.0f, 0.1f, 1.0f, false},
    {"gateReleaseMs", "Gate release", 10.0f, 1000.0f, 120.0f, 1.0f, 0.5f, false},
}};
inline constexpr int currentSchemaVersion = 3;
constexpr std::size_t parameterCountForVersion(int version) noexcept {
    return version == 1   ? legacyParameterCount
           : version == 2 ? effectsParameterCount
           : version == 3 ? parameterSchema.size()
                          : 0;
}
constexpr int parameterVersion(std::size_t index) noexcept {
    return index < legacyParameterCount ? 1 : index < effectsParameterCount ? 2 : 3;
}
} // namespace darkvoice
