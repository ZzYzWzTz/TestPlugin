#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

namespace audio_plugin {
class PluginProcessor : public juce::AudioProcessor {
public:
  PluginProcessor();
  ~PluginProcessor() override;

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override;
  bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
  void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
  using AudioProcessor::processBlock;

  juce::AudioProcessorEditor* createEditor() override;
  bool hasEditor() const override;
  const juce::String getName() const override;
  bool acceptsMidi() const override;
  bool producesMidi() const override;
  bool isMidiEffect() const override;
  double getTailLengthSeconds() const override;
  int getNumPrograms() override;
  int getCurrentProgram() override;
  void setCurrentProgram(int index) override;
  const juce::String getProgramName(int index) override;
  void changeProgramName(int index, const juce::String& newName) override;
  void getStateInformation(juce::MemoryBlock& destData) override;
  void setStateInformation(const void* data, int sizeInBytes) override;

  // 宿主 bypass 桥接：让 DAW 的 bypass 按钮和插件内按钮用同一个参数
  juce::AudioProcessorParameter* getBypassParameter() const override;

  juce::AudioProcessorValueTreeState apvts;

  std::atomic<float> leftLevel{0.0f};
  std::atomic<float> rightLevel{0.0f};

private:
  static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

  juce::AudioParameterFloat* gainMaster = nullptr;
  juce::AudioParameterFloat* gainL = nullptr;
  juce::AudioParameterFloat* gainR = nullptr;
  juce::AudioParameterFloat* threshold = nullptr;
  juce::AudioParameterFloat* shape = nullptr;
  juce::AudioParameterBool* softClipOn = nullptr;
  juce::AudioParameterBool* linkedParam = nullptr;
  juce::AudioParameterBool* midSideParam = nullptr;
  juce::AudioParameterBool* bypassParam = nullptr;

  float previousGainMaster = 1.0f;
  float previousGainL = 1.0f;
  float previousGainR = 1.0f;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
}  // namespace audio_plugin