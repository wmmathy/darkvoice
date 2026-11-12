#include "PluginProcessor.hpp"
#include "PresetBank.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <complex>
#include <cstdint>
#include <deque>
#include <iostream>
#include <limits>
#include <locale>
#include <stdexcept>

namespace {
constexpr int latency = 56;
struct CommaDecimal : std::numpunct<char> {
    char do_decimal_point() const override {
        return ',';
    }
};
struct ScopedCommaLocale {
    std::locale previous =
        std::locale::global(std::locale(std::locale::classic(), new CommaDecimal));
    ~ScopedCommaLocale() {
        std::locale::global(previous);
    }
};
void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
void set(DarkVoiceProcessor &p, const char *id, float actual) {
    auto *parameter = p.parameters.getParameter(id);
    require(parameter != nullptr, "Missing stable parameter ID");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(actual));
}
float raw(DarkVoiceProcessor &p, const char *id) {
    return p.parameters.getRawParameterValue(id)->load();
}
void layout(DarkVoiceProcessor &p, int channels) {
    auto buses = p.getBusesLayout();
    buses.inputBuses.set(0, channels == 1 ? juce::AudioChannelSet::mono()
                                          : juce::AudioChannelSet::stereo());
    buses.outputBuses.set(0, buses.inputBuses[0]);
    require(p.setBusesLayout(buses), "Supported layout rejected");
}
void sine(juce::AudioBuffer<float> &buffer, double rate, int start = 0) {
    for (int c = 0; c < buffer.getNumChannels(); ++c)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            buffer.setSample(c, i,
                             0.1f *
                                 std::sin(static_cast<float>(2.0 * juce::MathConstants<double>::pi *
                                                             1000.0 * (start + i) / rate)));
}
void stateTests() {
    DarkVoiceProcessor source;
    const auto &feedbackRange =
        source.parameters.getParameter("flangerFeedback")->getNormalisableRange();
    require(feedbackRange.convertTo0to1(0) == .5f && feedbackRange.convertFrom0to1(.5f) == 0 &&
                feedbackRange.snapToLegalValue(0) == 0,
            "Signed feedback preserves exact neutral under normalisation and snapping");
    for (float value : {.001f, .4f, .85f}) {
        require(feedbackRange.snapToLegalValue(-value) == -feedbackRange.snapToLegalValue(value),
                "Signed feedback quantisation is symmetric");
    }
    std::cout << "Start-relative fused feedback rounding residual: "
              << std::fma(.001f, 850.f, -.85f) << '\n';
    for (const auto *id : {"sculptBodyDb", "sculptWarmthDb", "sculptPresenceDb"}) {
        require(raw(source, id) == 0, "Tone defaults must be exactly neutral");
        const auto &range = source.parameters.getParameter(id)->getNormalisableRange();
        require(range.convertTo0to1(0) == .5f && range.convertFrom0to1(.5f) == 0 &&
                    range.snapToLegalValue(0) == 0,
                "Signed tone controls preserve exact zero under conversion and snapping");
        for (float value : {-6.f, -2.3f, -.1f, .1f, 2.3f, 6.f}) {
            set(source, id, value);
            require(std::abs(raw(source, id) - value) < 1e-6f,
                    "Signed nonzero tone values and endpoints roundtrip");
            require(range.snapToLegalValue(-value) == -range.snapToLegalValue(value),
                    "Signed tone quantisation stays symmetric");
        }
        set(source, id, 0);
        require(raw(source, id) == 0, "Setting zero must restore exact neutral tone");
    }
    set(source, "inputDb", 7.5f);
    set(source, "drive", 0.6f);
    set(source, "ratio", 4.0f);
    set(source, "bypass", 1.0f);
    set(source, "eqMidGainDb", 3.5f);
    set(source, "delayMix", 0.2f);
    set(source, "reverbMix", 0.1f);
    set(source, "gateThresholdDb", -45.0f);
    set(source, "gateRatio", 3.0f);
    set(source, "gateReleaseMs", 250.0f);
    set(source, "deEssHz", 5000);
    set(source, "deEssThresholdDb", -30);
    set(source, "deEssReductionDb", 9);
    set(source, "chorusRateHz", 2.5f);
    set(source, "chorusDepthMs", 10);
    set(source, "chorusMix", .25f);
    set(source, "stereoWidth", 1.7f);
    set(source, "flangerRateHz", 1.3f);
    set(source, "flangerDepthMs", 3);
    set(source, "flangerFeedback", -.4f);
    set(source, "flangerMix", .6f);
    set(source, "limiterEnabled", 1);
    set(source, "limiterCeilingDb", -8.5f);
    set(source, "limiterReleaseMs", 345);
    set(source, "stereoDelayMode", 2);
    set(source, "stereoDelayMs", 420);
    set(source, "stereoDelayFeedback", .6f);
    set(source, "stereoDelayMix", .2f);
    set(source, "stereoDelaySync", 1);
    set(source, "stereoDelayBpm", 80);
    set(source, "stereoDelayDivision", 3);
    set(source, "stereoDelayHostTempo", 1);
    set(source, "doublerSeparationMs", 31.5f);
    set(source, "doublerVariationMs", 4.5f);
    set(source, "doublerRateHz", 1.3f);
    set(source, "doublerMix", .3f);
    set(source, "spaceModel", 2);
    set(source, "spaceDecay", .7f);
    set(source, "spaceToneHz", 5300);
    set(source, "spacePreDelayMs", 17.5f);
    set(source, "spaceMix", .2f);
    set(source, "colourModel", 1);
    set(source, "colourDrive", .7f);
    set(source, "colourToneHz", 8200);
    set(source, "colourMix", .6f);
    set(source, "sculptBodyDb", -4.5f);
    set(source, "sculptWarmthDb", 2.3f);
    set(source, "sculptPresenceDb", 5.2f);
    juce::MemoryBlock state;
    source.getStateInformation(state);
    require(state.getSize() > 0, "State serialization empty");
    DarkVoiceProcessor restored;
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    require(std::abs(raw(restored, "inputDb") - 7.5f) < 0.01f, "Input state not restored");
    require(std::abs(raw(restored, "drive") - 0.6f) < 0.001f, "Drive state not restored");
    require(std::abs(raw(restored, "ratio") - 4.0f) < 0.01f, "Ratio state not restored");
    require(raw(restored, "bypass") == 1.0f, "Bypass state not restored");
    for (std::size_t i = 0; i < darkvoice::parameterSchema.size(); ++i) {
        const auto &spec = darkvoice::parameterSchema[i];
        require(source.parameters.getParameter(spec.id)->getVersionHint() == (i < 8    ? 1
                                                                              : i < 21 ? 2
                                                                              : i < 24 ? 3
                                                                              : i < 27 ? 4
                                                                              : i < 31 ? 5
                                                                              : i < 35 ? 6
                                                                              : i < 38 ? 7
                                                                              : i < 46 ? 8
                                                                              : i < 50 ? 9
                                                                              : i < 55 ? 10
                                                                              : i < 59 ? 11
                                                                                       : 12),
                "AU version hint changed");
        require(std::abs(raw(restored, spec.id) - raw(source, spec.id)) < 0.001f,
                "Full v12 state roundtrip failed");
    }
    require(source.getTailLengthSeconds() == 240.0, "Stereo musical delay tail not reported");

    const char garbage[] = "invalid state";
    restored.setStateInformation(garbage, sizeof(garbage));
    require(raw(restored, "inputDb") == raw(source, "inputDb"),
            "Malformed state changed parameters");
    for (const char *version : {"13", "1junk"}) {
        auto xml = source.parameters.copyState().createXml();
        xml->setAttribute("schemaVersion", version);
        juce::MemoryBlock invalid;
        juce::AudioProcessor::copyXmlToBinary(*xml, invalid);
        set(restored, "inputDb", -5.0f);
        restored.setStateInformation(invalid.getData(), static_cast<int>(invalid.getSize()));
        require(std::abs(raw(restored, "inputDb") + 5.0f) < 0.01f,
                "Invalid schema changed parameters");
    }
    for (const char *value : {"NaN", "1.2.3", "100", "", ".", "--1", "1 2"}) {
        auto tree = source.parameters.copyState();
        tree.setProperty("schemaVersion", darkvoice::currentSchemaVersion, nullptr);
        tree.getChildWithProperty("id", "inputDb").setProperty("value", value, nullptr);
        juce::MemoryBlock invalid;
        juce::AudioProcessor::copyXmlToBinary(*tree.createXml(), invalid);
        restored.setStateInformation(invalid.getData(), static_cast<int>(invalid.getSize()));
        require(std::abs(raw(restored, "inputDb") + 5.0f) < 0.01f,
                "Invalid parameter state accepted");
    }
    auto scientificTree = source.parameters.copyState();
    scientificTree.setProperty("schemaVersion", darkvoice::currentSchemaVersion, nullptr);
    scientificTree.getChildWithProperty("id", "inputDb").setProperty("value", "1e1", nullptr);
    juce::MemoryBlock scientific;
    juce::AudioProcessor::copyXmlToBinary(*scientificTree.createXml(), scientific);
    restored.setStateInformation(scientific.getData(), static_cast<int>(scientific.getSize()));
    require(std::abs(raw(restored, "inputDb") - 10.0f) < 0.01f, "Valid scientific value rejected");
    {
        ScopedCommaLocale nonEnglishLocale;
        set(source, "inputDb", -3.5f);
        juce::MemoryBlock decimal;
        source.getStateInformation(decimal);
        restored.setStateInformation(decimal.getData(), static_cast<int>(decimal.getSize()));
        require(std::abs(raw(restored, "inputDb") + 3.5f) < 0.01f,
                "State parsing depended on global decimal locale");
    }
    // A legacy project contains exactly the original eight IDs. New effects
    // must reset to defaults even when restoring into a modified v2 instance.
    auto legacy = source.parameters.copyState();
    legacy.setProperty("schemaVersion", 1, nullptr);
    for (int child = legacy.getNumChildren() - 1; child >= 0; --child)
        if (legacy.getChild(child).getProperty("id").toString().startsWith("space") ||
            legacy.getChild(child).getProperty("id").toString().startsWith("colour") ||
            legacy.getChild(child).getProperty("id").toString().startsWith("sculpt"))
            legacy.removeChild(child, nullptr);
    for (int child = legacy.getNumChildren() - 1; child >= 0; --child)
        if (legacy.getChild(child).getProperty("id").toString().startsWith("doubler"))
            legacy.removeChild(child, nullptr);
    for (int child = legacy.getNumChildren() - 1; child >= 0; --child)
        if (legacy.getChild(child).getProperty("id").toString().startsWith("stereoDelay"))
            legacy.removeChild(child, nullptr);
    for (const auto *id : {"limiterEnabled", "limiterCeilingDb", "limiterReleaseMs"})
        legacy.removeChild(legacy.getChildWithProperty("id", id), nullptr);
    for (int index = legacy.getNumChildren() - 1; index >= 0; --index) {
        const auto id = legacy.getChild(index).getProperty("id").toString();
        bool original = false;
        for (std::size_t i = 0; i < darkvoice::legacyParameterCount; ++i)
            original = original || id == darkvoice::parameterSchema[i].id;
        if (!original)
            legacy.removeChild(index, nullptr);
    }
    set(restored, "delayMix", 0.4f);
    set(restored, "eqMidGainDb", 5.0f);
    juce::MemoryBlock legacyData;
    juce::AudioProcessor::copyXmlToBinary(*legacy.createXml(), legacyData);
    restored.setStateInformation(legacyData.getData(), static_cast<int>(legacyData.getSize()));
    require(std::abs(raw(restored, "inputDb") - raw(source, "inputDb")) < 0.01f,
            "Legacy input not restored");
    for (std::size_t i = darkvoice::legacyParameterCount; i < darkvoice::parameterSchema.size();
         ++i) {
        const auto &spec = darkvoice::parameterSchema[i];
        require(std::abs(raw(restored, spec.id) - spec.defaultValue) < 0.001f,
                "Legacy effect defaults not restored");
    }
    auto v2 = source.parameters.copyState();
    v2.setProperty("schemaVersion", 2, nullptr);
    for (int child = v2.getNumChildren() - 1; child >= 0; --child)
        if (v2.getChild(child).getProperty("id").toString().startsWith("space") ||
            v2.getChild(child).getProperty("id").toString().startsWith("colour") ||
            v2.getChild(child).getProperty("id").toString().startsWith("sculpt"))
            v2.removeChild(child, nullptr);
    for (int child = v2.getNumChildren() - 1; child >= 0; --child)
        if (v2.getChild(child).getProperty("id").toString().startsWith("doubler"))
            v2.removeChild(child, nullptr);
    for (int child = v2.getNumChildren() - 1; child >= 0; --child)
        if (v2.getChild(child).getProperty("id").toString().startsWith("stereoDelay"))
            v2.removeChild(child, nullptr);
    for (const auto *id : {"limiterEnabled", "limiterCeilingDb", "limiterReleaseMs"})
        v2.removeChild(v2.getChildWithProperty("id", id), nullptr);
    for (const auto *id : {"flangerRateHz", "flangerDepthMs", "flangerFeedback", "flangerMix"})
        v2.removeChild(v2.getChildWithProperty("id", id), nullptr);
    for (const auto *id :
         {"gateThresholdDb", "gateRatio", "gateReleaseMs", "deEssHz", "deEssThresholdDb",
          "deEssReductionDb", "chorusRateHz", "chorusDepthMs", "chorusMix", "stereoWidth"})
        v2.removeChild(v2.getChildWithProperty("id", id), nullptr);
    set(restored, "gateRatio", 10);
    set(restored, "chorusRateHz", 4);
    set(restored, "chorusDepthMs", 12);
    set(restored, "chorusMix", .5f);
    set(restored, "stereoWidth", 2);
    set(restored, "deEssHz", 9000);
    set(restored, "deEssThresholdDb", -50);
    set(restored, "deEssReductionDb", 12);
    juce::MemoryBlock v2Data;
    juce::AudioProcessor::copyXmlToBinary(*v2.createXml(), v2Data);
    restored.setStateInformation(v2Data.getData(), static_cast<int>(v2Data.getSize()));
    require(raw(restored, "gateRatio") == 1 && raw(restored, "gateThresholdDb") == -50 &&
                std::abs(raw(restored, "eqMidGainDb") - raw(source, "eqMidGainDb")) < .001f,
            "V2 state preserves effects and disables gate");
    require(raw(restored, "deEssHz") == 6000 && raw(restored, "deEssThresholdDb") == -24 &&
                raw(restored, "deEssReductionDb") == 0,
            "V2 state did not reset active de-esser");
    require(raw(restored, "chorusMix") == 0 && raw(restored, "stereoWidth") == 1 &&
                raw(restored, "chorusDepthMs") == 6 &&
                std::abs(raw(restored, "chorusRateHz") - .8f) < .001f,
            "V2 state resets modulation");
    auto v3 = source.parameters.copyState();
    v3.setProperty("schemaVersion", 3, nullptr);
    for (int child = v3.getNumChildren() - 1; child >= 0; --child)
        if (v3.getChild(child).getProperty("id").toString().startsWith("space") ||
            v3.getChild(child).getProperty("id").toString().startsWith("colour") ||
            v3.getChild(child).getProperty("id").toString().startsWith("sculpt"))
            v3.removeChild(child, nullptr);
    for (int child = v3.getNumChildren() - 1; child >= 0; --child)
        if (v3.getChild(child).getProperty("id").toString().startsWith("doubler"))
            v3.removeChild(child, nullptr);
    for (int child = v3.getNumChildren() - 1; child >= 0; --child)
        if (v3.getChild(child).getProperty("id").toString().startsWith("stereoDelay"))
            v3.removeChild(child, nullptr);
    for (const auto *id : {"limiterEnabled", "limiterCeilingDb", "limiterReleaseMs"})
        v3.removeChild(v3.getChildWithProperty("id", id), nullptr);
    for (const auto *id : {"flangerRateHz", "flangerDepthMs", "flangerFeedback", "flangerMix"})
        v3.removeChild(v3.getChildWithProperty("id", id), nullptr);
    for (const auto *id : {"deEssHz", "deEssThresholdDb", "deEssReductionDb", "chorusRateHz",
                           "chorusDepthMs", "chorusMix", "stereoWidth"})
        v3.removeChild(v3.getChildWithProperty("id", id), nullptr);
    set(restored, "deEssHz", 9000);
    set(restored, "deEssThresholdDb", -50);
    set(restored, "deEssReductionDb", 12);
    set(restored, "chorusRateHz", 4);
    set(restored, "chorusDepthMs", 12);
    set(restored, "chorusMix", .5f);
    set(restored, "stereoWidth", 2);
    juce::MemoryBlock v3Data;
    juce::AudioProcessor::copyXmlToBinary(*v3.createXml(), v3Data);
    restored.setStateInformation(v3Data.getData(), static_cast<int>(v3Data.getSize()));
    require(raw(restored, "deEssReductionDb") == 0 && raw(restored, "deEssHz") == 6000 &&
                raw(restored, "deEssThresholdDb") == -24 && raw(restored, "gateRatio") == 3,
            "V3 state preserves gate and disables de-esser");
    require(raw(restored, "chorusMix") == 0 && raw(restored, "stereoWidth") == 1 &&
                raw(restored, "chorusDepthMs") == 6 &&
                std::abs(raw(restored, "chorusRateHz") - .8f) < .001f,
            "V3 state resets modulation");
    auto v4 = source.parameters.copyState();
    v4.setProperty("schemaVersion", 4, nullptr);
    for (int child = v4.getNumChildren() - 1; child >= 0; --child)
        if (v4.getChild(child).getProperty("id").toString().startsWith("space") ||
            v4.getChild(child).getProperty("id").toString().startsWith("colour") ||
            v4.getChild(child).getProperty("id").toString().startsWith("sculpt"))
            v4.removeChild(child, nullptr);
    for (int child = v4.getNumChildren() - 1; child >= 0; --child)
        if (v4.getChild(child).getProperty("id").toString().startsWith("doubler"))
            v4.removeChild(child, nullptr);
    for (int child = v4.getNumChildren() - 1; child >= 0; --child)
        if (v4.getChild(child).getProperty("id").toString().startsWith("stereoDelay"))
            v4.removeChild(child, nullptr);
    for (const auto *id : {"limiterEnabled", "limiterCeilingDb", "limiterReleaseMs"})
        v4.removeChild(v4.getChildWithProperty("id", id), nullptr);
    for (const auto *id : {"flangerRateHz", "flangerDepthMs", "flangerFeedback", "flangerMix"})
        v4.removeChild(v4.getChildWithProperty("id", id), nullptr);
    for (const auto *id : {"chorusRateHz", "chorusDepthMs", "chorusMix", "stereoWidth"})
        v4.removeChild(v4.getChildWithProperty("id", id), nullptr);
    set(restored, "chorusRateHz", 4);
    set(restored, "chorusDepthMs", 12);
    set(restored, "chorusMix", .5f);
    set(restored, "stereoWidth", 2);
    juce::MemoryBlock v4Data;
    juce::AudioProcessor::copyXmlToBinary(*v4.createXml(), v4Data);
    restored.setStateInformation(v4Data.getData(), static_cast<int>(v4Data.getSize()));
    require(std::abs(raw(restored, "chorusRateHz") - .8f) < .001f &&
                raw(restored, "chorusDepthMs") == 6 && raw(restored, "chorusMix") == 0 &&
                raw(restored, "stereoWidth") == 1 && raw(restored, "deEssReductionDb") == 9,
            "V4 state retains de-esser and resets modulation defaults");
    auto v5 = source.parameters.copyState();
    v5.setProperty("schemaVersion", 5, nullptr);
    for (int child = v5.getNumChildren() - 1; child >= 0; --child)
        if (v5.getChild(child).getProperty("id").toString().startsWith("space") ||
            v5.getChild(child).getProperty("id").toString().startsWith("colour") ||
            v5.getChild(child).getProperty("id").toString().startsWith("sculpt"))
            v5.removeChild(child, nullptr);
    for (int child = v5.getNumChildren() - 1; child >= 0; --child)
        if (v5.getChild(child).getProperty("id").toString().startsWith("doubler"))
            v5.removeChild(child, nullptr);
    for (int child = v5.getNumChildren() - 1; child >= 0; --child)
        if (v5.getChild(child).getProperty("id").toString().startsWith("stereoDelay"))
            v5.removeChild(child, nullptr);
    for (const auto *id : {"limiterEnabled", "limiterCeilingDb", "limiterReleaseMs"})
        v5.removeChild(v5.getChildWithProperty("id", id), nullptr);
    for (const auto *id : {"flangerRateHz", "flangerDepthMs", "flangerFeedback", "flangerMix"})
        v5.removeChild(v5.getChildWithProperty("id", id), nullptr);
    set(restored, "flangerRateHz", 4);
    set(restored, "flangerDepthMs", 4);
    set(restored, "flangerFeedback", .8f);
    set(restored, "flangerMix", 1);
    juce::MemoryBlock v5Data;
    juce::AudioProcessor::copyXmlToBinary(*v5.createXml(), v5Data);
    restored.setStateInformation(v5Data.getData(), static_cast<int>(v5Data.getSize()));
    require(std::abs(raw(restored, "flangerRateHz") - .3f) < .001f &&
                raw(restored, "flangerDepthMs") == 2 && raw(restored, "flangerFeedback") == 0 &&
                raw(restored, "flangerMix") == 0 &&
                std::abs(raw(restored, "chorusMix") - .25f) < .001f,
            "V5 state preserves chorus and resets active flanger defaults");
    for (int version = 1; version <= 6; ++version) {
        auto old = source.parameters.copyState();
        old.setProperty("schemaVersion", version, nullptr);
        for (int child = old.getNumChildren() - 1; child >= 0; --child) {
            const auto id = old.getChild(child).getProperty("id").toString();
            bool keep = false;
            for (std::size_t i = 0; i < darkvoice::parameterCountForVersion(version); ++i)
                keep = keep || id == darkvoice::parameterSchema[i].id;
            if (!keep)
                old.removeChild(child, nullptr);
        }
        set(restored, "limiterEnabled", 1);
        set(restored, "limiterCeilingDb", -20);
        set(restored, "limiterReleaseMs", 950);
        set(restored, "flangerRateHz", 5);
        set(restored, "flangerDepthMs", 4);
        set(restored, "flangerFeedback", -.8f);
        set(restored, "flangerMix", 1);
        juce::MemoryBlock bytes;
        juce::AudioProcessor::copyXmlToBinary(*old.createXml(), bytes);
        restored.setStateInformation(bytes.getData(), static_cast<int>(bytes.getSize()));
        require(raw(restored, "limiterEnabled") == 0 &&
                    std::abs(raw(restored, "limiterCeilingDb") + 1) < .001f &&
                    raw(restored, "limiterReleaseMs") == 100,
                "Every legacy state clears active limiter and initializes safe defaults");
        if (version <= 5)
            require(std::abs(raw(restored, "flangerRateHz") - .3f) < .001f &&
                        raw(restored, "flangerDepthMs") == 2 &&
                        raw(restored, "flangerFeedback") == 0 && raw(restored, "flangerMix") == 0,
                    "Every legacy state clears active flanger fields");
    }
    for (int version = 1; version <= 7; ++version) {
        auto old = source.parameters.copyState();
        old.setProperty("schemaVersion", version, nullptr);
        for (int child = old.getNumChildren() - 1; child >= 0; --child) {
            bool keep = false;
            for (std::size_t i = 0; i < darkvoice::parameterCountForVersion(version); ++i)
                keep = keep || old.getChild(child).getProperty("id").toString() ==
                                   darkvoice::parameterSchema[i].id;
            if (!keep)
                old.removeChild(child, nullptr);
        }
        set(restored, "stereoDelayMix", .5f);
        set(restored, "stereoDelaySync", 1);
        set(restored, "stereoDelayHostTempo", 1);
        set(restored, "stereoDelayMode", 2);
        juce::MemoryBlock bytes;
        juce::AudioProcessor::copyXmlToBinary(*old.createXml(), bytes);
        restored.setStateInformation(bytes.getData(), static_cast<int>(bytes.getSize()));
        for (std::size_t i = 38; i < darkvoice::parameterSchema.size(); ++i) {
            const auto &spec = darkvoice::parameterSchema[i];
            require(std::abs(raw(restored, spec.id) - spec.defaultValue) < .001f,
                    "Legacy state initializes all eight stereo delay controls");
        }
        require(restored.getTailLengthSeconds() == 40.0,
                "Migrated neutral delay retains forty-second tail");
        if (version == 7)
            require(raw(restored, "limiterEnabled") == 1, "V7 migration preserves limiter enable");
    }
    for (const auto *id :
         {"stereoDelaySync", "stereoDelayHostTempo", "stereoDelayMode", "stereoDelayDivision"})
        for (const auto *token : {"0.5", "1.00000001", "1e-500", "NaN", "6", "-1"}) {
            auto bad = source.parameters.copyState();
            bad.setProperty("schemaVersion", darkvoice::currentSchemaVersion, nullptr);
            bad.getChildWithProperty("id", id).setProperty("value", token, nullptr);
            const auto before = restored.snapshotPresetValues();
            juce::MemoryBlock bytes;
            juce::AudioProcessor::copyXmlToBinary(*bad.createXml(), bytes);
            restored.setStateInformation(bytes.getData(), static_cast<int>(bytes.getSize()));
            require(restored.snapshotPresetValues() == before,
                    "Invalid stereo delay Boolean/discrete state changed live snapshot");
        }
    for (int version = 1; version <= 8; ++version) {
        auto old = source.parameters.copyState();
        old.setProperty("schemaVersion", version, nullptr);
        for (int child = old.getNumChildren() - 1; child >= 0; --child) {
            bool keep = false;
            for (std::size_t i = 0; i < darkvoice::parameterCountForVersion(version); ++i)
                keep = keep || old.getChild(child).getProperty("id").toString() ==
                                   darkvoice::parameterSchema[i].id;
            if (!keep)
                old.removeChild(child, nullptr);
        }
        set(restored, "doublerSeparationMs", 40);
        set(restored, "doublerVariationMs", 6);
        set(restored, "doublerRateHz", 2);
        set(restored, "doublerMix", .5f);
        juce::MemoryBlock bytes;
        juce::AudioProcessor::copyXmlToBinary(*old.createXml(), bytes);
        restored.setStateInformation(bytes.getData(), static_cast<int>(bytes.getSize()));
        for (std::size_t i = 46; i < darkvoice::parameterSchema.size(); ++i) {
            const auto &spec = darkvoice::parameterSchema[i];
            require(std::abs(raw(restored, spec.id) - spec.defaultValue) < .001f,
                    "Every legacy state resets four simulated doubler defaults");
        }
        if (version == 8)
            require(std::abs(raw(restored, "stereoDelayMix") - .2f) < .001f &&
                        raw(restored, "stereoDelaySync") == 1 &&
                        raw(restored, "limiterEnabled") == 1,
                    "V8 migration preserves prior delay and limiter settings");
    }
    for (const auto *id :
         {"doublerSeparationMs", "doublerVariationMs", "doublerRateHz", "doublerMix"})
        for (const auto *invalid : {"NaN", "1e100", "-1", "100"}) {
            auto bad = source.parameters.copyState();
            bad.setProperty("schemaVersion", darkvoice::currentSchemaVersion, nullptr);
            bad.getChildWithProperty("id", id).setProperty("value", invalid, nullptr);
            const auto before = restored.snapshotPresetValues();
            juce::MemoryBlock bytes;
            juce::AudioProcessor::copyXmlToBinary(*bad.createXml(), bytes);
            restored.setStateInformation(bytes.getData(), static_cast<int>(bytes.getSize()));
            require(restored.snapshotPresetValues() == before,
                    "Invalid doubler state changes no live controls");
        }
    for (int version = 1; version <= 9; ++version) {
        auto old = source.parameters.copyState();
        old.setProperty("schemaVersion", version, nullptr);
        for (int child = old.getNumChildren() - 1; child >= 0; --child) {
            bool keep = false;
            for (std::size_t i = 0; i < darkvoice::parameterCountForVersion(version); ++i)
                keep = keep || old.getChild(child).getProperty("id").toString() ==
                                   darkvoice::parameterSchema[i].id;
            if (!keep)
                old.removeChild(child, nullptr);
        }
        set(restored, "spaceModel", 2);
        set(restored, "spaceDecay", 1);
        set(restored, "spaceToneHz", 16000);
        set(restored, "spacePreDelayMs", 100);
        set(restored, "spaceMix", .5f);
        juce::MemoryBlock bytes;
        juce::AudioProcessor::copyXmlToBinary(*old.createXml(), bytes);
        restored.setStateInformation(bytes.getData(), static_cast<int>(bytes.getSize()));
        for (std::size_t i = 50; i < darkvoice::parameterSchema.size(); ++i) {
            const auto &spec = darkvoice::parameterSchema[i];
            require(std::abs(raw(restored, spec.id) - spec.defaultValue) < .001f,
                    "Every legacy state initializes five Space Reverb defaults");
        }
        if (version == 9)
            require(std::abs(raw(restored, "doublerMix") - .3f) < .001f &&
                        std::abs(raw(restored, "stereoDelayMix") - .2f) < .001f &&
                        raw(restored, "limiterEnabled") == 1,
                    "V9 migration preserves prior effects");
    }
    for (const auto *invalid : {"0.5", "1.00000001", "1e-500", "NaN", "3", "-1"}) {
        auto bad = source.parameters.copyState();
        bad.setProperty("schemaVersion", darkvoice::currentSchemaVersion, nullptr);
        bad.getChildWithProperty("id", "spaceModel").setProperty("value", invalid, nullptr);
        const auto before = restored.snapshotPresetValues();
        juce::MemoryBlock bytes;
        juce::AudioProcessor::copyXmlToBinary(*bad.createXml(), bytes);
        restored.setStateInformation(bytes.getData(), static_cast<int>(bytes.getSize()));
        require(restored.snapshotPresetValues() == before,
                "Invalid Space model lexical state changed live snapshot");
    }
    for (const auto *id : {"spaceDecay", "spaceToneHz", "spacePreDelayMs", "spaceMix"})
        for (const auto *invalid : {"NaN", "1e100", "-1", "100000"}) {
            auto bad = source.parameters.copyState();
            bad.setProperty("schemaVersion", darkvoice::currentSchemaVersion, nullptr);
            bad.getChildWithProperty("id", id).setProperty("value", invalid, nullptr);
            const auto before = restored.snapshotPresetValues();
            juce::MemoryBlock bytes;
            juce::AudioProcessor::copyXmlToBinary(*bad.createXml(), bytes);
            restored.setStateInformation(bytes.getData(), static_cast<int>(bytes.getSize()));
            require(restored.snapshotPresetValues() == before,
                    "Invalid Space float state changed live snapshot");
        }
    for (int version = 1; version <= 10; ++version) {
        auto old = source.parameters.copyState();
        old.setProperty("schemaVersion", version, nullptr);
        for (int child = old.getNumChildren() - 1; child >= 0; --child) {
            bool keep = false;
            for (std::size_t i = 0; i < darkvoice::parameterCountForVersion(version); ++i)
                keep = keep || old.getChild(child).getProperty("id").toString() ==
                                   darkvoice::parameterSchema[i].id;
            if (!keep)
                old.removeChild(child, nullptr);
        }
        set(restored, "colourModel", 1);
        set(restored, "colourDrive", 1);
        set(restored, "colourToneHz", 1000);
        set(restored, "colourMix", 1);
        juce::MemoryBlock bytes;
        juce::AudioProcessor::copyXmlToBinary(*old.createXml(), bytes);
        restored.setStateInformation(bytes.getData(), static_cast<int>(bytes.getSize()));
        for (std::size_t i = 55; i < darkvoice::parameterSchema.size(); ++i) {
            const auto &spec = darkvoice::parameterSchema[i];
            require(std::abs(raw(restored, spec.id) - spec.defaultValue) < .001f,
                    "Every legacy state initializes four neutral Colour fields");
        }
        if (version == 10)
            require(std::abs(raw(restored, "spaceMix") - .2f) < .001f, "V10 preserves Space sound");
    }
    for (int version = 1; version <= 11; ++version) {
        auto old = source.parameters.copyState();
        old.setProperty("schemaVersion", version, nullptr);
        for (int child = old.getNumChildren() - 1; child >= 0; --child) {
            bool keep = false;
            for (std::size_t i = 0; i < darkvoice::parameterCountForVersion(version); ++i)
                keep = keep || old.getChild(child).getProperty("id").toString() ==
                                   darkvoice::parameterSchema[i].id;
            if (!keep)
                old.removeChild(child, nullptr);
        }
        for (const auto *id : {"sculptBodyDb", "sculptWarmthDb", "sculptPresenceDb"})
            set(restored, id, 6);
        juce::MemoryBlock bytes;
        juce::AudioProcessor::copyXmlToBinary(*old.createXml(), bytes);
        restored.setStateInformation(bytes.getData(), static_cast<int>(bytes.getSize()));
        for (std::size_t i = 59; i < darkvoice::parameterSchema.size(); ++i)
            require(raw(restored, darkvoice::parameterSchema[i].id) == 0,
                    "Every legacy state initializes neutral VoiceSculptor tone");
        for (std::size_t i = 0; i < darkvoice::parameterCountForVersion(version); ++i) {
            const auto &spec = darkvoice::parameterSchema[i];
            require(std::abs(raw(restored, spec.id) - raw(source, spec.id)) < .001f,
                    "Tone migration preserves every accepted prior state field");
        }
    }
    for (const auto *id : {"sculptBodyDb", "sculptWarmthDb", "sculptPresenceDb"})
        for (const auto *invalid : {"NaN", "1e100", "-6.01", "6.01"}) {
            auto bad = source.parameters.copyState();
            bad.setProperty("schemaVersion", darkvoice::currentSchemaVersion, nullptr);
            bad.getChildWithProperty("id", id).setProperty("value", invalid, nullptr);
            const auto before = restored.snapshotPresetValues();
            juce::MemoryBlock bytes;
            juce::AudioProcessor::copyXmlToBinary(*bad.createXml(), bytes);
            restored.setStateInformation(bytes.getData(), static_cast<int>(bytes.getSize()));
            require(restored.snapshotPresetValues() == before,
                    "Invalid VoiceSculptor tone state must be atomic");
        }
    for (const auto *invalid : {"0.5", "1.00000001", "1e-500", "NaN", "2", "-1"}) {
        auto bad = source.parameters.copyState();
        bad.setProperty("schemaVersion", darkvoice::currentSchemaVersion, nullptr);
        bad.getChildWithProperty("id", "colourModel").setProperty("value", invalid, nullptr);
        const auto before = restored.snapshotPresetValues();
        juce::MemoryBlock bytes;
        juce::AudioProcessor::copyXmlToBinary(*bad.createXml(), bytes);
        restored.setStateInformation(bytes.getData(), static_cast<int>(bytes.getSize()));
        require(restored.snapshotPresetValues() == before,
                "Invalid Colour style lexical state changed controls");
    }
    for (const auto *id : {"colourDrive", "colourToneHz", "colourMix"})
        for (const auto *invalid : {"NaN", "1e100", "-1", "100000"}) {
            auto bad = source.parameters.copyState();
            bad.setProperty("schemaVersion", darkvoice::currentSchemaVersion, nullptr);
            bad.getChildWithProperty("id", id).setProperty("value", invalid, nullptr);
            const auto before = restored.snapshotPresetValues();
            juce::MemoryBlock bytes;
            juce::AudioProcessor::copyXmlToBinary(*bad.createXml(), bytes);
            restored.setStateInformation(bytes.getData(), static_cast<int>(bytes.getSize()));
            require(restored.snapshotPresetValues() == before,
                    "Invalid Colour float state changed controls");
        }
    set(restored, "gateRatio", 1);
    for (const char *badGate : {"NaN", "21", "0"}) {
        auto bad = source.parameters.copyState();
        bad.setProperty("schemaVersion", darkvoice::currentSchemaVersion, nullptr);
        bad.getChildWithProperty("id", "gateRatio").setProperty("value", badGate, nullptr);
        juce::MemoryBlock data;
        juce::AudioProcessor::copyXmlToBinary(*bad.createXml(), data);
        restored.setStateInformation(data.getData(), static_cast<int>(data.getSize()));
        require(raw(restored, "gateRatio") == 1, "Invalid gate state changed live state");
    }
    for (const char *badReduction : {"NaN", "19", "-1"}) {
        auto bad = source.parameters.copyState();
        bad.setProperty("schemaVersion", darkvoice::currentSchemaVersion, nullptr);
        bad.getChildWithProperty("id", "deEssReductionDb")
            .setProperty("value", badReduction, nullptr);
        juce::MemoryBlock data;
        juce::AudioProcessor::copyXmlToBinary(*bad.createXml(), data);
        const auto before = restored.snapshotPresetValues();
        restored.setStateInformation(data.getData(), static_cast<int>(data.getSize()));
        require(restored.snapshotPresetValues() == before,
                "Invalid de-esser state changed live parameters");
    }
    for (const auto *invalidEnable : {"0.5", "-1", "2", "NaN", "1.00000001", "0.99999999", "1e-500",
                                      "1.00000000000000000001"}) {
        auto bad = source.parameters.copyState();
        bad.setProperty("schemaVersion", darkvoice::currentSchemaVersion, nullptr);
        bad.getChildWithProperty("id", "limiterEnabled")
            .setProperty("value", invalidEnable, nullptr);
        const auto before = restored.snapshotPresetValues();
        juce::MemoryBlock binary;
        juce::AudioProcessor::copyXmlToBinary(*bad.createXml(), binary);
        restored.setStateInformation(binary.getData(), static_cast<int>(binary.getSize()));
        require(restored.snapshotPresetValues() == before,
                "Invalid limiter boolean changed live snapshot");
    }
    for (int fault = 0; fault < 3; ++fault) {
        auto invalidTree = source.parameters.copyState();
        invalidTree.setProperty("schemaVersion", darkvoice::currentSchemaVersion, nullptr);
        if (fault == 0)
            invalidTree.removeChild(0, nullptr);
        if (fault == 1)
            invalidTree.getChild(1).setProperty("id", "inputDb", nullptr);
        if (fault == 2)
            invalidTree.getChild(1).setProperty("id", "unknown", nullptr);
        juce::MemoryBlock invalid;
        juce::AudioProcessor::copyXmlToBinary(*invalidTree.createXml(), invalid);
        set(restored, "inputDb", -12.0f);
        restored.setStateInformation(invalid.getData(), static_cast<int>(invalid.getSize()));
        require(std::abs(raw(restored, "inputDb") + 12.0f) < 0.01f,
                "Invalid ID set partially changed state");
    }
    auto values = source.snapshotPresetValues();
    require(values.size() == 61 && values.count("bypass") == 0,
            "Preset must contain only sound parameters");
    values["inputDb"] = 10.0f;
    values["delayMix"] = 0.2f;
    set(restored, "bypass", 1.0f);
    require(restored.applyPreset(values), "Valid preset not applied");
    require(raw(restored, "bypass") == 1.0f, "Preset changed bypass");
    require(std::abs(raw(restored, "delayMix") - 0.2f) < 0.001f,
            "Effect preset parameter not applied");
    values["delayMix"] = 10.0f;
    values["inputDb"] = -20.0f;
    require(!restored.applyPreset(values), "Invalid preset applied");
    require(std::abs(raw(restored, "inputDb") - 10.0f) < 0.01f,
            "Invalid preset partially changed state");
}
std::complex<double> toneTransfer(double rate, double frequency, double body, double warmth,
                                  double presence) {
    const auto z =
        std::exp(std::complex<double>(0, -2 * juce::MathConstants<double>::pi * frequency / rate));
    const auto band = [&](double center, double q) {
        const double w = 2 * juce::MathConstants<double>::pi * center / rate;
        const double alpha = std::sin(w) / (2 * q);
        return alpha * (1. - z * z) / ((1 + alpha) - 2 * std::cos(w) * z + (1 - alpha) * z * z);
    };
    const double k = std::tan(juce::MathConstants<double>::pi * 450 / rate);
    const auto low = k * (1. + z) / ((1 + k) + (k - 1) * z);
    const double a = std::pow(10., warmth / 40), b = std::pow(10., -warmth / 40);
    return (1. + (std::pow(10., body / 20) - 1) * band(180, .85)) * (b + (a - b) * low) *
           (1. + (std::pow(10., presence / 20) - 1) * band(3000, .7));
}
void sculptorToneAudioTests() {
    juce::MidiBuffer midi;
    for (double rate : {44100., 48000., 88200., 96000.})
        for (int channels : {1, 2})
            for (double frequency : {180., 450., 3000.}) {
                const auto render = [&](float body, float warmth, float presence) {
                    DarkVoiceProcessor processor;
                    layout(processor, channels);
                    set(processor, "sculptBodyDb", body);
                    set(processor, "sculptWarmthDb", warmth);
                    set(processor, "sculptPresenceDb", presence);
                    processor.prepareToPlay(rate, 73);
                    // Let the mandatory 20Hz high-pass and tonal filters settle before
                    // fitting steady-state transfer. A fixed 2048-frame preroll was
                    // too short at higher rates and mixed startup decay into the oracle.
                    const int warmFrames = static_cast<int>(std::ceil(.2 * rate));
                    const int totalFrames = warmFrames + 2048;
                    // Least-squares sine/cosine fitting avoids a whole-cycle/RMS assumption.
                    double ss = 0, cc = 0, sc = 0, ys = 0, yc = 0;
                    for (int offset = 0; offset < totalFrames;) {
                        const int frames = std::min(257, totalFrames - offset);
                        juce::AudioBuffer<float> audio(channels, frames);
                        for (int c = 0; c < channels; ++c)
                            for (int i = 0; i < frames; ++i)
                                audio.setSample(
                                    c, i,
                                    static_cast<float>(
                                        .15 * std::sin(2 * juce::MathConstants<double>::pi *
                                                       frequency * (offset + i) / rate)));
                        processor.processBlock(audio, midi);
                        for (int i = 0; i < frames; ++i) {
                            const auto sample = audio.getSample(0, i);
                            require(std::isfinite(sample),
                                    "Actual VoiceSculptor tone audio must be finite");
                            if (channels == 2)
                                require(sample == audio.getSample(1, i),
                                        "Identical stereo tone inputs retain channel equality");
                            if (offset + i < warmFrames)
                                continue;
                            const double phase = 2 * juce::MathConstants<double>::pi * frequency *
                                                 (offset + i) / rate;
                            const double sn = std::sin(phase), cs = std::cos(phase);
                            ss += sn * sn;
                            cc += cs * cs;
                            sc += sn * cs;
                            ys += sample * sn;
                            yc += sample * cs;
                        }
                        offset += frames;
                    }
                    require(processor.getLatencySamples() == latency,
                            "VoiceSculptor tone leaves fixed Colour latency unchanged");
                    const double determinant = ss * cc - sc * sc;
                    const auto fitted = std::complex<double>((ys * cc - yc * sc) / determinant,
                                                             (yc * ss - ys * sc) / determinant);
                    const auto expected = toneTransfer(
                        rate, frequency, raw(processor, "sculptBodyDb"),
                        raw(processor, "sculptWarmthDb"), raw(processor, "sculptPresenceDb"));
                    return std::pair{fitted, expected};
                };
                const auto neutral = render(0, 0, 0).first;
                require(std::abs(neutral) > .01,
                        "Neutral paired host tone reference remains audible");
                for (const auto settings :
                     {std::array<float, 3>{6, 0, 0}, {0, 6, 0}, {0, 0, 6}, {-3, 4, -5}}) {
                    const auto [actual, expected] = render(settings[0], settings[1], settings[2]);
                    if (std::abs(actual / neutral - expected) >= 2e-5)
                        std::cerr << "Tone oracle case rate=" << rate << " channels=" << channels
                                  << " frequency=" << frequency << " settings=" << settings[0]
                                  << ',' << settings[1] << ',' << settings[2]
                                  << " neutral=" << neutral << " actual=" << actual
                                  << " ratio=" << actual / neutral << " expected=" << expected
                                  << " error=" << std::abs(actual / neutral - expected) << '\n';
                    require(std::abs(actual / neutral - expected) < 2e-5,
                            "Actual host tone transfer differs from independent paired analytic "
                            "oracle");
                }
            }
}

// Three matched processors isolate the actual host Colour parallel route without
// assuming nominal APVTS 0dB is represented exactly on every target architecture.
void colourAudioTests() {
    for (double rate : {44100., 48000., 88200., 96000.})
        for (int channels : {1, 2})
            for (int model : {0, 1}) {
                DarkVoiceProcessor wet, dry, parallel, otherStyle, dark;
                for (auto *p : {&wet, &dry, &parallel, &otherStyle, &dark}) {
                    layout(*p, channels);
                    set(*p, "colourModel", static_cast<float>(model));
                    set(*p, "colourDrive", .9f);
                    set(*p, "colourToneHz", 16000);
                }
                set(wet, "colourMix", 1);
                set(parallel, "colourMix", .35f);
                set(otherStyle, "colourMix", 1);
                set(otherStyle, "colourModel", static_cast<float>(1 - model));
                set(dark, "colourMix", 1);
                set(dark, "colourToneHz", 1000);
                for (auto *p : {&wet, &dry, &parallel, &otherStyle, &dark})
                    p->prepareToPlay(rate, 64);
                const double mix = raw(parallel, "colourMix");
                double colourDifference = 0, styleDifference = 0, toneDifference = 0;
                juce::MidiBuffer midi;
                int position = 0;
                for (int block = 0; block < 12; ++block) {
                    const int frames = block % 2 ? 257 : 73;
                    juce::AudioBuffer<float> a(channels, frames), b(channels, frames),
                        c(channels, frames), d(channels, frames), e(channels, frames);
                    for (int channel = 0; channel < channels; ++channel)
                        for (int i = 0; i < frames; ++i) {
                            const double phase = (position + i) / rate;
                            const float input = static_cast<float>(
                                .25 * std::sin(2 * juce::MathConstants<double>::pi * 211 * phase) +
                                .12 * std::sin(2 * juce::MathConstants<double>::pi * 4103 * phase +
                                               channel));
                            for (auto *audio : {&a, &b, &c, &d, &e})
                                audio->setSample(channel, i, input);
                        }
                    wet.processBlock(a, midi);
                    dry.processBlock(b, midi);
                    parallel.processBlock(c, midi);
                    otherStyle.processBlock(d, midi);
                    dark.processBlock(e, midi);
                    for (int channel = 0; channel < channels; ++channel)
                        for (int i = 0; i < frames; ++i) {
                            const double w = a.getSample(channel, i),
                                         original = b.getSample(channel, i);
                            require(std::isfinite(w) && std::isfinite(c.getSample(channel, i)),
                                    "Host Colour audio must remain finite");
                            require(
                                std::abs(c.getSample(channel, i) -
                                         ((1 - mix) * original + mix * w)) < 2e-6,
                                "Actual Colour parallel mix must align dry and processed audio");
                            colourDifference += (w - original) * (w - original);
                            styleDifference += std::pow(w - d.getSample(channel, i), 2);
                            toneDifference += std::pow(w - e.getSample(channel, i), 2);
                        }
                    position += frames;
                    for (auto *p : {&wet, &dry, &parallel, &otherStyle, &dark})
                        require(p->getLatencySamples() == latency,
                                "Colour model/tone/mix must not change fixed host latency");
                }
                require(colourDifference > .01 && styleDifference > .001 && toneDifference > .01,
                        "Colour drive, style and tone must produce actual distinct host audio");
            }
}
void forcedBypassTests() {
    constexpr std::array<std::uint32_t, 12> bits{
        0U,          0x80000000U, 1U,          0x80000001U, 0x00800000U, 0x80800000U,
        0x7f800000U, 0xff800000U, 0x7fc12345U, 0xffc23456U, 0x3eaaaaabU, 0xbf123456U};
    juce::MidiBuffer midi;
    for (double rate : {44100., 48000., 88200., 96000.})
        for (int channels : {1, 2}) {
            DarkVoiceProcessor subject;
            layout(subject, channels);
            set(subject, "colourMix", 1);
            set(subject, "outputDb", 24);
            set(subject, "limiterEnabled", 1);
            subject.prepareToPlay(rate, 17);
            std::array<std::deque<std::uint32_t>, 2> history;
            for (auto &queue : history)
                queue.assign(latency, 0U);
            int position = 0;
            for (int block = 0; block < 12; ++block) {
                // Force-only, APVTS-only and both paths must share one raw queue;
                // the forced callback must not modify APVTS bypass or any sound value.
                const bool forced = block % 3 != 1;
                set(subject, "bypass", block % 3 == 0 ? 0.f : 1.f);
                const float acceptedBypass = raw(subject, "bypass");
                const auto acceptedSound = subject.snapshotPresetValues();
                juce::AudioBuffer<float> empty(channels, 0);
                subject.processBlock(empty, midi);
                subject.processBlockBypassed(empty, midi);
                const int frames = block % 2 ? 113 : 31;
                juce::AudioBuffer<float> audio(channels, frames);
                std::array<std::vector<std::uint32_t>, 2> expected;
                for (int channel = 0; channel < channels; ++channel)
                    for (int i = 0; i < frames; ++i) {
                        const auto rawBits =
                            bits[static_cast<std::size_t>(position + i + channel * 5) %
                                 bits.size()];
                        audio.setSample(channel, i, std::bit_cast<float>(rawBits));
                        auto &queue = history[static_cast<std::size_t>(channel)];
                        expected[static_cast<std::size_t>(channel)].push_back(queue.front());
                        queue.pop_front();
                        queue.push_back(rawBits);
                    }
                if (forced)
                    subject.processBlockBypassed(audio, midi);
                else
                    subject.processBlock(audio, midi);
                for (int channel = 0; channel < channels; ++channel)
                    for (int i = 0; i < frames; ++i)
                        require(std::bit_cast<std::uint32_t>(audio.getSample(channel, i)) ==
                                    expected[static_cast<std::size_t>(channel)]
                                            [static_cast<std::size_t>(i)],
                                "Host forced/APVTS bypass must preserve delayed float bits across "
                                "chunks");
                require(raw(subject, "bypass") == acceptedBypass &&
                            subject.snapshotPresetValues() == acceptedSound,
                        "Forced host bypass must not mutate parameter state");
                require(subject.getLatencySamples() == latency,
                        "Forced/APVTS bypass must preserve host latency");
                position += frames;
            }
        }
}

void spaceReverbAudioTests() {
    for (double rate : {44100., 48000., 88200., 96000.})
        for (int channels : {1, 2})
            for (int model : {0, 1, 2})
                for (float predelay : {0.f, 2.5f, 10.f}) {
                    DarkVoiceProcessor subject, reference;
                    layout(subject, channels);
                    layout(reference, channels);
                    set(subject, "spaceModel", static_cast<float>(model));
                    set(subject, "spaceMix", .5f);
                    set(subject, "spacePreDelayMs", predelay);
                    set(reference, "spaceModel", static_cast<float>(model));
                    subject.prepareToPlay(rate, 257);
                    reference.prepareToPlay(rate, 257);
                    juce::MidiBuffer midi;
                    const double preSamples = rate * raw(subject, "spacePreDelayMs") * .001;
                    const auto base = std::llround(rate * (model == 0   ? .003
                                                           : model == 1 ? .012
                                                                        : .0239));
                    const int first = static_cast<int>(latency + base + std::floor(preSamples));
                    std::vector<float> recovered(static_cast<std::size_t>(rate * .08)),
                        dry = recovered;
                    for (int position = 0; position < static_cast<int>(recovered.size());) {
                        const int frames = std::min(position % 2 ? 191 : 257,
                                                    static_cast<int>(recovered.size()) - position);
                        juce::AudioBuffer<float> a(channels, frames), b(channels, frames);
                        a.clear();
                        b.clear();
                        if (position == 0)
                            for (int c = 0; c < channels; ++c) {
                                a.setSample(c, 0, .8f);
                                b.setSample(c, 0, .8f);
                            }
                        subject.processBlock(a, midi);
                        reference.processBlock(b, midi);
                        for (int i = 0; i < frames; ++i) {
                            recovered[static_cast<std::size_t>(position + i)] =
                                2 * a.getSample(0, i) - b.getSample(0, i);
                            dry[static_cast<std::size_t>(position + i)] = b.getSample(0, i);
                            for (int c = 0; c < channels; ++c)
                                require(std::isfinite(a.getSample(c, i)),
                                        "Space model host render must remain finite");
                        }
                        position += frames;
                    }
                    for (int i = 0; i < first; ++i)
                        require(std::abs(recovered[static_cast<std::size_t>(i)]) < 2e-6f,
                                "Space predelay/model emitted an early wet arrival");
                    const double pole = std::exp(-2 * juce::MathConstants<double>::pi *
                                                 raw(subject, "spaceToneHz") / rate);
                    const double rawCoefficient =
                        model == 0   ? .18
                        : model == 1 ? .08
                                     : (1 - (.25 + .70 * raw(subject, "spaceDecay"))) / 8;
                    const double expected = dry[latency] * rawCoefficient * (1 - pole) *
                                            (1 - (preSamples - std::floor(preSamples)));
                    require(std::abs(recovered[static_cast<std::size_t>(first)] - expected) < 3e-6,
                            "Actual Space model/predelay/tone arrival differs from independent "
                            "coefficient oracle");
                    require(subject.getLatencySamples() == latency &&
                                subject.getTailLengthSeconds() == 60,
                            "Space musical predelay must not invent host latency or alter existing "
                            "tail");
                }
}

void spaceTailTests() {
    DarkVoiceProcessor p;
    require(p.getTailLengthSeconds() == 40, "Neutral effects preserve original forty-second tail");
    set(p, "spaceMix", .1f);
    require(p.getTailLengthSeconds() == 60, "Audible Space Reverb reports sixty-second tail");
    set(p, "stereoDelayMix", .1f);
    require(p.getTailLengthSeconds() == 240,
            "Musical stereo delay tail has precedence over Space Reverb");
    set(p, "spaceMix", 0);
    require(p.getTailLengthSeconds() == 240,
            "Neutral Space must not shorten audible musical delay tail");
    set(p, "spaceMix", .1f);
    set(p, "stereoDelayMix", 0);
    require(p.getTailLengthSeconds() == 60, "Disabling musical delay restores active Space tail");
    set(p, "spaceMix", 0);
    require(p.getTailLengthSeconds() == 40, "Neutral Space restores original forty-second tail");
}

void doublerAudioTests() {
    for (double rate : {44100., 48000., 88200., 96000.})
        for (int channels : {1, 2}) {
            DarkVoiceProcessor p, reference;
            layout(p, channels);
            layout(reference, channels);
            require(raw(p, "doublerMix") == 0, "Doubler is off by default");
            set(p, "doublerVariationMs", 0);
            set(p, "doublerSeparationMs", 20);
            set(p, "doublerMix", .5f);
            p.prepareToPlay(rate, 257);
            reference.prepareToPlay(rate, 257);
            juce::MidiBuffer midi;
            const int first = latency + static_cast<int>(std::round(rate * .020)),
                      second = latency + static_cast<int>(std::round(rate * .031));
            std::vector<float> actualL(static_cast<std::size_t>(rate * .07)), actualR = actualL,
                                                                              source = actualL;
            for (int position = 0; position < static_cast<int>(actualL.size());) {
                const int frames =
                    std::min(position % 2 ? 191 : 257, static_cast<int>(actualL.size()) - position);
                juce::AudioBuffer<float> a(channels, frames), b(channels, frames);
                a.clear();
                b.clear();
                if (position == 0) {
                    a.setSample(0, 0, .8f);
                    b.setSample(0, 0, .8f);
                    if (channels == 2) {
                        a.setSample(1, 0, .8f);
                        b.setSample(1, 0, .8f);
                    }
                }
                p.processBlock(a, midi);
                reference.processBlock(b, midi);
                for (int i = 0; i < frames; ++i) {
                    actualL[static_cast<std::size_t>(position + i)] = a.getSample(0, i);
                    source[static_cast<std::size_t>(position + i)] = b.getSample(0, i);
                    if (channels == 2)
                        actualR[static_cast<std::size_t>(position + i)] = a.getSample(1, i);
                }
                position += frames;
            }
            const auto nearby = [&](const auto &values, int at) {
                float peak = 0;
                for (int i = at - 1; i <= at + 1; ++i)
                    peak = std::max(peak, std::abs(values[static_cast<std::size_t>(i)]));
                return peak;
            };
            const float firstPeak = nearby(actualL, first), secondPeak = nearby(actualL, second);
            require(firstPeak > .06f && secondPeak > .02f,
                    "Actual doubler controls produce two delayed copies");
            require(source[latency] > .7f &&
                        std::abs(actualL[latency] - .5f * source[latency]) < 1e-7f,
                    "Doubler dry/wet applies to real host audio");
            if (channels == 2)
                require(nearby(actualR, second) > nearby(actualR, first) && firstPeak > secondPeak,
                        "Doubler uses opposite stereo voice weights");
            set(p, "mix", 0);
            set(reference, "mix", 0);
            p.prepareToPlay(rate, 257);
            reference.prepareToPlay(rate, 257);
            juce::AudioBuffer<float> dry(channels, 257), dryControl(channels, 257);
            dry.clear();
            dryControl.clear();
            dry.setSample(0, 0, .123f);
            dryControl.setSample(0, 0, .123f);
            p.processBlock(dry, midi);
            reference.processBlock(dryControl, midi);
            for (int c = 0; c < channels; ++c)
                require(std::equal(dry.getReadPointer(c), dry.getReadPointer(c) + 257,
                                   dryControl.getReadPointer(c)),
                        "Global dry mix bypasses simulated doubler output");
            set(p, "bypass", 1);
            dry.clear();
            dry.setSample(0, 0, .456f);
            p.processBlock(dry, midi);
            require(dry.getSample(0, latency) == .456f,
                    "Global bypass remains exact with doubler active");
        }
}

class TempoPlayHead final : public juce::AudioPlayHead {
  public:
    juce::Optional<double> bpm{120.};
    bool hasPosition = true;
    juce::Optional<PositionInfo> getPosition() const override {
        if (!hasPosition)
            return {};
        PositionInfo info;
        info.setBpm(bpm);
        return info;
    }
};
void stereoDelayAudioTests() {
    for (double rate : {44100., 48000., 88200., 96000.})
        for (int channels : {1, 2}) {
            for (int scenario = 0; scenario < 9; ++scenario) {
                DarkVoiceProcessor p;
                layout(p, channels);
                TempoPlayHead playhead;
                set(p, "stereoDelayMix", .5f);
                set(p, "stereoDelayFeedback", 0);
                set(p, "stereoDelaySync", 1);
                set(p, "stereoDelayBpm", 80);
                set(p, "stereoDelayDivision", 5);
                set(p, "stereoDelayHostTempo", scenario == 1 ? 0 : 1);
                if (scenario == 2)
                    playhead.bpm = {};
                if (scenario == 3)
                    playhead.bpm = 19.;
                if (scenario == 4)
                    playhead.bpm = 301.;
                if (scenario == 5)
                    playhead.bpm = std::numeric_limits<double>::quiet_NaN();
                if (scenario == 6)
                    playhead.hasPosition = false;
                if (scenario != 7)
                    p.setPlayHead(&playhead);
                p.prepareToPlay(rate, 257);
                juce::MidiBuffer midi;
                double peak = 0;
                int peakFrame = -1, position = 0;
                const int expected =
                    latency + static_cast<int>(rate * ((scenario == 0) ? .5 : .75));
                for (int block = 0; position < static_cast<int>(rate); ++block) {
                    const int frames =
                        std::min(block % 2 ? 257 : 191, static_cast<int>(rate) - position);
                    juce::AudioBuffer<float> audio(channels, frames);
                    audio.clear();
                    if (position == 0)
                        for (int c = 0; c < channels; ++c)
                            audio.setSample(c, 0, c == 0 ? .8f : .4f);
                    // Change an existing host's valid tempo before the first echo.
                    if (scenario == 8 && position > rate * .1)
                        playhead.bpm = 80.;
                    p.processBlock(audio, midi);
                    for (int i = 0; i < frames; ++i)
                        if (position + i > rate * .3 && std::abs(audio.getSample(0, i)) > peak) {
                            peak = std::abs(audio.getSample(0, i));
                            peakFrame = position + i;
                        }
                    position += frames;
                }
                require(
                    peak > .3 && std::abs(peakFrame - expected) <= 1,
                    "Host/manual/fallback/live tempo did not control actual musical echo timing");
                const bool valid = scenario == 0 || scenario == 1 || scenario == 8;
                require((p.hostTempoBpm() > 0) == valid,
                        "Host tempo availability status incorrect");
                if (scenario == 8)
                    require(p.hostTempoBpm() == 80, "Live host BPM change not observed");
                p.setPlayHead(nullptr);
            }
        }
    DarkVoiceProcessor p;
    require(p.getTailLengthSeconds() == 40, "Neutral delay changed existing tail");
    set(p, "stereoDelayMix", .1f);
    require(p.getTailLengthSeconds() == 240, "Audible musical delay needs extended tail");
    set(p, "stereoDelayMix", 0);
    require(p.getTailLengthSeconds() == 40, "Disabled delay tail did not restore");
    // Compare against the held valid snapshot, not nominal exact 0dB: JUCE's
    // start-relative .1dB snapping can leave a small residual under FMA.
    for (const char *id : {"stereoDelayMode", "stereoDelayDivision", "spaceModel", "colourModel"}) {
        DarkVoiceProcessor subject, control;
        for (auto *processor : {&subject, &control}) {
            set(*processor, "mix", 0);
            set(*processor, "stereoDelayMode", 2);
            set(*processor, "stereoDelayDivision", 5);
            set(*processor, "stereoDelayMix", .5f);
            processor->prepareToPlay(48000, 64);
        }
        set(subject, "outputDb", 12);
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> actual(2, 64), expected(2, 64);
        const auto input = [&] {
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 64; ++i) {
                    const float value = .05f * (c + 1) + .001f * (i % 7);
                    actual.setSample(c, i, value);
                    expected.setSample(c, i, value);
                }
        };
        for (float invalid :
             {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
              std::numeric_limits<float>::max(), .5f, -1.f}) {
            subject.parameters.getRawParameterValue(id)->store(invalid);
            for (int block = 0; block < 3; ++block) {
                input();
                subject.processBlock(actual, midi);
                control.processBlock(expected, midi);
                for (int c = 0; c < 2; ++c)
                    require(std::equal(actual.getReadPointer(c), actual.getReadPointer(c) + 64,
                                       expected.getReadPointer(c)),
                            "Invalid raw enum must hold entire valid snapshot");
            }
        }
        const float valid = juce::String(id) == "stereoDelayDivision" ? 5.f
                            : juce::String(id) == "stereoDelayMode"   ? 2.f
                                                                      : 0.f;
        subject.parameters.getRawParameterValue(id)->store(valid);
        float difference = 0;
        for (int block = 0; block < 32; ++block) {
            input();
            subject.processBlock(actual, midi);
            control.processBlock(expected, midi);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 64; ++i)
                    difference = std::max(
                        difference, std::abs(actual.getSample(c, i) - expected.getSample(c, i)));
        }
        require(difference > .01f, "Valid enum must release the pending observable gain change");
    }
}

void limiterAudioTests() {
    for (double rate : {44100., 48000., 88200., 96000.}) {
        for (int channels : {1, 2}) {
            DarkVoiceProcessor p;
            layout(p, channels);
            require(raw(p, "limiterEnabled") == 0, "Limiter default must remain off");
            set(p, "mix", 0);
            set(p, "outputDb", 12);
            set(p, "limiterEnabled", 1);
            set(p, "limiterCeilingDb", -6);
            set(p, "limiterReleaseMs", 1000);
            p.prepareToPlay(rate, 73);
            juce::MidiBuffer midi;
            juce::AudioBuffer<float> audio(channels, 257);
            const auto fill = [&](float value) {
                for (int c = 0; c < channels; ++c)
                    for (int i = 0; i < audio.getNumSamples(); ++i)
                        audio.setSample(c, i, c == 0 ? value : value * .5f);
            };
            fill(.8f);
            p.processBlock(audio, midi);
            const double ceiling = std::pow(10., raw(p, "limiterCeilingDb") / 20.);
            require(p.outputPeak() <= ceiling && p.outputPeak() > .49f,
                    "Host meters must reflect limiter after output gain and dry mix");
            for (int i = 0; i < audio.getNumSamples(); ++i) {
                require(std::abs(audio.getSample(0, i)) <= ceiling,
                        "Actual host output exceeds sample ceiling");
                if (channels == 2)
                    require(std::abs(audio.getSample(1, i) - audio.getSample(0, i) * .5f) < 1e-7f,
                            "Host limiter stereo image not linked");
            }
            set(p, "limiterCeilingDb", -18);
            fill(.8f);
            p.processBlock(audio, midi);
            require(audio.getSample(0, 0) <= std::pow(10., raw(p, "limiterCeilingDb") / 20.),
                    "Host ceiling automation must protect next sample");
            fill(.001f);
            p.processBlock(audio, midi); // Complete the explicit source-amplitude transition.
            fill(.001f);
            p.processBlock(audio, midi);
            require(audio.getSample(0, 256) > audio.getSample(0, 0),
                    "Host limiter release must recover continuously");
            set(p, "limiterEnabled", 0);
            fill(.8f);
            p.processBlock(audio, midi);
            require(audio.getSample(0, latency) > 3,
                    "Disabling host limiter must restore dry gain");
            // Move the small source through the raw alignment queue while disabled,
            // then test re-enable against genuinely small delayed input.
            fill(.001f);
            p.processBlock(audio, midi);
            set(p, "limiterEnabled", 1);
            fill(.001f);
            p.processBlock(audio, midi);
            require(std::abs(audio.getSample(0, 0) - .001f * std::pow(10., 12. / 20.)) < 1e-8,
                    "Host reenable must clear stale reduction");
            set(p, "bypass", 1);
            fill(2.5f);
            p.processBlock(audio, midi);
            require(audio.getSample(0, latency) == 2.5f,
                    "Host global bypass must bypass limiter exactly");
            set(p, "bypass", 0);
            set(p, "mix", 1);
            p.prepareToPlay(rate, 73);
            fill(.8f);
            p.processBlock(audio, midi);
            require(p.outputPeak() <= std::pow(10., raw(p, "limiterCeilingDb") / 20.),
                    "Wet host output gain also protected");
        }
    }
}

void gateAudioTests() {
    DarkVoiceProcessor p;
    layout(p, 2);
    set(p, "gateRatio", 4);
    set(p, "gateThresholdDb", -10);
    set(p, "gateReleaseMs", 10);
    p.prepareToPlay(48000, 512);
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    for (int i = 0; i < 100; ++i) {
        sine(buffer, 48000, i * 512);
        p.processBlock(buffer, midi);
    }
    require(buffer.getMagnitude(0, 512) < .004f, "Gate automation did not attenuate host audio");
    set(p, "bypass", 1);
    sine(buffer, 48000);
    const float before = buffer.getSample(0, 10);
    p.processBlock(buffer, midi);
    require(buffer.getSample(0, 10 + latency) == before, "Gate changed bypassed host audio");
    p.releaseResources();
}
void deEsserAudioTests() {
    DarkVoiceProcessor p;
    layout(p, 2);
    set(p, "deEssHz", 2000);
    set(p, "deEssThresholdDb", -60);
    set(p, "deEssReductionDb", 12);
    p.prepareToPlay(48000, 512);
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    for (int block = 0; block < 100; ++block) {
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 512; ++i)
                buffer.setSample(
                    c, i,
                    static_cast<float>(.1 * std::sin(2 * juce::MathConstants<double>::pi * 8000 *
                                                     (block * 512 + i) / 48000)));
        p.processBlock(buffer, midi);
    }
    require(buffer.getMagnitude(0, 512) < .065f,
            "De-esser controls did not attenuate host high band");
    set(p, "bypass", 1);
    sine(buffer, 48000);
    const float before = buffer.getSample(0, 10);
    p.processBlock(buffer, midi);
    require(buffer.getSample(0, 10 + latency) == before, "De-esser changed bypassed audio");
    p.releaseResources();
}
void flangerAudioTests() {
    DarkVoiceProcessor p;
    layout(p, 1);
    set(p, "flangerMix", 1);
    set(p, "flangerDepthMs", 0);
    p.prepareToPlay(48000, 512);
    juce::AudioBuffer<float> impulse(1, 512);
    impulse.clear();
    impulse.setSample(0, 0, 1);
    juce::MidiBuffer midi;
    p.processBlock(impulse, midi);
    require(impulse.getMagnitude(0, 0, 215 + latency) == 0 &&
                impulse.getMagnitude(0, 215 + latency, 3) > .9f,
            "Actual flanger controls move impulse to4.5ms");
    p.releaseResources();
}
void modulationAudioTests() {
    DarkVoiceProcessor p;
    layout(p, 2);
    set(p, "stereoWidth", 0);
    p.prepareToPlay(48000, 512);
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    sine(buffer, 48000);
    for (int i = 0; i < 512; ++i)
        buffer.setSample(1, i, -buffer.getSample(0, i));
    p.processBlock(buffer, midi);
    require(buffer.getMagnitude(0, 0, 512) == 0 && buffer.getMagnitude(1, 0, 512) == 0,
            "Width controls did not cancel antiphase host side signal");
    set(p, "bypass", 1);
    sine(buffer, 48000);
    const float before = buffer.getSample(0, 10);
    p.processBlock(buffer, midi);
    require(buffer.getSample(0, 10 + latency) == before, "Width changed bypassed host audio");
    p.releaseResources();
    DarkVoiceProcessor chorus;
    layout(chorus, 1);
    set(chorus, "chorusMix", .5f);
    set(chorus, "chorusDepthMs", 0);
    chorus.prepareToPlay(48000, 512);
    juce::AudioBuffer<float> impulse(1, 2048);
    impulse.clear();
    impulse.setSample(0, 0, 1);
    chorus.processBlock(impulse, midi);
    require(impulse.getSample(0, 720 + latency) > .4f,
            "Chorus host parameter did not create delayed audio");
    chorus.releaseResources();
}

void doubleTrackedRockAudioTests() {
    const auto root = juce::File::getSpecialLocation(juce::File::tempDirectory)
                          .getChildFile("darkvoice-double-tracked-rock-" + juce::Uuid().toString());
    darkvoice::PresetBank bank(root);
    const auto *preset = bank.find("factory-double-tracked-rock");
    require(preset && preset->name == "Double Tracked Rock",
            "Double Tracked Rock factory is available");
    require(preset->values.at("doublerMix") > 0 && preset->values.at("doublerVariationMs") > 0,
            "Factory must use both delayed mixing and variation");
    for (double rate : {44100., 48000., 88200., 96000.})
        for (int channels : {1, 2}) {
            DarkVoiceProcessor actual, reference;
            layout(actual, channels);
            layout(reference, channels);
            auto dry = preset->values;
            dry["doublerMix"] = 0;
            require(actual.applyPreset(preset->values) && reference.applyPreset(dry),
                    "Factory and paired reference must apply");
            actual.prepareToPlay(rate, 257);
            reference.prepareToPlay(rate, 257);
            juce::MidiBuffer midi;
            double energy = 0, difference = 0;
            int position = 0;
            for (int block = 0; block < 80; ++block) {
                const int frames = block % 2 ? 257 : 73;
                juce::AudioBuffer<float> a(channels, frames), b(channels, frames);
                for (int c = 0; c < channels; ++c)
                    for (int i = 0; i < frames; ++i) {
                        const double t = (position + i) / rate;
                        const auto input = static_cast<float>(
                            .2 * std::sin(2 * juce::MathConstants<double>::pi * 110 * t) +
                            .08 * std::sin(2 * juce::MathConstants<double>::pi * 220 * t) +
                            .03 * std::sin(2 * juce::MathConstants<double>::pi * 1900 * t));
                        a.setSample(c, i, input);
                        b.setSample(c, i, input);
                    }
                actual.processBlock(a, midi);
                reference.processBlock(b, midi);
                for (int c = 0; c < channels; ++c)
                    for (int i = 0; i < frames; ++i) {
                        const double wet = a.getSample(c, i), drySample = b.getSample(c, i);
                        require(std::isfinite(wet) && std::isfinite(drySample),
                                "Factory paired renders must remain finite");
                        energy += drySample * drySample;
                        difference += (wet - drySample) * (wet - drySample);
                    }
                position += frames;
            }
            require(energy > 1 && difference / energy > .005,
                    "Real simulated double tracking must measurably change paired factory audio");
        }
    root.deleteRecursively();
}

void factoryAudioTests() {
    darkvoice::PresetBank bank(
        juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("darkvoice-factory-render-" + juce::Uuid().toString()));
    require(bank.refresh().wasOk(), "Factory render library refresh");
    for (double rate : {44100., 48000., 88200., 96000.}) {
        for (int channels : {1, 2}) {
            for (const auto &preset : bank.presets()) {
                DarkVoiceProcessor processor;
                layout(processor, channels);
                require(processor.applyPreset(preset.values), "Factory application failed");
                for (const auto *id : {"sculptBodyDb", "sculptWarmthDb", "sculptPresenceDb"})
                    require(raw(processor, id) == 0,
                            "All factory preset loads retain exact neutral tone");
                processor.prepareToPlay(rate, 257);
                juce::MidiBuffer midi;
                double energy = 0;
                int position = 0;
                for (int block = 0; block < 80; ++block) {
                    juce::AudioBuffer<float> audio(channels, block % 2 ? 257 : 73);
                    for (int c = 0; c < channels; ++c)
                        for (int i = 0; i < audio.getNumSamples(); ++i) {
                            const double t = (position + i) / rate;
                            audio.setSample(
                                c, i,
                                static_cast<float>(
                                    .08 * std::sin(2 * juce::MathConstants<double>::pi * 130 * t) +
                                    .04 *
                                        std::sin(2 * juce::MathConstants<double>::pi * 1900 * t)));
                        }
                    position += audio.getNumSamples();
                    processor.processBlock(audio, midi);
                    for (int c = 0; c < channels; ++c)
                        for (int i = 0; i < audio.getNumSamples(); ++i) {
                            const auto value = audio.getSample(c, i);
                            require(std::isfinite(value), "Nonfinite factory render");
                            energy += value * value;
                        }
                }
                require(energy > .001, "Factory unexpectedly silenced vocal fixture");
            }
        }
    }
}
void renderingTests() {
    juce::MidiBuffer midi;
    for (double rate : {44100.0, 48000.0, 88200.0, 96000.0}) {
        for (int channels : {1, 2}) {
            DarkVoiceProcessor p;
            layout(p, channels);
            set(p, "mix", 0.0f);
            set(p, "outputDb", -6.0f);
            p.prepareToPlay(rate, 64);
            require(p.getLatencySamples() == latency,
                    "Prepared host latency must stay fixed at56 samples");
            // Oversized host block is processed in bounded prepared-size chunks.
            juce::AudioBuffer<float> buffer(channels, 257);
            sine(buffer, rate);
            const float original = buffer.getSample(0, 20);
            p.processBlock(buffer, midi);
            const auto expected = original * juce::Decibels::decibelsToGain(-6.0f);
            require(std::abs(buffer.getSample(0, 20 + latency) - expected) < 1.0e-5f,
                    "Output gain not applied to actual audio");
            for (int c = 0; c < channels; ++c)
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                    require(std::isfinite(buffer.getSample(c, i)), "Nonfinite render");
            require(p.inputPeak() > 0.09f && p.outputPeak() > 0.045f && p.outputPeak() < 0.055f,
                    "Block meters incorrect");
            set(p, "bypass", 1.0f);
            sine(buffer, rate);
            const float bypassSample = buffer.getSample(0, 20);
            p.processBlock(buffer, midi);
            require(buffer.getSample(0, 20 + latency) == bypassSample, "Bypass changed audio");
            juce::AudioBuffer<float> zero(channels, 0);
            p.processBlock(zero, midi);
            p.releaseResources();
            require(p.getLatencySamples() == 0, "Release must clear host latency");
            sine(buffer, rate);
            p.processBlock(buffer, midi);
            require(buffer.getMagnitude(0, buffer.getNumSamples()) == 0,
                    "Unprepared processing should silence");
        }
    }
    DarkVoiceProcessor master(true);
    require(master.getName() == "DarkVoice Master", "Master identity incorrect");
    DarkVoiceProcessor vocal;
    require(vocal.getName() == "DarkVoice Vocal", "Vocal identity incorrect");
    auto badLayout = vocal.getBusesLayout();
    badLayout.outputBuses.set(0, juce::AudioChannelSet::mono());
    require(!vocal.isBusesLayoutSupported(badLayout), "Mismatched layout accepted");
    layout(vocal, 1);
    vocal.prepareToPlay(32000, 64);
    juce::AudioBuffer<float> rejected(1, 64);
    sine(rejected, 32000);
    vocal.processBlock(rejected, midi);
    require(rejected.getMagnitude(0, 64) == 0 && vocal.getLatencySamples() == 0,
            "Unsupported prepare must silence and clear latency");
}
} // namespace
int main() {
    juce::ScopedJuceInitialiser_GUI runtime;
    try {
        stateTests();
        colourAudioTests();
        sculptorToneAudioTests();
        forcedBypassTests();
        renderingTests();
        factoryAudioTests();
        doubleTrackedRockAudioTests();
        gateAudioTests();
        deEsserAudioTests();
        modulationAudioTests();
        flangerAudioTests();
        limiterAudioTests();
        stereoDelayAudioTests();
        doublerAudioTests();
        spaceReverbAudioTests();
        spaceTailTests();
        std::cout
            << "PASS plugin state, automation/render, rates/layouts, lifecycle and identity\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
