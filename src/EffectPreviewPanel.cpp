#include "EffectPreviewPanel.hpp"

EffectPreviewPanel::EffectPreviewPanel(darkvoice::EffectPreviewService &service)
    : service_(service) {
    setName("Effect examples");
    title_.setText("Effect examples", juce::dontSendNotification);
    help_.setText(
        "Uses host output; live settings stay unchanged. Host recordings may capture previews. "
        "Stereo width uses a stereo demonstration and needs stereo output.",
        juce::dontSendNotification);
    for (const auto &definition : darkvoice::previewCatalog())
        effect_.addItem(juce::String::fromUTF8(definition.name.data(),
                                               static_cast<int>(definition.name.size())),
                        definition.id);
    effect_.setName("Preview effect");
    voice_.setName("Preview voice");
    source_.setName("Preview source");
    voice_.addItem("Male", 1);
    voice_.addItem("Female", 2);
    source_.addItem("Auto", 1);
    source_.addItem("Speech", 2);
    source_.addItem("Singing", 3);
    effect_.setSelectedItemIndex(0, juce::dontSendNotification);
    voice_.setSelectedId(1, juce::dontSendNotification);
    source_.setSelectedId(1, juce::dontSendNotification);
    effect_.onChange = voice_.onChange = source_.onChange = [this] { selectionChanged(); };
    processed_.setToggleState(service_.status().processed, juce::dontSendNotification);
    processed_.onClick = [this] { service_.setProcessed(processed_.getToggleState()); };
    level_.setName("Preview level");
    level_.setRange(0, 1, .01);
    level_.setValue(service_.status().level, juce::dontSendNotification);
    level_.setSliderStyle(juce::Slider::LinearHorizontal);
    level_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 24);
    level_.onValueChange = [this] {
        (void)service_.setLevel(static_cast<float>(level_.getValue()));
    };
    play_.onClick = [this] {
        const auto *definition = darkvoice::previewDefinition(
            static_cast<darkvoice::PreviewEffectId>(effect_.getSelectedId()));
        if (!definition)
            return;
        const auto kind = source_.getSelectedId() == 1   ? definition->recommendedSample
                          : source_.getSelectedId() == 2 ? darkvoice::PreviewSampleKind::Speech
                                                         : darkvoice::PreviewSampleKind::Singing;
        (void)service_.request(definition->id,
                               voice_.getSelectedId() == 2 ? darkvoice::PreviewVoice::Female
                                                           : darkvoice::PreviewVoice::Male,
                               kind);
        timerCallback();
    };
    stop_.onClick = [this] {
        service_.stop();
        timerCallback();
    };
    notices_.setMultiLine(true);
    notices_.setReadOnly(true);
    notices_.setText(juce::String::fromUTF8(darkvoice::effectPreviewCredits().c_str()));
    credits_.onClick = [this] { notices_.setVisible(!notices_.isVisible()); };
    for (auto *component : std::initializer_list<juce::Component *>{
             &title_, &help_, &effect_, &voice_, &source_, &play_, &stop_, &processed_, &level_,
             &status_, &levelLabel_, &progressBar_, &credits_})
        addAndMakeVisible(component);
    addChildComponent(notices_);
    setSize(620, 440);
    timerCallback();
    startTimerHz(20);
}
EffectPreviewPanel::~EffectPreviewPanel() {
    stopTimer();
    service_.stop();
}
void EffectPreviewPanel::selectionChanged() {
    service_.stop();
    timerCallback();
}
void EffectPreviewPanel::paint(juce::Graphics &g) {
    g.fillAll(juce::Colour(0xff1c2229));
}
void EffectPreviewPanel::resized() {
    title_.setBounds(16, 8, getWidth() - 32, 28);
    help_.setBounds(16, 38, getWidth() - 32, 68);
    effect_.setBounds(16, 112, getWidth() - 32, 30);
    voice_.setBounds(16, 150, 150, 30);
    source_.setBounds(180, 150, 180, 30);
    play_.setBounds(16, 194, 90, 30);
    stop_.setBounds(116, 194, 90, 30);
    processed_.setBounds(228, 194, 230, 30);
    levelLabel_.setBounds(16, 238, 110, 30);
    level_.setBounds(130, 238, getWidth() - 146, 30);
    status_.setBounds(16, 278, getWidth() - 32, 52);
    progressBar_.setBounds(16, 338, getWidth() - 32, 20);
    credits_.setBounds(16, 378, 180, 30);
    notices_.setBounds(16, 38, getWidth() - 32, 328);
}
void EffectPreviewPanel::timerCallback() {
    const auto state = service_.status();
    juce::String text;
    if (state.blocked)
        text = "Preview stopped: bypass, offline rendering or host recording.";
    else
        switch (state.state) {
        case darkvoice::PreviewState::Idle:
            text = "Choose an effect and press Play.";
            break;
        case darkvoice::PreviewState::Preparing:
            text = "Preparing example...";
            break;
        case darkvoice::PreviewState::WaitingForAudio:
            text = juce::String::fromUTF8("Ready\xe2\x80\x94waiting for host audio");
            break;
        case darkvoice::PreviewState::Playing:
            text = "Playing example through host output";
            break;
        case darkvoice::PreviewState::Stopping:
            text = "Stopping...";
            break;
        case darkvoice::PreviewState::Unavailable:
            text = "Preview unavailable. Start host audio with a supported format.";
            break;
        case darkvoice::PreviewState::Error:
            text = state.error == darkvoice::PreviewError::StereoRequired
                       ? "This demonstration needs stereo output."
                       : "Example unavailable. Try another selection or restart host audio.";
            break;
        }
    const auto *definition = darkvoice::previewDefinition(
        static_cast<darkvoice::PreviewEffectId>(effect_.getSelectedId()));
    const bool stereoRequired =
        definition && definition->stereoDemonstration && state.format.channels != 2;
    if (state.prepared && stereoRequired)
        text = "Stereo output required for this demonstration.";
    status_.setText(text, juce::dontSendNotification);
    progress_ = state.state == darkvoice::PreviewState::Preparing
                    ? static_cast<double>(state.progress) / 100.0
                : state.totalFrames ? static_cast<double>(state.positionFrames) /
                                          static_cast<double>(state.totalFrames)
                                    : 0;
    play_.setEnabled(state.prepared && !stereoRequired);
}
