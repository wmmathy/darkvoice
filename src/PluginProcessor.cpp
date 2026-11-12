#include "PluginProcessor.hpp"
#include "BooleanNumber.hpp"
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
        const int version = darkvoice::parameterVersion(i);
        if (spec.boolean)
            layout.add(std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID{spec.id, version}, spec.name, spec.defaultValue != 0));
        else {
            auto range =
                juce::NormalisableRange<float>{spec.minimum, spec.maximum, spec.step, spec.skew};
            if (juce::String(spec.id) == "flangerFeedback" ||
                juce::String(spec.id) == "sculptBodyDb" ||
                juce::String(spec.id) == "sculptWarmthDb" ||
                juce::String(spec.id) == "sculptPresenceDb" ||
                juce::String(spec.id) == "sculptDepthDb" ||
                juce::String(spec.id) == "sculptResonanceDb") {
                // Anchor these signed controls at zero. Start-relative snapping and affine
                // conversion can leave a nonzero residual under fused multiply-add.
                range = juce::NormalisableRange<float>{
                    spec.minimum, spec.maximum,
                    [](float, float limit, float normalised) {
                        return static_cast<float>((2.0 * normalised - 1.0) * limit);
                    },
                    [](float, float limit, float value) {
                        return static_cast<float>(.5 + .5 * static_cast<double>(value) / limit);
                    },
                    [step = spec.step](float low, float high, float value) {
                        const auto snapped = static_cast<float>(
                            std::round(static_cast<double>(value) / step) * step);
                        return std::clamp(snapped, low, high);
                    }};
                range.interval = spec.step;
            }
            layout.add(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID{spec.id, version}, spec.name, range, spec.defaultValue));
        }
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
    p.gateThresholdDb = raw_[21]->load(std::memory_order_relaxed);
    p.gateRatio = raw_[22]->load(std::memory_order_relaxed);
    p.gateReleaseMs = raw_[23]->load(std::memory_order_relaxed);
    p.deEssHz = raw_[24]->load(std::memory_order_relaxed);
    p.deEssThresholdDb = raw_[25]->load(std::memory_order_relaxed);
    p.deEssReductionDb = raw_[26]->load(std::memory_order_relaxed);
    p.chorusRateHz = raw_[27]->load(std::memory_order_relaxed);
    p.chorusDepthMs = raw_[28]->load(std::memory_order_relaxed);
    p.chorusMix = raw_[29]->load(std::memory_order_relaxed);
    p.stereoWidth = raw_[30]->load(std::memory_order_relaxed);
    p.flangerRateHz = raw_[31]->load(std::memory_order_relaxed);
    p.flangerDepthMs = raw_[32]->load(std::memory_order_relaxed);
    p.flangerFeedback = raw_[33]->load(std::memory_order_relaxed);
    p.flangerMix = raw_[34]->load(std::memory_order_relaxed);
    p.limiterEnabled = raw_[35]->load(std::memory_order_relaxed) >= 0.5f;
    p.limiterCeilingDb = raw_[36]->load(std::memory_order_relaxed);
    p.limiterReleaseMs = raw_[37]->load(std::memory_order_relaxed);
    const auto discrete = [](float value, int maximum) noexcept {
        return std::isfinite(value) && value >= 0 && value <= maximum && std::trunc(value) == value
                   ? static_cast<int>(value)
                   : -1;
    };
    p.stereoDelayMode = discrete(raw_[38]->load(std::memory_order_relaxed), 2);
    p.stereoDelayMs = raw_[39]->load(std::memory_order_relaxed);
    p.stereoDelayFeedback = raw_[40]->load(std::memory_order_relaxed);
    p.stereoDelayMix = raw_[41]->load(std::memory_order_relaxed);
    p.stereoDelaySync = raw_[42]->load(std::memory_order_relaxed) >= .5f;
    p.stereoDelayBpm = raw_[43]->load(std::memory_order_relaxed);
    p.stereoDelayDivision = discrete(raw_[44]->load(std::memory_order_relaxed), 5);
    const auto actual = hostTempoBpm();
    if (raw_[45]->load(std::memory_order_relaxed) >= .5f && actual >= 20 && actual <= 300)
        p.stereoDelayBpm = actual;
    p.doublerSeparationMs = raw_[46]->load(std::memory_order_relaxed);
    p.doublerVariationMs = raw_[47]->load(std::memory_order_relaxed);
    p.doublerRateHz = raw_[48]->load(std::memory_order_relaxed);
    p.doublerMix = raw_[49]->load(std::memory_order_relaxed);
    p.spaceModel = discrete(raw_[50]->load(std::memory_order_relaxed), 2);
    p.spaceDecay = raw_[51]->load(std::memory_order_relaxed);
    p.spaceToneHz = raw_[52]->load(std::memory_order_relaxed);
    p.spacePreDelayMs = raw_[53]->load(std::memory_order_relaxed);
    p.spaceMix = raw_[54]->load(std::memory_order_relaxed);
    p.colourModel = discrete(raw_[55]->load(std::memory_order_relaxed), 1);
    p.colourDrive = raw_[56]->load(std::memory_order_relaxed);
    p.colourToneHz = raw_[57]->load(std::memory_order_relaxed);
    p.colourMix = raw_[58]->load(std::memory_order_relaxed);
    p.sculptBodyDb = raw_[59]->load(std::memory_order_relaxed);
    p.sculptWarmthDb = raw_[60]->load(std::memory_order_relaxed);
    p.sculptPresenceDb = raw_[61]->load(std::memory_order_relaxed);
    p.sculptBreathiness = raw_[62]->load(std::memory_order_relaxed);
    p.sculptDepthDb = raw_[63]->load(std::memory_order_relaxed);
    p.sculptResonanceDb = raw_[64]->load(std::memory_order_relaxed);
    return p;
}
void DarkVoiceProcessor::prepareToPlay(double rate, int maxFrames) {
    hostTempoBpm_.store(0, std::memory_order_relaxed);
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
    hostTempoBpm_.store(0, std::memory_order_relaxed);
    prepared_ = false;
    dryAnalysis_.release();
    engine_.release();
    setLatencySamples(0);
    inputPeak_.store(0, std::memory_order_relaxed);
    outputPeak_.store(0, std::memory_order_relaxed);
}
void DarkVoiceProcessor::updateHostTempo() noexcept {
    float tempo = 0;
    if (auto *playHead = getPlayHead())
        if (const auto position = playHead->getPosition())
            if (const auto bpm = position->getBpm();
                bpm && std::isfinite(*bpm) && *bpm >= 20 && *bpm <= 300)
                tempo = static_cast<float>(*bpm);
    hostTempoBpm_.store(tempo, std::memory_order_relaxed);
}
void DarkVoiceProcessor::processBlock(juce::AudioBuffer<float> &audio, juce::MidiBuffer &) {
    processAudio(audio, false);
}
void DarkVoiceProcessor::processBlockBypassed(juce::AudioBuffer<float> &audio, juce::MidiBuffer &) {
    processAudio(audio, true);
}
void DarkVoiceProcessor::processAudio(juce::AudioBuffer<float> &audio, bool forceBypass) {
    juce::ScopedNoDenormals noDenormals;
    if (!prepared_ || audio.getNumChannels() != getTotalNumInputChannels() ||
        audio.getNumChannels() < 1 || audio.getNumChannels() > 2) {
        audio.clear();
        inputPeak_.store(0, std::memory_order_relaxed);
        outputPeak_.store(0, std::memory_order_relaxed);
        return;
    }
    updateHostTempo();
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
        const auto channelSpan =
            std::span<float *>(channels.data(), static_cast<std::size_t>(audio.getNumChannels()));
        if (!(forceBypass ? engine_.processBypassed(channelSpan, count)
                          : engine_.process(channelSpan, count))) {
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
    state.setProperty("schemaVersion", darkvoice::currentSchemaVersion, nullptr);
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
    if (version != "1" && version != "2" && version != "3" && version != "4" && version != "5" &&
        version != "6" && version != "7" && version != "8" && version != "9" && version != "10" &&
        version != "11" && version != "12" && version != "13" && version != "14")
        return;
    const auto count = darkvoice::parameterCountForVersion(version.getIntValue());
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
        const auto numberText = child.getProperty("value").toString().trim().toStdString();
        if (spec.boolean && !darkvoice::exactBooleanNumber(numberText))
            return;
        if ((id == "stereoDelayMode" && !darkvoice::exactSmallIntegerNumber(numberText, 2)) ||
            (id == "stereoDelayDivision" && !darkvoice::exactSmallIntegerNumber(numberText, 5)) ||
            (id == "spaceModel" && !darkvoice::exactSmallIntegerNumber(numberText, 2)) ||
            (id == "colourModel" && !darkvoice::exactSmallIntegerNumber(numberText, 1)))
            return;
        std::istringstream parser(numberText);
        parser.imbue(std::locale::classic());
        float value = 0;
        parser >> std::noskipws >> value;
        if (!parser || parser.peek() != std::char_traits<char>::eof() || !std::isfinite(value) ||
            value < spec.minimum || value > spec.maximum ||
            (spec.boolean && value != 0 && value != 1))
            return;
        if ((id == "stereoDelayMode" || id == "stereoDelayDivision" || id == "spaceModel" ||
             id == "colourModel") &&
            std::trunc(value) != value)
            return;
        seen[index] = true;
        values[index] = value;
    }
    // Build canonical complete state only after the entire snapshot validates.
    juce::ValueTree migrated("DarkVoiceState");
    migrated.setProperty("schemaVersion", darkvoice::currentSchemaVersion, nullptr);
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
        if (juce::String(darkvoice::parameterSchema[i].id) != "bypass")
            values.emplace(darkvoice::parameterSchema[i].id,
                           raw_[i]->load(std::memory_order_relaxed));
    return values;
}
bool DarkVoiceProcessor::applyPreset(const darkvoice::PresetValues &values) {
    if (darkvoice::PresetBank::validate(values).failed())
        return false;
    for (const auto &spec : darkvoice::parameterSchema) {
        if (juce::String(spec.id) == "bypass")
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
