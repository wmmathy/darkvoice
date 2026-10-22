#include "PluginProcessor.hpp"
#include "PresetBank.hpp"
#include <array>
#include <cmath>
#include <iostream>
#include <locale>
#include <stdexcept>

namespace {
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
                                                                                       : 7),
                "AU version hint changed");
        require(std::abs(raw(restored, spec.id) - raw(source, spec.id)) < 0.001f,
                "Full v7 state roundtrip failed");
    }
    require(source.getTailLengthSeconds() == 40.0, "Spatial tail not reported");

    const char garbage[] = "invalid state";
    restored.setStateInformation(garbage, sizeof(garbage));
    require(raw(restored, "inputDb") == raw(source, "inputDb"),
            "Malformed state changed parameters");
    for (const char *version : {"8", "1junk"}) {
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
    for (const auto *invalidEnable : {"0.5", "-1", "2", "NaN", "1.00000001", "0.99999999"}) {
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
    require(values.size() == 37 && values.count("bypass") == 0,
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
            p.processBlock(audio, midi);
            require(audio.getSample(0, 256) > audio.getSample(0, 0),
                    "Host limiter release must recover continuously");
            set(p, "limiterEnabled", 0);
            fill(.8f);
            p.processBlock(audio, midi);
            require(audio.getSample(0, 0) > 3, "Disabling host limiter must restore dry gain");
            set(p, "limiterEnabled", 1);
            fill(.001f);
            p.processBlock(audio, midi);
            require(std::abs(audio.getSample(0, 0) - .001f * std::pow(10., 12. / 20.)) < 1e-8,
                    "Host reenable must clear stale reduction");
            set(p, "bypass", 1);
            fill(2.5f);
            p.processBlock(audio, midi);
            require(audio.getSample(0, 0) == 2.5f,
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
    require(buffer.getSample(0, 10) == before, "Gate changed bypassed host audio");
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
    require(buffer.getSample(0, 10) == before, "De-esser changed bypassed audio");
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
    require(impulse.getMagnitude(0, 0, 215) == 0 && impulse.getMagnitude(0, 215, 3) > .9f,
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
    require(buffer.getSample(0, 10) == before, "Width changed bypassed host audio");
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
    require(impulse.getSample(0, 720) > .4f, "Chorus host parameter did not create delayed audio");
    chorus.releaseResources();
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
            require(p.getLatencySamples() == 0, "Starter chain latency should be zero");
            // Oversized host block is processed in bounded prepared-size chunks.
            juce::AudioBuffer<float> buffer(channels, 257);
            sine(buffer, rate);
            const float original = buffer.getSample(0, 20);
            p.processBlock(buffer, midi);
            const auto expected = original * juce::Decibels::decibelsToGain(-6.0f);
            require(std::abs(buffer.getSample(0, 20) - expected) < 1.0e-5f,
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
            require(buffer.getSample(0, 20) == bypassSample, "Bypass changed audio");
            juce::AudioBuffer<float> zero(channels, 0);
            p.processBlock(zero, midi);
            p.releaseResources();
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
    require(rejected.getMagnitude(0, 64) == 0, "Unsupported sample rate not silenced");
}
} // namespace
int main() {
    juce::ScopedJuceInitialiser_GUI runtime;
    try {
        stateTests();
        renderingTests();
        factoryAudioTests();
        gateAudioTests();
        deEsserAudioTests();
        modulationAudioTests();
        flangerAudioTests();
        limiterAudioTests();
        std::cout
            << "PASS plugin state, automation/render, rates/layouts, lifecycle and identity\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
