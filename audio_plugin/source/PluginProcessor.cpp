namespace audio_plugin {

namespace {
inline float softClip(float x, float thresh, float shape) {
  if (thresh <= 0.0f)
    return 0.0f;
  const float ax = std::abs(x);
  if (ax <= thresh)
    return x;
  const float sign = x < 0.0f ? -1.0f : 1.0f;
  const float over = ax - thresh;
  const float headroom = 1.0f - thresh;
  if (headroom <= 1.0e-6f)
    return sign * thresh;
  const float k = juce::jmap(shape, 0.5f, 8.0f);
  const float t = std::tanh(k * over / headroom) / std::tanh(k);
  return sign * (thresh + headroom * t);
}
}  // namespace

juce::AudioProcessorValueTreeState::ParameterLayout
PluginProcessor::createLayout() {
  juce::AudioProcessorValueTreeState::ParameterLayout layout;
  auto makeRange = [] {
    juce::NormalisableRange<float> r(0.0f,
                                     juce::Decibels::decibelsToGain(6.0f));
    r.setSkewForCentre(juce::Decibels::decibelsToGain(-12.0f));
    return r;
  };

  juce::AudioParameterFloatAttributes attributes;
  attributes =
      attributes
          .withStringFromValueFunction([](float value, int) {
            if (value <= 0.0f)
              return juce::String("-inf dB");
            return juce::String(juce::Decibels::gainToDecibels(value), 1) +
                   " dB";
          })
          .withValueFromStringFunction([](const juce::String& text) {
            if (text.containsIgnoreCase("inf"))
              return 0.0f;
            return juce::Decibels::decibelsToGain(text.getFloatValue());
          });

  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"gainMaster", 1}, "Master", makeRange(), 1.0f,
      attributes));
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"gainL", 1}, "Gain L", makeRange(), 1.0f, attributes));
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"gainR", 1}, "Gain R", makeRange(), 1.0f, attributes));

  juce::NormalisableRange<float> threshRange(
      juce::Decibels::decibelsToGain(-24.0f),
      juce::Decibels::decibelsToGain(0.0f));
  threshRange.setSkewForCentre(juce::Decibels::decibelsToGain(-12.0f));

  juce::AudioParameterFloatAttributes threshAttrs;
  threshAttrs =
      threshAttrs
          .withStringFromValueFunction([](float v, int) {
            if (v <= 0.0f)
              return juce::String("-inf dB");
            return juce::String(juce::Decibels::gainToDecibels(v), 1) + " dB";
          })
          .withValueFromStringFunction([](const juce::String& t) {
            if (t.containsIgnoreCase("inf"))
              return 0.0f;
            return juce::Decibels::decibelsToGain(t.getFloatValue());
          });

  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"threshold", 1}, "Threshold", threshRange,
      juce::Decibels::decibelsToGain(-6.0f), threshAttrs));

  juce::AudioParameterFloatAttributes shapeAttrs;
  shapeAttrs = shapeAttrs.withStringFromValueFunction(
      [](float v, int) { return juce::String(v * 100.0f, 0) + "%"; });
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"shape", 1}, "Shape",
      juce::NormalisableRange<float>(0.0f, 1.0f), 0.3f, shapeAttrs));

  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"softClipOn", 1}, "Soft Clip", false));
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"linked", 1}, "Link", true));
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"midSideMode", 1}, "M/S", false));

  // 新增：Bypass 参数
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"bypass", 1}, "Bypass", false));

  return layout;
}

PluginProcessor::PluginProcessor()
    : AudioProcessor(
          BusesProperties()
#if !JUCE_IS_MIDI_EFFECT
#if !JUCE_IS_SYNTH
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
              ),
      apvts(*this, nullptr, "Parameters", createLayout()) {
  gainMaster = dynamic_cast<juce::AudioParameterFloat*>(
      apvts.getParameter("gainMaster"));
  gainL = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("gainL"));
  gainR = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("gainR"));
  threshold =
      dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("threshold"));
  shape = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("shape"));
  softClipOn =
      dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("softClipOn"));
  linkedParam =
      dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("linked"));
  midSideParam = dynamic_cast<juce::AudioParameterBool*>(
      apvts.getParameter("midSideMode"));
  bypassParam =
      dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("bypass"));

  jassert(gainMaster && gainL && gainR && threshold && shape && softClipOn &&
          linkedParam && midSideParam && bypassParam);

  previousGainMaster = gainMaster->get();
  previousGainL = gainL->get();
  previousGainR = gainR->get();
}

PluginProcessor::~PluginProcessor() = default;

// 宿主 bypass 桥接：让 DAW 的 bypass 按钮绑定到我们的 bypass 参数
juce::AudioProcessorParameter* PluginProcessor::getBypassParameter() const {
  return bypassParam;
}

const juce::String PluginProcessor::getName() const {
  return JUCE_PLUGIN_NAME;
}
bool PluginProcessor::acceptsMidi() const {
  return false;
}
bool PluginProcessor::producesMidi() const {
  return false;
}
bool PluginProcessor::isMidiEffect() const {
  return false;
}
double PluginProcessor::getTailLengthSeconds() const {
  return 0.0;
}
int PluginProcessor::getNumPrograms() {
  return 1;
}
int PluginProcessor::getCurrentProgram() {
  return 0;
}
void PluginProcessor::setCurrentProgram(int index) {
  juce::ignoreUnused(index);
}
const juce::String PluginProcessor::getProgramName(int index) {
  juce::ignoreUnused(index);
  return {};
}
void PluginProcessor::changeProgramName(int index,
                                        const juce::String& newName) {
  juce::ignoreUnused(index, newName);
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
  juce::ignoreUnused(sampleRate, samplesPerBlock);
  previousGainMaster = gainMaster->get();
  previousGainL = gainL->get();
  previousGainR = gainR->get();
}

void PluginProcessor::releaseResources() {}

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
  if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() &&
      layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
    return false;
  if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
    return false;
  return true;
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                   juce::MidiBuffer& midiMessages) {
  juce::ignoreUnused(midiMessages);
  juce::ScopedNoDenormals noDenormals;

  const int numSamples = buffer.getNumSamples();
  if (numSamples == 0)
    return;

  // Bypass：直接透传，只清电平表（不动 previousGain*，解除时自然 ramp 无爆音）
  if (bypassParam != nullptr && bypassParam->get()) {
    leftLevel.store(0.0f, std::memory_order_relaxed);
    rightLevel.store(0.0f, std::memory_order_relaxed);
    return;
  }

  auto totalNumInputChannels = getTotalNumInputChannels();
  auto totalNumOutputChannels = getTotalNumOutputChannels();
  for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
    buffer.clear(i, 0, numSamples);

  const bool isLinked = linkedParam->get();
  const bool isMS = midSideParam->get();

  const float master = gainMaster->get();
  const float gL = gainL->get();
  const float gR = gainR->get();

  const float targetL = isLinked ? master : gL;
  const float targetR = isLinked ? master : gR;

  if (buffer.getNumChannels() >= 2) {
    auto* L = buffer.getWritePointer(0);
    auto* R = buffer.getWritePointer(1);

    if (isMS) {
      for (int i = 0; i < numSamples; ++i) {
        const float l = L[i], r = R[i];
        L[i] = 0.5f * (l + r);
        R[i] = 0.5f * (l - r);
      }
      buffer.applyGainRamp(0, 0, numSamples, previousGainL, targetL);
      buffer.applyGainRamp(1, 0, numSamples, previousGainR, targetR);
      for (int i = 0; i < numSamples; ++i) {
        const float m = L[i], s = R[i];
        L[i] = m + s;
        R[i] = m - s;
      }
    } else {
      buffer.applyGainRamp(0, 0, numSamples, previousGainL, targetL);
      buffer.applyGainRamp(1, 0, numSamples, previousGainR, targetR);
    }
  } else if (buffer.getNumChannels() == 1) {
    const float target = isLinked ? master : gL;
    buffer.applyGainRamp(0, 0, numSamples, previousGainL, target);
  }

  previousGainMaster = master;
  previousGainL = targetL;
  previousGainR = targetR;

  if (softClipOn->get()) {
    const float thresh = threshold->get();
    const float sh = shape->get();
    if (buffer.getNumChannels() >= 2) {
      auto* L = buffer.getWritePointer(0);
      auto* R = buffer.getWritePointer(1);
      for (int i = 0; i < numSamples; ++i) {
        L[i] = softClip(L[i], thresh, sh);
        R[i] = softClip(R[i], thresh, sh);
      }
    } else if (buffer.getNumChannels() == 1) {
      auto* L = buffer.getWritePointer(0);
      for (int i = 0; i < numSamples; ++i)
        L[i] = softClip(L[i], thresh, sh);
    }
  }

  float lPeak = 0.0f, rPeak = 0.0f;
  if (buffer.getNumChannels() > 0) {
    const float* L = buffer.getReadPointer(0);
    for (int i = 0; i < numSamples; ++i)
      lPeak = juce::jmax(lPeak, std::abs(L[i]));
  }
  if (buffer.getNumChannels() > 1) {
    const float* R = buffer.getReadPointer(1);
    for (int i = 0; i < numSamples; ++i)
      rPeak = juce::jmax(rPeak, std::abs(R[i]));
  } else {
    rPeak = lPeak;
  }

  if (isMS && buffer.getNumChannels() >= 2) {
    const float* L = buffer.getReadPointer(0);
    const float* R = buffer.getReadPointer(1);
    float mPeak = 0.0f, sPeak = 0.0f;
    for (int i = 0; i < numSamples; ++i) {
      const float m = 0.5f * (L[i] + R[i]);
      const float s = 0.5f * (L[i] - R[i]);
      mPeak = juce::jmax(mPeak, std::abs(m));
      sPeak = juce::jmax(sPeak, std::abs(s));
    }
    leftLevel.store(mPeak, std::memory_order_relaxed);
    rightLevel.store(sPeak, std::memory_order_relaxed);
  } else {
    leftLevel.store(lPeak, std::memory_order_relaxed);
    rightLevel.store(rPeak, std::memory_order_relaxed);
  }
}

bool PluginProcessor::hasEditor() const {
  return true;
}

void PluginProcessor::getStateInformation(juce::MemoryBlock& destData) {
  auto state = apvts.copyState();
  if (auto xml = state.createXml())
    copyXmlToBinary(*xml, destData);
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes) {
  std::unique_ptr<juce::XmlElement> xmlState(
      getXmlFromBinary(data, sizeInBytes));
  if (xmlState == nullptr)
    return;
  auto tree = juce::ValueTree::fromXml(*xmlState);
  if (tree.isValid() && tree.hasType(apvts.state.getType()))
    apvts.replaceState(tree);

  previousGainMaster = gainMaster->get();
  previousGainL = gainL->get();
  previousGainR = gainR->get();
}

}  // namespace audio_plugin

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
  return new audio_plugin::PluginProcessor();
}