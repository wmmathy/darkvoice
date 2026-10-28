#include "PluginEditor.hpp"
#include <cmath>
namespace {
const juce::Colour background{0xff111418}, panel{0xff1c2229}, accent{0xffe54b52}, text{0xffecedf0};
constexpr std::array<const char *, 61> ids{"inputDb",
                                           "highPassHz",
                                           "drive",
                                           "thresholdDb",
                                           "ratio",
                                           "mix",
                                           "outputDb",
                                           "eqLowHz",
                                           "eqLowGainDb",
                                           "eqMidHz",
                                           "eqMidGainDb",
                                           "eqMidQ",
                                           "eqHighHz",
                                           "eqHighGainDb",
                                           "delayMs",
                                           "delayFeedback",
                                           "delayMix",
                                           "reverbSize",
                                           "reverbDamping",
                                           "reverbMix",
                                           "gateThresholdDb",
                                           "gateRatio",
                                           "gateReleaseMs",
                                           "deEssHz",
                                           "deEssThresholdDb",
                                           "deEssReductionDb",
                                           "chorusRateHz",
                                           "chorusDepthMs",
                                           "chorusMix",
                                           "stereoWidth",
                                           "flangerRateHz",
                                           "flangerDepthMs",
                                           "flangerFeedback",
                                           "flangerMix",
                                           "limiterEnabled",
                                           "limiterCeilingDb",
                                           "limiterReleaseMs",
                                           "stereoDelayMode",
                                           "stereoDelayMs",
                                           "stereoDelayFeedback",
                                           "stereoDelayMix",
                                           "stereoDelaySync",
                                           "stereoDelayBpm",
                                           "stereoDelayDivision",
                                           "stereoDelayHostTempo",
                                           "doublerSeparationMs",
                                           "doublerVariationMs",
                                           "doublerRateHz",
                                           "doublerMix",
                                           "spaceModel",
                                           "spaceDecay",
                                           "spaceToneHz",
                                           "spacePreDelayMs",
                                           "spaceMix",
                                           "colourModel",
                                           "colourDrive",
                                           "colourToneHz",
                                           "colourMix",
                                           "sculptBodyDb",
                                           "sculptWarmthDb",
                                           "sculptPresenceDb"};
constexpr std::array<const char *, 61> names{"INPUT GAIN",
                                             "HIGH-PASS",
                                             "WARMTH DRIVE",
                                             "COMPRESSOR THRESHOLD",
                                             "COMPRESSOR RATIO",
                                             "DRY / WET",
                                             "OUTPUT GAIN",
                                             "LOW SHELF FREQUENCY",
                                             "LOW SHELF GAIN",
                                             "MID FREQUENCY",
                                             "MID GAIN",
                                             "MID Q",
                                             "HIGH SHELF FREQUENCY",
                                             "HIGH SHELF GAIN",
                                             "DELAY TIME",
                                             "DELAY FEEDBACK",
                                             "DELAY MIX",
                                             "REVERB SIZE",
                                             "REVERB DAMPING",
                                             "REVERB MIX",
                                             "GATE THRESHOLD",
                                             "GATE RATIO (1 = OFF)",
                                             "GATE RELEASE",
                                             "DE-ESSER FREQUENCY",
                                             "DE-ESSER THRESHOLD",
                                             "DE-ESSER MAX REDUCTION",
                                             "CHORUS RATE",
                                             "CHORUS DEPTH",
                                             "CHORUS MIX",
                                             "STEREO WIDTH",
                                             "FLANGER RATE",
                                             "FLANGER DEPTH",
                                             "FLANGER FEEDBACK",
                                             "FLANGER MIX",
                                             "LIMITER ENABLE (0 = OFF, 1 = ON)",
                                             "LIMITER CEILING",
                                             "LIMITER RELEASE",
                                             "STEREO DELAY MODE",
                                             "STEREO DELAY TIME",
                                             "STEREO DELAY FEEDBACK",
                                             "STEREO DELAY MIX",
                                             "STEREO DELAY SYNC (0 / 1)",
                                             "MANUAL TEMPO",
                                             "BEAT DIVISION",
                                             "USE HOST TEMPO (0 / 1)",
                                             "DOUBLER SEPARATION",
                                             "DOUBLER VARIATION",
                                             "DOUBLER RATE",
                                             "DOUBLER MIX",
                                             "SPACE MODEL",
                                             "SPACE DECAY",
                                             "SPACE HF TONE",
                                             "SPACE PREDELAY",
                                             "SPACE MIX",
                                             "COLOUR STYLE",
                                             "COLOUR DRIVE",
                                             "COLOUR HF TONE",
                                             "COLOUR PARALLEL MIX",
                                             "BODY 180 Hz",
                                             "WARMTH TILT",
                                             "PRESENCE 3 kHz"};
constexpr std::array<const char *, 61> suffixes{
    " dB", " Hz", "", " dB",  ":1",  "",    " dB", " Hz", " dB", " Hz", " dB", "",    " Hz",
    " dB", " ms", "", "",     "",    "",    "",    " dB", ":1",  " ms", " Hz", " dB", " dB",
    " Hz", " ms", "", "",     " Hz", " ms", "",    "",    "",    " dB", " ms", "",    " ms",
    "",    "",    "", " BPM", "",    "",    " ms", " ms", " Hz", "",    "",    "",    " Hz",
    " ms", "",    "", "",     " Hz", "",    " dB", " dB", " dB"};
juce::File presetDirectory(const juce::File &supplied) {
    if (supplied != juce::File{})
        return supplied;
#if JUCE_LINUX || JUCE_BSD
    // JUCE 8.0.6's Linux special-location helper reads user-dirs.dirs rather
    // than the XDG_CONFIG_HOME process variable. Honor the standard override.
    const auto configured = juce::SystemStats::getEnvironmentVariable("XDG_CONFIG_HOME", {});
    if (juce::File::isAbsolutePath(configured))
        return juce::File(configured).getChildFile("DarkVoiceStudio/Presets");
#endif
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("DarkVoiceStudio/Presets");
}
} // namespace
void DarkVoiceLookAndFeel::drawTabButton(juce::TabBarButton &button, juce::Graphics &g, bool hover,
                                         bool down) {
    auto area = button.getLocalBounds().reduced(4, 3);
    g.setColour(button.isFrontTab() ? juce::Colour(0xff34303a)
                                    : (hover || down ? juce::Colour(0xff293039) : panel));
    g.fillRoundedRectangle(area.toFloat(), 5.0f);
    if (button.isFrontTab()) {
        g.setColour(accent);
        g.fillRect(area.getX(), area.getY() + 5, 3, area.getHeight() - 10);
    }
    g.setColour(button.isFrontTab() ? text : juce::Colour(0xffb5bdc7));
    g.setFont(juce::Font(
        juce::FontOptions(12.0f, button.isFrontTab() ? juce::Font::bold : juce::Font::plain)));
    g.drawFittedText(button.getButtonText(), area.reduced(10, 0), juce::Justification::centredLeft,
                     1);
}
DarkVoiceEditor::DarkVoiceEditor(DarkVoiceProcessor &p, juce::File root)
    : AudioProcessorEditor(p), processor_(p), bank_(presetDirectory(root)) {
    look_.setColour(juce::Slider::thumbColourId, accent);
    look_.setColour(juce::Slider::trackColourId, accent);
    look_.setColour(juce::Slider::backgroundColourId, juce::Colour(0xff303943));
    look_.setColour(juce::Slider::textBoxTextColourId, text);
    look_.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    look_.setColour(juce::Slider::textBoxBackgroundColourId, panel);
    look_.setColour(juce::ToggleButton::textColourId, text);
    look_.setColour(juce::ToggleButton::tickColourId, accent);
    look_.setColour(juce::ComboBox::backgroundColourId, panel);
    look_.setColour(juce::TextEditor::backgroundColourId, panel);
    look_.setColour(juce::TextButton::buttonColourId, panel);
    setLookAndFeel(&look_);
    tabs_.setName("Processing groups");
    tabs_.setTabBarDepth(170);
    tabs_.addTab("DYNAMICS / TONE", panel, &pages_[0], false);
    tabs_.addTab("PARAMETRIC EQ", panel, &pages_[1], false);
    tabs_.addTab("DELAY / REVERB", panel, &pages_[2], false);
    tabs_.addTab("GATE / EXPANDER", panel, &pages_[3], false);
    tabs_.addTab("DE-ESSER", panel, &pages_[4], false);
    tabs_.addTab("CHORUS / WIDTH", panel, &pages_[5], false);
    tabs_.addTab("FLANGER", panel, &pages_[6], false);
    tabs_.addTab("LIMITER", panel, &pages_[7], false);
    tabs_.addTab("STEREO DELAY", panel, &pages_[8], false);
    tabs_.addTab("SIMULATED DOUBLER", panel, &pages_[9], false);
    tabs_.addTab("SPACE REVERB", panel, &pages_[10], false);
    tabs_.addTab("COLOUR", panel, &pages_[11], false);
    tabs_.addTab("VOICE SCULPTOR", panel, &pages_[12], false);
    addAndMakeVisible(tabs_);
    for (std::size_t i = 0; i < sliders_.size(); ++i) {
        auto &slider = sliders_[i];
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 90, 28);
        slider.setTextValueSuffix(suffixes[i]);
        slider.setName(names[i]);
        slider.setTitle(names[i]);
        slider.setTooltip(
            i == 59 ? "Warmth tilt: +6 dB span gives +3 dB low-frequency gain and -3 dB "
                      "high-frequency gain, around 450 Hz."
            : i >= 58
                ? "Vocal tone shaping: Body at 180 Hz and Presence at 3 kHz. Zero dB is neutral."
            : i >= 54
                ? "Tape/tube-style saturation with 8x oversampling, HF tone and parallel mix. "
                  "Fixed 56-sample alignment."
            : i >= 49 ? "Distinct algorithmic Room / Hall / Plate networks. HF tone is output "
                        "filtering; Decay is not an RT60 time."
            : i >= 45
                ? "Simulated double tracking: two independently wandering delayed copies; not "
                  "another performance or harmony."
            : i >= 37
                ? "Stereo repeats, single echo or ping-pong. Sync uses beat division; host BPM "
                  "falls back to manual if unavailable."
            : i >= 34
                ? "Default off. Final-output sample-peak limiting, not true-peak. Enable 0 is "
                  "off; 1 is on."
            : i >= 30 ? "Flanger mix 0 is off. Signed feedback changes the comb character."
            : i >= 26 ? "Chorus mix 0 is off. Width 1 is neutral; 0 folds stereo to mono. "
                        "Wide settings can increase peaks; width is a no-op in mono."
            : i >= 23 ? "Split-band de-esser; max reduction 0 disables attenuation. "
                        "Stereo-linked high-band detector."
            : i >= 20 ? "Linked downward expander; ratio 1 disables attenuation. "
                        "Detector follows the compressed wet signal."
                      : "Automatable " + juce::String(names[i]).toLowerCase());
        const auto page = i < 7    ? 0U
                          : i < 14 ? 1U
                          : i < 20 ? 2U
                          : i < 23 ? 3U
                          : i < 26 ? 4U
                          : i < 30 ? 5U
                          : i < 34 ? 6U
                          : i < 37 ? 7U
                          : i < 45 ? 8U
                          : i < 49 ? 9U
                          : i < 54 ? 10U
                          : i < 58 ? 11U
                                   : 12U;
        pages_[page].addAndMakeVisible(slider);
        labels_[i].setText(names[i], juce::dontSendNotification);
        labels_[i].setColour(juce::Label::textColourId, juce::Colour(0xffb5bdc7));
        labels_[i].setFont(juce::Font(juce::FontOptions(12.0f)));
        pages_[page].addAndMakeVisible(labels_[i]);
        attachments_[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            p.parameters, ids[i], slider);
    }
    // Discrete controls display musical choices rather than unexplained indices.
    sliders_[37].textFromValueFunction = [](double value) {
        constexpr const char *modes[]{"Stereo", "Single", "Ping-pong"};
        return juce::String(modes[std::clamp(static_cast<int>(std::round(value)), 0, 2)]);
    };
    sliders_[43].textFromValueFunction = [](double value) {
        constexpr const char *divisions[]{"1/16",        "1/8 triplet", "1/8",
                                          "1/4 triplet", "Dotted 1/8",  "1/4"};
        return juce::String(divisions[std::clamp(static_cast<int>(std::round(value)), 0, 5)]);
    };
    sliders_[37].setTextBoxStyle(juce::Slider::TextBoxRight, true, 110, 28);
    sliders_[43].setTextBoxStyle(juce::Slider::TextBoxRight, true, 110, 28);
    sliders_[49].textFromValueFunction = [](double value) {
        constexpr const char *models[]{"Room", "Hall", "Plate"};
        return juce::String(models[std::clamp(static_cast<int>(std::round(value)), 0, 2)]);
    };
    sliders_[49].setTextBoxStyle(juce::Slider::TextBoxRight, true, 110, 28);
    sliders_[54].textFromValueFunction = [](double value) {
        return juce::String(value < .5 ? "Tape" : "Tube");
    };
    sliders_[54].setTextBoxStyle(juce::Slider::TextBoxRight, true, 110, 28);
    tempo_.setName("Stereo delay tempo status");
    tempo_.setColour(juce::Label::textColourId, juce::Colour(0xffb5bdc7));
    tempo_.setFont(juce::Font(juce::FontOptions(12.0f)));
    pages_[8].addAndMakeVisible(tempo_);
    bypass_.setName("Bypass processing");
    addAndMakeVisible(bypass_);
    bypassAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        p.parameters, "bypass", bypass_);
    meter_.setColour(juce::Label::textColourId, text);
    meter_.setJustificationType(juce::Justification::centredRight);
    status_.setColour(juce::Label::textColourId, juce::Colour(0xffb5bdc7));
    status_.setFont(juce::Font(juce::FontOptions(12.0f)));
    addAndMakeVisible(meter_);
    pitch_.setColour(juce::Label::textColourId, juce::Colour(0xffb5bdc7));
    pitch_.setName("Dry pitch analysis");
    addAndMakeVisible(pitch_);
    addAndMakeVisible(status_);
    search_.setName("Search presets");
    search_.setTextToShowWhenEmpty("Search presets", juce::Colour(0xff9ba6b4));
    name_.setName("Preset name");
    name_.setTextToShowWhenEmpty("New preset name", juce::Colour(0xff9ba6b4));
    presets_.setName("Preset selection");
    presets_.setTextWhenNothingSelected("Choose a sound");
    favorite_.setName("Favorite selected preset");
    favoritesOnly_.setName("Favorites only");
    for (auto *component : std::initializer_list<juce::Component *>{
             &search_, &name_, &presets_, &favoritesOnly_, &favorite_, &load_, &save_, &rename_,
             &import_, &export_})
        addAndMakeVisible(component);
    load_.setName("Load preset");
    save_.setName("Save preset as");
    rename_.setName("Rename preset");
    import_.setName("Import preset");
    export_.setName("Export preset");
    search_.onTextChange = [this] { refreshPresets(); };
    favoritesOnly_.onClick = [this] { refreshPresets(); };
    presets_.onChange = [this] {
        if (const auto *preset = bank_.find(selectedId())) {
            name_.setText(preset->name, false);
            favorite_.setToggleState(preset->favorite, juce::dontSendNotification);
            rename_.setEnabled(!preset->factory);
        } else
            rename_.setEnabled(false);
    };
    load_.onClick = [this] { loadSelected(); };
    save_.onClick = [this] {
        juce::String created;
        const auto result =
            bank_.saveAs(name_.getText(), processor_.snapshotPresetValues(), created);
        showResult(result, "Saved a new user preset");
        if (result.wasOk())
            refreshPresets(created);
    };
    rename_.onClick = [this] {
        const auto id = selectedId();
        const auto result = bank_.rename(id, name_.getText());
        showResult(result, "Renamed user preset");
        if (result.wasOk())
            refreshPresets(id);
    };
    favorite_.onClick = [this] {
        const auto id = selectedId();
        const auto result = bank_.setFavorite(id, favorite_.getToggleState());
        showResult(result, "Updated favorite");
        refreshPresets(id);
    };
    import_.onClick = [this] { chooseImport(); };
    export_.onClick = [this] { chooseExport(); };
    const auto result = bank_.refresh();
    refreshPresets();
    showResult(result, "6 factory sounds / Save as creates a user copy");
    setResizable(true, true);
    setResizeLimits(900, 780, 1200, 1000);
    setSize(900, 780);
    timerCallback();
    startTimerHz(20);
}
DarkVoiceEditor::~DarkVoiceEditor() {
    stopTimer();
    chooser_.reset();
    setLookAndFeel(nullptr);
}
void DarkVoiceEditor::refreshPresets(const juce::String &requested) {
    const auto select = requested.isEmpty() ? selectedId() : requested;
    presets_.clear(juce::dontSendNotification);
    visibleIds_.clear();
    for (const auto &preset : bank_.search(search_.getText(), favoritesOnly_.getToggleState())) {
        visibleIds_.push_back(preset.id);
        presets_.addItem((preset.favorite ? "* " : "") + preset.name +
                             (preset.factory ? "  [factory]" : ""),
                         static_cast<int>(visibleIds_.size()));
        if (preset.id == select)
            presets_.setSelectedId(static_cast<int>(visibleIds_.size()),
                                   juce::dontSendNotification);
    }
    if (presets_.getSelectedId() == 0 && !visibleIds_.empty())
        presets_.setSelectedId(1, juce::dontSendNotification);
    if (presets_.onChange)
        presets_.onChange();
}
juce::String DarkVoiceEditor::selectedId() const {
    const auto index = presets_.getSelectedId() - 1;
    return index >= 0 && static_cast<std::size_t>(index) < visibleIds_.size()
               ? visibleIds_[static_cast<std::size_t>(index)]
               : juce::String{};
}
void DarkVoiceEditor::showResult(const juce::Result &result, const juce::String &success) {
    status_.setText(result.wasOk() ? success : result.getErrorMessage(),
                    juce::dontSendNotification);
}
void DarkVoiceEditor::loadSelected() {
    const auto *preset = bank_.find(selectedId());
    if (!preset) {
        status_.setText("Choose a preset first", juce::dontSendNotification);
        return;
    }
    const auto name = preset->name;
    status_.setText(processor_.applyPreset(preset->values)
                        ? "Loaded " + name + " / bypass preserved"
                        : "Preset parameters rejected",
                    juce::dontSendNotification);
}
void DarkVoiceEditor::chooseImport() {
    chooser_ =
        std::make_unique<juce::FileChooser>("Import a DarkVoice preset", juce::File{}, "*.json");
    juce::Component::SafePointer<DarkVoiceEditor> safe(this);
    chooser_->launchAsync(juce::FileBrowserComponent::openMode |
                              juce::FileBrowserComponent::canSelectFiles,
                          [safe](const juce::FileChooser &chooser) {
                              if (!safe)
                                  return;
                              const auto file = chooser.getResult();
                              if (file == juce::File{})
                                  return;
                              juce::String created;
                              const auto result = safe->bank_.importFile(file, created);
                              safe->showResult(result, "Imported a new user preset");
                              if (result.wasOk())
                                  safe->refreshPresets(created);
                          });
}
void DarkVoiceEditor::chooseExport() {
    const auto id = selectedId();
    if (id.isEmpty()) {
        status_.setText("Choose a preset first", juce::dontSendNotification);
        return;
    }
    chooser_ = std::make_unique<juce::FileChooser>("Export preset to a new JSON file", juce::File{},
                                                   "*.json");
    juce::Component::SafePointer<DarkVoiceEditor> safe(this);
    chooser_->launchAsync(juce::FileBrowserComponent::saveMode |
                              juce::FileBrowserComponent::canSelectFiles,
                          [safe, id](const juce::FileChooser &chooser) {
                              if (!safe)
                                  return;
                              const auto file = chooser.getResult();
                              if (file == juce::File{})
                                  return;
                              safe->showResult(safe->bank_.exportFile(id, file), "Exported preset");
                          });
}
void DarkVoiceEditor::paint(juce::Graphics &g) {
    g.fillAll(background);
    g.setColour(accent);
    g.fillRect(24, 24, 4, 45);
    g.setColour(text);
    g.setFont(juce::Font(juce::FontOptions(27.0f, juce::Font::bold)));
    g.drawText(processor_.getName().toUpperCase(), 40, 19, getWidth() - 65, 34,
               juce::Justification::centredLeft);
    g.setFont(juce::Font(juce::FontOptions(13.0f)));
    g.setColour(juce::Colour(0xff9ba6b4));
    g.drawText("DEVELOPMENT BUILD  /  Real DSP / Local presets / Editable sound", 40, 55,
               getWidth() - 65, 24, juce::Justification::centredLeft);
    g.setFont(juce::Font(juce::FontOptions(12.0f)));
    g.drawText(
        "Latency: " + juce::String(processor_.getLatencySamples()) +
            " samples / Peak meters; LUFS, pitch correction, AI and true-peak mastering pending",
        24, getHeight() - 30, getWidth() - 48, 22, juce::Justification::centredLeft);
}
void DarkVoiceEditor::resized() {
    const int width = getWidth() - 48;
    search_.setBounds(24, 99, 180, 30);
    presets_.setBounds(216, 99, width - 390, 30);
    load_.setBounds(getWidth() - 192, 99, 72, 30);
    favorite_.setBounds(getWidth() - 115, 99, 95, 30);
    name_.setBounds(24, 141, 240, 30);
    save_.setBounds(274, 141, 82, 30);
    rename_.setBounds(364, 141, 82, 30);
    import_.setBounds(454, 141, 70, 30);
    export_.setBounds(532, 141, 70, 30);
    favoritesOnly_.setBounds(610, 141, 120, 30);
    status_.setBounds(24, 178, width, 28);
    tabs_.setBounds(24, 214, width, getHeight() - 316);
    const int contentWidth = tabs_.getWidth() - tabs_.getTabBarDepth() - 8;
    for (std::size_t i = 0; i < sliders_.size(); ++i) {
        const auto page = i < 7    ? 0U
                          : i < 14 ? 1U
                          : i < 20 ? 2U
                          : i < 23 ? 3U
                          : i < 26 ? 4U
                          : i < 30 ? 5U
                          : i < 34 ? 6U
                          : i < 37 ? 7U
                          : i < 45 ? 8U
                          : i < 49 ? 9U
                          : i < 54 ? 10U
                          : i < 58 ? 11U
                                   : 12U;
        const auto row = i < 7    ? i
                         : i < 14 ? i - 7
                         : i < 20 ? i - 14
                         : i < 23 ? i - 20
                         : i < 26 ? i - 23
                         : i < 30 ? i - 26
                         : i < 34 ? i - 30
                         : i < 37 ? i - 34
                         : i < 45 ? i - 37
                         : i < 49 ? i - 45
                         : i < 54 ? i - 49
                         : i < 58 ? i - 54
                                  : i - 58;
        const int rowHeight = (tabs_.getHeight() - 48) / (page == 8 ? 9 : 7);
        const int y = 14 + static_cast<int>(row) * rowHeight;
        labels_[i].setBounds(16, y, 205, 24);
        sliders_[i].setBounds(228, y - 2, contentWidth - 258, 29);
        (void)page;
    }
    tempo_.setBounds(16, tabs_.getHeight() - 66, contentWidth - 32, 24);
    bypass_.setBounds(28, getHeight() - 82, 200, 28);
    meter_.setBounds(240, getHeight() - 82, getWidth() - 268, 28);
    pitch_.setBounds(24, getHeight() - 56, getWidth() - 48, 22);
}
void DarkVoiceEditor::timerCallback() {
    const auto actualBpm = processor_.hostTempoBpm();
    const auto manualBpm = processor_.parameters.getRawParameterValue("stereoDelayBpm")->load();
    const bool useHost =
        processor_.parameters.getRawParameterValue("stereoDelayHostTempo")->load() >= .5f;
    tempo_.setText(actualBpm > 0 ? "Host tempo available: " + juce::String(actualBpm, 1) +
                                       " BPM / " + (useHost ? "host selected" : "manual selected")
                                 : "Host tempo unavailable / manual fallback: " +
                                       juce::String(manualBpm, 1) + " BPM",
                   juce::dontSendNotification);

    const float frequency = processor_.dryPitchHz(), confidence = processor_.dryPitchConfidence();
    if (frequency > 0 && std::isfinite(frequency) && confidence >= 0.8f) {
        const double midi = 69.0 + 12.0 * std::log2(frequency / 440.0);
        const int nearest = static_cast<int>(std::round(midi));
        constexpr const char *notes[]{"C",  "C#", "D",  "D#", "E",  "F",
                                      "F#", "G",  "G#", "A",  "A#", "B"};
        pitch_.setText("Dry input: " + juce::String(notes[nearest % 12]) +
                           juce::String(nearest / 12 - 1) + " / " + juce::String(frequency, 1) +
                           " Hz / " + juce::String((midi - nearest) * 100, 0) +
                           " cents / confidence " + juce::String(confidence * 100, 0) +
                           "% (analysis only)",
                       juce::dontSendNotification);
    } else
        pitch_.setText("Dry input pitch: -- (analysis only, no correction)",
                       juce::dontSendNotification);
    const auto db = [](float value) {
        return value > 0 ? juce::String(juce::Decibels::gainToDecibels(value), 1)
                         : juce::String("-inf");
    };
    meter_.setText("IN " + db(processor_.inputPeak()) + " dBFS    OUT " +
                       db(processor_.outputPeak()) + " dBFS",
                   juce::dontSendNotification);
}
