#include "PluginEditor.hpp"
namespace {
const juce::Colour background{0xff111418}, panel{0xff1c2229}, accent{0xffe54b52}, text{0xffecedf0};
constexpr std::array<const char *, 20> ids{
    "inputDb",       "highPassHz", "drive",      "thresholdDb",   "ratio",
    "mix",           "outputDb",   "eqLowHz",    "eqLowGainDb",   "eqMidHz",
    "eqMidGainDb",   "eqMidQ",     "eqHighHz",   "eqHighGainDb",  "delayMs",
    "delayFeedback", "delayMix",   "reverbSize", "reverbDamping", "reverbMix"};
constexpr std::array<const char *, 20> names{"INPUT GAIN",
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
                                             "REVERB MIX"};
constexpr std::array<const char *, 20> suffixes{" dB", " Hz", "",    " dB", ":1", "",    " dB",
                                                " Hz", " dB", " Hz", " dB", "",   " Hz", " dB",
                                                " ms", "",    "",    "",    "",   ""};
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
    tabs_.addTab("DYNAMICS / TONE", panel, &pages_[0], false);
    tabs_.addTab("PARAMETRIC EQ", panel, &pages_[1], false);
    tabs_.addTab("DELAY / REVERB", panel, &pages_[2], false);
    addAndMakeVisible(tabs_);
    for (std::size_t i = 0; i < sliders_.size(); ++i) {
        auto &slider = sliders_[i];
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 90, 28);
        slider.setTextValueSuffix(suffixes[i]);
        slider.setName(names[i]);
        slider.setTitle(names[i]);
        slider.setTooltip("Automatable " + juce::String(names[i]).toLowerCase());
        const auto page = i < 7 ? 0U : i < 14 ? 1U : 2U;
        pages_[page].addAndMakeVisible(slider);
        labels_[i].setText(names[i], juce::dontSendNotification);
        labels_[i].setColour(juce::Label::textColourId, juce::Colour(0xffb5bdc7));
        labels_[i].setFont(juce::Font(juce::FontOptions(12.0f)));
        pages_[page].addAndMakeVisible(labels_[i]);
        attachments_[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            p.parameters, ids[i], slider);
    }
    bypass_.setName("Bypass processing");
    addAndMakeVisible(bypass_);
    bypassAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        p.parameters, "bypass", bypass_);
    meter_.setColour(juce::Label::textColourId, text);
    meter_.setJustificationType(juce::Justification::centredRight);
    status_.setColour(juce::Label::textColourId, juce::Colour(0xffb5bdc7));
    status_.setFont(juce::Font(juce::FontOptions(12.0f)));
    addAndMakeVisible(meter_);
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
    setResizeLimits(760, 720, 1200, 1000);
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
    g.drawText("Peak meters, not LUFS / Pitch / AI / true-peak mastering pending", 24,
               getHeight() - 30, getWidth() - 48, 22, juce::Justification::centredLeft);
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
    for (std::size_t i = 0; i < sliders_.size(); ++i) {
        const auto page = i < 7 ? 0U : i < 14 ? 1U : 2U;
        const auto row = i < 7 ? i : i < 14 ? i - 7 : i - 14;
        const int rowHeight = (tabs_.getHeight() - 48) / 7;
        const int y = 14 + static_cast<int>(row) * rowHeight;
        labels_[i].setBounds(16, y, 205, 24);
        sliders_[i].setBounds(228, y - 2, width - 258, 29);
        (void)page;
    }
    bypass_.setBounds(28, getHeight() - 82, 200, 28);
    meter_.setBounds(240, getHeight() - 82, getWidth() - 268, 28);
}
void DarkVoiceEditor::timerCallback() {
    const auto db = [](float value) {
        return value > 0 ? juce::String(juce::Decibels::gainToDecibels(value), 1)
                         : juce::String("-inf");
    };
    meter_.setText("IN " + db(processor_.inputPeak()) + " dBFS    OUT " +
                       db(processor_.outputPeak()) + " dBFS",
                   juce::dontSendNotification);
}
