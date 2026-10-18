namespace audio_plugin {

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p) {
  auto setupSlider = [this](juce::Slider& s, juce::Label& l,
                            const juce::String& name) {
    l.setText(name, juce::dontSendNotification);
    l.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(l);
    s.setSliderStyle(juce::Slider::LinearVertical);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    s.textFromValueFunction = [](double value) {
      if (value <= 0.0)
        return juce::String("-inf dB");
      return juce::String(juce::Decibels::gainToDecibels((float)value), 1) +
             " dB";
    };
    addAndMakeVisible(s);
  };

  setupSlider(masterSlider, masterLabel, "Master");
  setupSlider(gainSliderL, gainLabelL, "L");
  setupSlider(gainSliderR, gainLabelR, "R");

  addAndMakeVisible(linkButton);
  attachLink =
      std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
          processorRef.apvts, "linked", linkButton);
  linkButton.onClick = [this] { updateEnabledStates(); };

  addAndMakeVisible(msButton);
  attachMS =
      std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
          processorRef.apvts, "midSideMode", msButton);
  msButton.onClick = [this] {
    updateLabelTexts();
    updateChannelNames();
  };

  // Bypass 按钮
  addAndMakeVisible(bypassButton);
  attachBypass =
      std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
          processorRef.apvts, "bypass", bypassButton);
  bypassButton.setColour(juce::ToggleButton::textColourId,
                         juce::Colours::orange);

  addAndMakeVisible(levelMeter);

  auto setupKnob = [this](juce::Slider& s, juce::Label& l) {
    l.setJustificationType(juce::Justification::centred);
    l.setFont(12.0f);
    addAndMakeVisible(l);
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(s);
  };
  setupKnob(thresholdSlider, thresholdLabel);
  setupKnob(shapeSlider, shapeLabel);

  thresholdSlider.onValueChange = [this] {
    auto v = (float)thresholdSlider.getValue();
    juce::String str =
        v <= 0.0 ? "-inf dB"
                 : juce::String(juce::Decibels::gainToDecibels(v), 1) + " dB";
    thresholdLabel.setText("Thresh: " + str, juce::dontSendNotification);
  };
  shapeSlider.onValueChange = [this] {
    auto v = (float)shapeSlider.getValue();
    shapeLabel.setText("Shape: " + juce::String(v * 100.0f, 0) + "%",
                       juce::dontSendNotification);
  };

  addAndMakeVisible(softClipButton);
  attachSoftClip =
      std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
          processorRef.apvts, "softClipOn", softClipButton);
  softClipButton.onStateChange = [this] { updateSoftClipKnobs(); };

  attachMaster =
      std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
          processorRef.apvts, "gainMaster", masterSlider);
  attachL =
      std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
          processorRef.apvts, "gainL", gainSliderL);
  attachR =
      std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
          processorRef.apvts, "gainR", gainSliderR);
  attachThresh =
      std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
          processorRef.apvts, "threshold", thresholdSlider);
  attachShape =
      std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
          processorRef.apvts, "shape", shapeSlider);

  updateEnabledStates();
  updateLabelTexts();
  updateChannelNames();
  updateSoftClipKnobs();

  setResizable(true, true);
  setResizeLimits(420, 440, 1200, 1000);
  setSize(560, 500);
  startTimerHz(30);
}

PluginEditor::~PluginEditor() = default;

void PluginEditor::updateEnabledStates() {
  const bool linked = linkButton.getToggleState();
  masterSlider.setEnabled(linked);
  gainSliderL.setEnabled(!linked);
  gainSliderR.setEnabled(!linked);
  masterSlider.setAlpha(linked ? 1.0f : 0.35f);
  gainSliderL.setAlpha(linked ? 0.35f : 1.0f);
  gainSliderR.setAlpha(linked ? 0.35f : 1.0f);
}

void PluginEditor::updateLabelTexts() {
  const bool ms = msButton.getToggleState();
  gainLabelL.setText(ms ? "M" : "L", juce::dontSendNotification);
  gainLabelR.setText(ms ? "S" : "R", juce::dontSendNotification);
}

void PluginEditor::updateChannelNames() {
  const bool ms = msButton.getToggleState();
  levelMeter.setChannelNames(ms ? "M" : "L", ms ? "S" : "R");
}

void PluginEditor::updateSoftClipKnobs() {
  const bool on = softClipButton.getToggleState();
  thresholdSlider.setEnabled(on);
  shapeSlider.setEnabled(on);
  thresholdSlider.setAlpha(on ? 1.0f : 0.35f);
  shapeSlider.setAlpha(on ? 1.0f : 0.35f);
  if (thresholdSlider.onValueChange)
    thresholdSlider.onValueChange();
  if (shapeSlider.onValueChange)
    shapeSlider.onValueChange();
  repaint();
}

void PluginEditor::paint(juce::Graphics& g) {
  g.fillAll(
      getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void PluginEditor::resized() {
  auto area = getLocalBounds();
  const int margin = juce::jlimit(8, 24, (int)(area.getWidth() * 0.03f));
  area = area.reduced(margin);

  // 顶行：Link | M/S | Bypass 三等分
  const int topH = juce::jlimit(24, 36, (int)(area.getHeight() * 0.07f));
  auto topRow = area.removeFromTop(topH);
  auto btnW = topRow.getWidth() / 3;
  linkButton.setBounds(topRow.removeFromLeft(btnW).reduced(4, 0));
  msButton.setBounds(topRow.removeFromLeft(btnW).reduced(4, 0));
  bypassButton.setBounds(topRow.reduced(4, 0));

  area.removeFromTop(juce::jlimit(4, 10, (int)(area.getHeight() * 0.02f)));

  const int meterW = juce::jlimit(50, 110, (int)(area.getWidth() * 0.15f));
  auto meterArea = area.removeFromRight(meterW);
  const int meterBottomPad =
      juce::jlimit(12, 22, (int)(meterArea.getHeight() * 0.05f));
  levelMeter.setBounds(
      meterArea.reduced(4, 0).withTrimmedBottom(meterBottomPad));

  area.removeFromRight(juce::jlimit(4, 12, (int)(area.getWidth() * 0.02f)));

  const int bottomAreaH =
      juce::jlimit(80, 110, (int)(area.getHeight() * 0.22f));
  auto bottomArea = area.removeFromBottom(bottomAreaH);
  auto thirdW = bottomArea.getWidth() / 3;
  softClipButton.setBounds(
      bottomArea.removeFromLeft(thirdW).withSizeKeepingCentre(thirdW - 10, 24));
  auto knobArea = bottomArea;
  auto halfW = knobArea.getWidth() / 2;
  thresholdSlider.setBounds(knobArea.removeFromLeft(halfW).reduced(8));
  thresholdLabel.setBounds(thresholdSlider.getBounds()
                               .withY(thresholdSlider.getBottom())
                               .withHeight(16));
  shapeSlider.setBounds(knobArea.reduced(8));
  shapeLabel.setBounds(
      shapeSlider.getBounds().withY(shapeSlider.getBottom()).withHeight(16));

  const int labelH = juce::jlimit(18, 28, (int)(area.getHeight() * 0.06f));
  auto labelArea = area.removeFromBottom(labelH);
  auto thirdLabel = labelArea.getWidth() / 3;
  masterLabel.setBounds(labelArea.removeFromLeft(thirdLabel)
                            .withSizeKeepingCentre(thirdLabel, 24));
  gainLabelL.setBounds(labelArea.removeFromLeft(thirdLabel)
                           .withSizeKeepingCentre(thirdLabel, 24));
  gainLabelR.setBounds(labelArea.withSizeKeepingCentre(thirdLabel, 24));

  auto sliderW = area.getWidth() / 3;
  masterSlider.setBounds(area.removeFromLeft(sliderW).reduced(6, 0));
  gainSliderL.setBounds(area.removeFromLeft(sliderW).reduced(6, 0));
  gainSliderR.setBounds(area.reduced(6, 0));
}

void PluginEditor::timerCallback() {
  levelMeter.setLevels(processorRef.leftLevel.load(std::memory_order_relaxed),
                       processorRef.rightLevel.load(std::memory_order_relaxed));
}

// createEditor 实现放在这里：PluginEditor.cpp 才需要完整类型
juce::AudioProcessorEditor* PluginProcessor::createEditor() {
  return new PluginEditor(*this);
}

}  // namespace audio_plugin