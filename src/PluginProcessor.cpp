#include "PluginProcessor.hpp"
#include "PluginEditor.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>

namespace {
constexpr std::array<const char*, 8> ids {"inputDb", "outputDb", "highPassHz", "drive", "thresholdDb", "ratio", "mix", "bypass"};
constexpr std::array<float, 8> minima {-24, -24, 20, 0, -60, 1, 0, 0};
constexpr std::array<float, 8> maxima {24, 24, 400, 1, 0, 20, 1, 1};
}
static_assert(std::atomic<float>::is_always_lock_free, "Real-time parameter and meter transport requires lock-free float atomics");

juce::AudioProcessorValueTreeState::ParameterLayout DarkVoiceProcessor::createParameters() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"inputDb", 1}, "Input", juce::NormalisableRange<float>{-24, 24, 0.1f}, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"outputDb", 1}, "Output", juce::NormalisableRange<float>{-24, 24, 0.1f}, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"highPassHz", 1}, "High-pass", juce::NormalisableRange<float>{20, 400, 1, 0.5f}, 20));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"drive", 1}, "Warmth drive", juce::NormalisableRange<float>{0, 1, 0.001f}, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"thresholdDb", 1}, "Compressor threshold", juce::NormalisableRange<float>{-60, 0, 0.1f}, -18));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"ratio", 1}, "Compressor ratio", juce::NormalisableRange<float>{1, 20, 0.1f}, 1));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"mix", 1}, "Dry / wet", juce::NormalisableRange<float>{0, 1, 0.001f}, 1));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"bypass", 1}, "Bypass", false));
    return layout;
}
DarkVoiceProcessor::DarkVoiceProcessor(bool mastering)
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "DarkVoiceState", createParameters()), mastering_(mastering) {
    for (std::size_t i = 0; i < ids.size(); ++i) raw_[i] = parameters.getRawParameterValue(ids[i]);
    setLatencySamples(0);
}
const juce::String DarkVoiceProcessor::getName() const { return mastering_ ? "DarkVoice Master" : "DarkVoice Vocal"; }
bool DarkVoiceProcessor::isBusesLayoutSupported(const BusesLayout& layout) const {
    const auto input = layout.getMainInputChannelSet();
    return input == layout.getMainOutputChannelSet()
        && (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo());
}
darkvoice::EngineParameters DarkVoiceProcessor::snapshot() const noexcept {
    darkvoice::EngineParameters p;
    p.inputDb = raw_[0]->load(std::memory_order_relaxed);
    p.outputDb = raw_[1]->load(std::memory_order_relaxed);
    p.highPassHz = raw_[2]->load(std::memory_order_relaxed);
    p.drive = raw_[3]->load(std::memory_order_relaxed);
    p.thresholdDb = raw_[4]->load(std::memory_order_relaxed);
    p.ratio = raw_[5]->load(std::memory_order_relaxed);
    p.mix = raw_[6]->load(std::memory_order_relaxed);
    p.bypass = raw_[7]->load(std::memory_order_relaxed) >= 0.5f;
    return p;
}
void DarkVoiceProcessor::prepareToPlay(double rate, int maxFrames) {
    maximumFrames_ = maxFrames > 0 ? static_cast<std::size_t>(maxFrames) : 0;
    prepared_ = engine_.prepare(rate, maximumFrames_, static_cast<std::size_t>(getTotalNumInputChannels()));
    if (prepared_) { (void)engine_.setParameters(snapshot()); engine_.reset(); }
    inputPeak_.store(0, std::memory_order_relaxed);
    outputPeak_.store(0, std::memory_order_relaxed);
    setLatencySamples(static_cast<int>(engine_.latencySamples()));
}
void DarkVoiceProcessor::releaseResources() {
    prepared_ = false;
    engine_.reset();
    inputPeak_.store(0, std::memory_order_relaxed);
    outputPeak_.store(0, std::memory_order_relaxed);
}
void DarkVoiceProcessor::processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    if (!prepared_ || audio.getNumChannels() != getTotalNumInputChannels()
        || audio.getNumChannels() < 1 || audio.getNumChannels() > 2) {
        audio.clear();
        inputPeak_.store(0, std::memory_order_relaxed);
        outputPeak_.store(0, std::memory_order_relaxed);
        return;
    }
    (void)engine_.setParameters(snapshot());
    float inPeak = 0, outPeak = 0;
    const auto frames = static_cast<std::size_t>(audio.getNumSamples());
    std::size_t offset = 0;
    while (offset < frames) {
        const auto count = std::min(maximumFrames_, frames - offset);
        std::array<float*, 2> channels {audio.getWritePointer(0) + offset, nullptr};
        if (audio.getNumChannels() == 2) channels[1] = audio.getWritePointer(1) + offset;
        if (!engine_.process(std::span<float*>(channels.data(), static_cast<std::size_t>(audio.getNumChannels())), count)) {
            audio.clear();
            inPeak = outPeak = 0;
            break;
        }
        const auto meters = engine_.meters();
        inPeak = std::max(inPeak, meters.inputPeak);
        outPeak = std::max(outPeak, meters.outputPeak);
        offset += count;
    }
    inputPeak_.store(inPeak, std::memory_order_relaxed);
    outputPeak_.store(outPeak, std::memory_order_relaxed);
}
juce::AudioProcessorEditor* DarkVoiceProcessor::createEditor() { return new DarkVoiceEditor(*this); }
void DarkVoiceProcessor::getStateInformation(juce::MemoryBlock& destination) {
    auto state = parameters.copyState();
    state.setProperty("schemaVersion", 1, nullptr);
    if (const auto xml = state.createXml()) copyXmlToBinary(*xml, destination);
}
void DarkVoiceProcessor::setStateInformation(const void* data, int bytes) {
    if (data == nullptr || bytes <= 0 || bytes > 1024 * 1024) return;
    const auto xml = getXmlFromBinary(data, bytes);
    if (!xml || !xml->hasTagName("DarkVoiceState") || xml->getStringAttribute("schemaVersion") != "1") return;
    const auto state = juce::ValueTree::fromXml(*xml);
    if (!state.isValid() || state.getNumChildren() != static_cast<int>(ids.size())) return;
    std::array<bool, 8> seen {};
    for (int childIndex = 0; childIndex < state.getNumChildren(); ++childIndex) {
        const auto child = state.getChild(childIndex);
        if (!child.hasType("PARAM") || !child.hasProperty("id") || !child.hasProperty("value")) return;
        const auto id = child.getProperty("id").toString();
        std::size_t index = 0;
        while (index < ids.size() && id != ids[index]) ++index;
        if (index == ids.size() || seen[index]) return;
        const auto token = child.getProperty("value").toString().trim();
        const auto bytes = token.toRawUTF8();
        const auto length = static_cast<std::size_t>(token.getNumBytesAsUTF8());
        float value = 0;
        const auto parsed = std::from_chars(bytes, bytes + length, value);
        if (parsed.ec != std::errc{} || parsed.ptr != bytes + length
            || !std::isfinite(value) || value < minima[index] || value > maxima[index]) return;
        if (index == 7 && value != 0 && value != 1) return;
        seen[index] = true;
    }
    parameters.replaceState(state);
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
#if defined(DARKVOICE_MASTER) && DARKVOICE_MASTER
    return new DarkVoiceProcessor(true);
#else
    return new DarkVoiceProcessor(false);
#endif
}
