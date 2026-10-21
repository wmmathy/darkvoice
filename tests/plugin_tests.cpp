#include "PluginProcessor.hpp"
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
void set(DarkVoiceProcessor& p, const char* id, float actual) {
    auto* parameter = p.parameters.getParameter(id);
    require(parameter != nullptr, "Missing stable parameter ID");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(actual));
}
float raw(DarkVoiceProcessor& p, const char* id) { return p.parameters.getRawParameterValue(id)->load(); }
void layout(DarkVoiceProcessor& p, int channels) {
    auto buses = p.getBusesLayout();
    buses.inputBuses.set(0, channels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
    buses.outputBuses.set(0, buses.inputBuses[0]);
    require(p.setBusesLayout(buses), "Supported layout rejected");
}
void sine(juce::AudioBuffer<float>& buffer, double rate, int start = 0) {
    for (int c = 0; c < buffer.getNumChannels(); ++c)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            buffer.setSample(c, i, 0.1f * std::sin(static_cast<float>(2.0 * juce::MathConstants<double>::pi * 1000.0 * (start + i) / rate)));
}
void stateTests() {
    DarkVoiceProcessor source;
    set(source, "inputDb", 7.5f);
    set(source, "drive", 0.6f);
    set(source, "ratio", 4.0f);
    set(source, "bypass", 1.0f);
    juce::MemoryBlock state;
    source.getStateInformation(state);
    require(state.getSize() > 0, "State serialization empty");
    DarkVoiceProcessor restored;
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    require(std::abs(raw(restored, "inputDb") - 7.5f) < 0.01f, "Input state not restored");
    require(std::abs(raw(restored, "drive") - 0.6f) < 0.001f, "Drive state not restored");
    require(std::abs(raw(restored, "ratio") - 4.0f) < 0.01f, "Ratio state not restored");
    require(raw(restored, "bypass") == 1.0f, "Bypass state not restored");
    const char garbage[] = "invalid state";
    restored.setStateInformation(garbage, sizeof(garbage));
    require(raw(restored, "inputDb") == raw(source, "inputDb"), "Malformed state changed parameters");
    for (const char* version : {"2", "1junk"}) {
        auto xml = source.parameters.copyState().createXml();
        xml->setAttribute("schemaVersion", version);
        juce::MemoryBlock invalid;
        juce::AudioProcessor::copyXmlToBinary(*xml, invalid);
        set(restored, "inputDb", -5.0f);
        restored.setStateInformation(invalid.getData(), static_cast<int>(invalid.getSize()));
        require(std::abs(raw(restored, "inputDb") + 5.0f) < 0.01f, "Invalid schema changed parameters");
    }
    for (const char* value : {"NaN", "1.2.3", "100", "", ".", "--1", "1 2"}) {
        auto tree = source.parameters.copyState();
        tree.setProperty("schemaVersion", 1, nullptr);
        tree.getChildWithProperty("id", "inputDb").setProperty("value", value, nullptr);
        juce::MemoryBlock invalid;
        juce::AudioProcessor::copyXmlToBinary(*tree.createXml(), invalid);
        restored.setStateInformation(invalid.getData(), static_cast<int>(invalid.getSize()));
        require(std::abs(raw(restored, "inputDb") + 5.0f) < 0.01f, "Invalid parameter state accepted");
    }
    auto scientificTree = source.parameters.copyState();
    scientificTree.setProperty("schemaVersion", 1, nullptr);
    scientificTree.getChildWithProperty("id", "inputDb").setProperty("value", "1e1", nullptr);
    juce::MemoryBlock scientific;
    juce::AudioProcessor::copyXmlToBinary(*scientificTree.createXml(), scientific);
    restored.setStateInformation(scientific.getData(), static_cast<int>(scientific.getSize()));
    require(std::abs(raw(restored, "inputDb") - 10.0f) < 0.01f, "Valid scientific value rejected");

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
            require(std::abs(buffer.getSample(0, 20) - expected) < 1.0e-5f, "Output gain not applied to actual audio");
            for (int c = 0; c < channels; ++c)
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                    require(std::isfinite(buffer.getSample(c, i)), "Nonfinite render");
            require(p.inputPeak() > 0.09f && p.outputPeak() > 0.045f && p.outputPeak() < 0.055f, "Block meters incorrect");
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
            require(buffer.getMagnitude(0, buffer.getNumSamples()) == 0, "Unprepared processing should silence");
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
}
int main() {
    try {
        stateTests();
        renderingTests();
        std::cout << "PASS plugin state, automation/render, rates/layouts, lifecycle and identity\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
