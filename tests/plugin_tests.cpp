#include "PluginProcessor.hpp"
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
                                                                                       : 5),
                "AU version hint changed");
        require(std::abs(raw(restored, spec.id) - raw(source, spec.id)) < 0.001f,
                "Full v5 state roundtrip failed");
    }
    require(source.getTailLengthSeconds() == 40.0, "Spatial tail not reported");

    const char garbage[] = "invalid state";
    restored.setStateInformation(garbage, sizeof(garbage));
    require(raw(restored, "inputDb") == raw(source, "inputDb"),
            "Malformed state changed parameters");
    for (const char *version : {"6", "1junk"}) {
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
    require(values.size() == 30 && values.count("bypass") == 0,
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
        gateAudioTests();
        deEsserAudioTests();
        modulationAudioTests();
        std::cout
            << "PASS plugin state, automation/render, rates/layouts, lifecycle and identity\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
