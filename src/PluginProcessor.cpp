#include "PluginProcessor.hpp"
#include "PluginEditor.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

static_assert(std::atomic<float>::is_always_lock_free,
              "Real-time parameter and meter transport requires lock-free float atomics");

juce::AudioProcessorValueTreeState::ParameterLayout DarkVoiceProcessor::createParameters() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (std::size_t i = 0; i < darkvoice::parameterSchema.size(); ++i) {
        const auto &spec = darkvoice::parameterSchema[i];
        const int version = i < darkvoice::legacyParameterCount ? 1 : 2;
        if (spec.boolean)
            layout.add(std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID{spec.id, version}, spec.name, spec.defaultValue != 0));
        else
            layout.add(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID{spec.id, version}, spec.name,
                juce::NormalisableRange<float>{spec.minimum, spec.maximum, spec.step, spec.skew},
                spec.defaultValue));
    }
    return layout;
}
DarkVoiceProcessor::DarkVoiceProcessor(bool mastering)
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "DarkVoiceState", createParameters()), mastering_(mastering) {
    for (std::size_t i = 0; i < darkvoice::parameterSchema.size(); ++i)
        raw_[i] = parameters.getRawParameterValue(darkvoice::parameterSchema[i].id);
    setLatencySamples(0);
}
const juce::String DarkVoiceProcessor::getName() const {
    if (mastering_)
        return "DarkVoice Master";
#ifdef JucePlugin_Name
    return JucePlugin_Name;
#else
    return "DarkVoice Vocal";
#endif
}
bool DarkVoiceProcessor::isBusesLayoutSupported(const BusesLayout &layout) const {
    const auto input = layout.getMainInputChannelSet();
    return input == layout.getMainOutputChannelSet() &&
           (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo());
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
    p.eqLowHz = raw_[8]->load(std::memory_order_relaxed);
    p.eqLowGainDb = raw_[9]->load(std::memory_order_relaxed);
    p.eqMidHz = raw_[10]->load(std::memory_order_relaxed);
    p.eqMidGainDb = raw_[11]->load(std::memory_order_relaxed);
    p.eqMidQ = raw_[12]->load(std::memory_order_relaxed);
    p.eqHighHz = raw_[13]->load(std::memory_order_relaxed);
    p.eqHighGainDb = raw_[14]->load(std::memory_order_relaxed);
    p.delayMs = raw_[15]->load(std::memory_order_relaxed);
    p.delayFeedback = raw_[16]->load(std::memory_order_relaxed);
    p.delayMix = raw_[17]->load(std::memory_order_relaxed);
    p.reverbSize = raw_[18]->load(std::memory_order_relaxed);
    p.reverbDamping = raw_[19]->load(std::memory_order_relaxed);
    p.reverbMix = raw_[20]->load(std::memory_order_relaxed);
    return p;
}
void DarkVoiceProcessor::prepareToPlay(double rate, int maxFrames) {
    maximumFrames_ = maxFrames > 0 ? static_cast<std::size_t>(maxFrames) : 0;
    prepared_ =
        engine_.prepare(rate, maximumFrames_, static_cast<std::size_t>(getTotalNumInputChannels()));
    if (prepared_) {
        (void)engine_.setParameters(snapshot());
        engine_.reset();
    }
    if (prepared_)
        (void)dryAnalysis_.prepare(rate);
    else
        dryAnalysis_.release();
    inputPeak_.store(0, std::memory_order_relaxed);
    outputPeak_.store(0, std::memory_order_relaxed);
    setLatencySamples(static_cast<int>(engine_.latencySamples()));
}
void DarkVoiceProcessor::releaseResources() {
    prepared_ = false;
    dryAnalysis_.release();
    engine_.reset();
    inputPeak_.store(0, std::memory_order_relaxed);
    outputPeak_.store(0, std::memory_order_relaxed);
}
void DarkVoiceProcessor::processBlock(juce::AudioBuffer<float> &audio, juce::MidiBuffer &) {
    juce::ScopedNoDenormals noDenormals;
    if (!prepared_ || audio.getNumChannels() != getTotalNumInputChannels() ||
        audio.getNumChannels() < 1 || audio.getNumChannels() > 2) {
        audio.clear();
        inputPeak_.store(0, std::memory_order_relaxed);
        outputPeak_.store(0, std::memory_order_relaxed);
        return;
    }
    dryAnalysis_.push(audio);
    (void)engine_.setParameters(snapshot());
    float inPeak = 0, outPeak = 0;
    const auto frames = static_cast<std::size_t>(audio.getNumSamples());
    std::size_t offset = 0;
    while (offset < frames) {
        const auto count = std::min(maximumFrames_, frames - offset);
        std::array<float *, 2> channels{audio.getWritePointer(0) + offset, nullptr};
        if (audio.getNumChannels() == 2)
            channels[1] = audio.getWritePointer(1) + offset;
        if (!engine_.process(std::span<float *>(channels.data(),
                                                static_cast<std::size_t>(audio.getNumChannels())),
                             count)) {
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
juce::AudioProcessorEditor *DarkVoiceProcessor::createEditor() {
    return new DarkVoiceEditor(*this);
}
void DarkVoiceProcessor::getStateInformation(juce::MemoryBlock &destination) {
    auto state = parameters.copyState();
    state.setProperty("schemaVersion", 2, nullptr);
    // JUCE numeric var-to-String conversion follows the global C++ locale.
    // Serialize a canonical decimal representation without changing process locale.
    for (auto child : state) {
        std::ostringstream canonical;
        canonical.imbue(std::locale::classic());
        canonical << std::setprecision(std::numeric_limits<float>::max_digits10)
                  << static_cast<float>(child.getProperty("value"));
        child.setProperty("value", juce::String(canonical.str()), nullptr);
    }
    if (const auto xml = state.createXml())
        copyXmlToBinary(*xml, destination);
}
void DarkVoiceProcessor::setStateInformation(const void *data, int bytes) {
    if (data == nullptr || bytes <= 0 || bytes > 1024 * 1024)
        return;
    const auto xml = getXmlFromBinary(data, bytes);
    if (!xml || !xml->hasTagName("DarkVoiceState"))
        return;
    const auto version = xml->getStringAttribute("schemaVersion");
    if (version != "1" && version != "2")
        return;
    const auto count =
        version == "1" ? darkvoice::legacyParameterCount : darkvoice::parameterSchema.size();
    const auto state = juce::ValueTree::fromXml(*xml);
    if (!state.isValid() || state.getNumChildren() != static_cast<int>(count))
        return;
    std::array<bool, darkvoice::parameterSchema.size()> seen{};
    std::array<float, darkvoice::parameterSchema.size()> values{};
    for (std::size_t i = 0; i < values.size(); ++i)
        values[i] = darkvoice::parameterSchema[i].defaultValue;
    for (int childIndex = 0; childIndex < state.getNumChildren(); ++childIndex) {
        const auto child = state.getChild(childIndex);
        if (!child.hasType("PARAM") || !child.hasProperty("id") || !child.hasProperty("value"))
            return;
        const auto id = child.getProperty("id").toString();
        std::size_t index = 0;
        while (index < count && id != darkvoice::parameterSchema[index].id)
            ++index;
        if (index == count || seen[index])
            return;
        const auto &spec = darkvoice::parameterSchema[index];
        std::istringstream parser(child.getProperty("value").toString().trim().toStdString());
        parser.imbue(std::locale::classic());
        float value = 0;
        parser >> std::noskipws >> value;
        if (!parser || parser.peek() != std::char_traits<char>::eof() || !std::isfinite(value) ||
            value < spec.minimum || value > spec.maximum ||
            (spec.boolean && value != 0 && value != 1))
            return;
        seen[index] = true;
        values[index] = value;
    }
    // Build canonical complete state only after the entire snapshot validates.
    juce::ValueTree migrated("DarkVoiceState");
    migrated.setProperty("schemaVersion", 2, nullptr);
    for (std::size_t i = 0; i < values.size(); ++i) {
        juce::ValueTree child("PARAM");
        child.setProperty("id", darkvoice::parameterSchema[i].id, nullptr);
        child.setProperty("value", values[i], nullptr);
        migrated.addChild(child, -1, nullptr);
    }
    parameters.replaceState(migrated);
}
darkvoice::PresetValues DarkVoiceProcessor::snapshotPresetValues() const {
    darkvoice::PresetValues values;
    for (std::size_t i = 0; i < darkvoice::parameterSchema.size(); ++i)
        if (!darkvoice::parameterSchema[i].boolean)
            values.emplace(darkvoice::parameterSchema[i].id,
                           raw_[i]->load(std::memory_order_relaxed));
    return values;
}
bool DarkVoiceProcessor::applyPreset(const darkvoice::PresetValues &values) {
    if (darkvoice::PresetBank::validate(values).failed())
        return false;
    for (const auto &spec : darkvoice::parameterSchema) {
        if (spec.boolean)
            continue;
        auto *parameter = parameters.getParameter(spec.id);
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(values.at(spec.id)));
        parameter->endChangeGesture();
    }
    return true;
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
#if defined(DARKVOICE_MASTER) && DARKVOICE_MASTER
    return new DarkVoiceProcessor(true);
#else
    return new DarkVoiceProcessor(false);
#endif
}
