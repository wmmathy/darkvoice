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
inline constexpr std::size_t gateParameterCount = 24;
inline constexpr std::size_t deEsserParameterCount = 27;
inline constexpr std::array<ParameterSpec, 65> parameterSchema{{
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
    {"deEssHz", "De-esser frequency", 2000.0f, 12000.0f, 6000.0f, 1.0f, 0.6f, false},
    {"deEssThresholdDb", "De-esser threshold", -60.0f, 0.0f, -24.0f, 0.1f, 1.0f, false},
    {"deEssReductionDb", "De-esser max reduction (0 = off)", 0.0f, 18.0f, 0.0f, 0.1f, 1.0f, false},
    {"chorusRateHz", "Chorus rate", .1f, 5.0f, .8f, .01f, .5f, false},
    {"chorusDepthMs", "Chorus depth", 0.0f, 12.0f, 6.0f, .1f, 1.0f, false},
    {"chorusMix", "Chorus mix", 0.0f, .5f, 0.0f, .001f, 1.0f, false},
    {"stereoWidth", "Stereo width", 0.0f, 2.0f, 1.0f, .01f, 1.0f, false},
    {"flangerRateHz", "Flanger rate", .05f, 5.0f, .3f, .01f, .5f, false},
    {"flangerDepthMs", "Flanger depth", 0.0f, 4.0f, 2.0f, .1f, 1.0f, false},
    {"flangerFeedback", "Flanger feedback", -.85f, .85f, 0.0f, .001f, 1.0f, false},
    {"flangerMix", "Flanger mix", 0.0f, 1.0f, 0.0f, .001f, 1.0f, false},
    {"limiterEnabled", "Limiter enabled (0 = off, 1 = on)", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, true},
    {"limiterCeilingDb", "Limiter ceiling", -24.0f, 0.0f, -1.0f, .1f, 1.0f, false},
    {"limiterReleaseMs", "Limiter release", 5.0f, 1000.0f, 100.0f, 1.0f, .5f, false},
    {"stereoDelayMode", "Stereo delay mode", 0, 2, 0, 1, 1, false},
    {"stereoDelayMs", "Stereo delay manual time", 20, 2000, 100, 1, .5f, false},
    {"stereoDelayFeedback", "Stereo delay feedback", 0, .8f, .25f, .001f, 1, false},
    {"stereoDelayMix", "Stereo delay mix", 0, .5f, 0, .001f, 1, false},
    {"stereoDelaySync", "Stereo delay sync (0 = off, 1 = on)", 0, 1, 0, 1, 1, true},
    {"stereoDelayBpm", "Stereo delay manual BPM", 20, 300, 120, .1f, 1, false},
    {"stereoDelayDivision", "Stereo delay beat division", 0, 5, 5, 1, 1, false},
    {"stereoDelayHostTempo", "Stereo delay use host BPM (0 = off, 1 = on)", 0, 1, 0, 1, 1, true},
    {"doublerSeparationMs", "Doubler separation", 10, 40, 20, .1f, 1, false},
    {"doublerVariationMs", "Doubler variation", 0, 6, 2, .1f, 1, false},
    {"doublerRateHz", "Doubler variation rate", .1f, 2, .4f, .01f, .5f, false},
    {"doublerMix", "Doubler mix", 0, .5f, 0, .001f, 1, false},
    {"spaceModel", "Space model", 0, 2, 0, 1, 1, false},
    {"spaceDecay", "Space decay", 0, 1, .5f, .001f, 1, false},
    {"spaceToneHz", "Space HF tone", 1000, 16000, 8000, 1, .5f, false},
    {"spacePreDelayMs", "Space predelay", 0, 100, 0, .1f, 1, false},
    {"spaceMix", "Space mix", 0, .5f, 0, .001f, 1, false},
    {"colourModel", "Colour style", 0, 1, 0, 1, 1, false},
    {"colourDrive", "Colour drive", 0, 1, .25f, .001f, 1, false},
    {"colourToneHz", "Colour HF tone", 1000, 20000, 16000, 1, .5f, false},
    {"colourMix", "Colour parallel mix", 0, 1, 0, .001f, 1, false},
    {"sculptBodyDb", "Body 180 Hz", -6, 6, 0, .1f, 1, false},
    {"sculptWarmthDb", "Warmth tilt", -6, 6, 0, .1f, 1, false},
    {"sculptPresenceDb", "Presence 3 kHz", -6, 6, 0, .1f, 1, false},
    {"sculptBreathiness", "Breath texture", 0, 1, 0, .001f, 1, false},
    {"sculptDepthDb", "Depth lows", -6, 6, 0, .1f, 1, false},
    {"sculptResonanceDb", "Resonance 700 Hz", -6, 6, 0, .1f, 1, false},
}};
inline constexpr int currentSchemaVersion = 14;
constexpr std::size_t parameterCountForVersion(int version) noexcept {
    return version == 1    ? legacyParameterCount
           : version == 2  ? effectsParameterCount
           : version == 3  ? gateParameterCount
           : version == 4  ? deEsserParameterCount
           : version == 5  ? 31
           : version == 6  ? 35
           : version == 7  ? 38
           : version == 8  ? 46
           : version == 9  ? 50
           : version == 10 ? 55
           : version == 11 ? 59
           : version == 12 ? 62
           : version == 13 ? 63
           : version == 14 ? parameterSchema.size()
                           : 0;
}
constexpr int parameterVersion(std::size_t index) noexcept {
    return index < legacyParameterCount    ? 1
           : index < effectsParameterCount ? 2
           : index < gateParameterCount    ? 3
           : index < deEsserParameterCount ? 4
           : index < 31                    ? 5
           : index < 35                    ? 6
           : index < 38                    ? 7
           : index < 46                    ? 8
           : index < 50                    ? 9
           : index < 55                    ? 10
           : index < 59                    ? 11
           : index < 62                    ? 12
           : index < 63                    ? 13
                                           : 14;
}
} // namespace darkvoice
