#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "LevelMeter.h"
#include "PluginProcessor.h"

namespace audio_plugin {
class PluginEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
  explicit PluginEditor(PluginProcessor&);
  ~PluginEditor() override;

  void paint(juce::Graphics&) override;
  void resized() override;

private:
  void timerCallback() override;
  void updateEnabledStates();
  void updateLabelTexts();
  void updateChannelNames();
  void updateSoftClipKnobs();

  PluginProcessor& processorRef;

  juce::Slider masterSlider, gainSliderL, gainSliderR;
  juce::Label masterLabel, gainLabelL, gainLabelR;
  juce::ToggleButton linkButton{"Link"};
  juce::ToggleButton msButton{"M/S"};
  juce::ToggleButton bypassButton{"Bypass"};
  LevelMeter levelMeter;

  juce::Slider thresholdSlider, shapeSlider;
  juce::Label thresholdLabel, shapeLabel;
  juce::ToggleButton softClipButton{"Soft Clip"};

  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
      attachMaster, attachL, attachR, attachThresh, attachShape;

  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
      attachLink, attachMS, attachSoftClip, attachBypass;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};
}  // namespace audio_plugin