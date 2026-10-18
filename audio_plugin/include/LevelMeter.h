#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace audio_plugin {

class LevelMeter : public juce::Component, private juce::Timer {
public:
  LevelMeter() {
    startTimerHz(30);
    setOpaque(false);
  }

  void setLevels(float left, float right) {
    targetL = juce::jmax(0.0f, left);
    targetR = juce::jmax(0.0f, right);
  }

  // 切换表下方的通道名，如 "L"/"R" 或 "M"/"S"
  void setChannelNames(const juce::String& leftName,
                       const juce::String& rightName) {
    if (nameL != leftName || nameR != rightName) {
      nameL = leftName;
      nameR = rightName;
      repaint();
    }
  }

  void paint(juce::Graphics& g) override {
    auto bounds = getLocalBounds().toFloat();
    bounds.removeFromBottom(16.0f);  // 底部留 16px 画通道名

    const float gap = 4.0f;
    const float barW = (bounds.getWidth() - gap) / 2.0f;

    auto leftArea = bounds.removeFromLeft(barW);
    bounds.removeFromLeft(gap);
    auto rightArea = bounds.removeFromLeft(barW);

    drawBar(g, leftArea, displayL, peakL, nameL);
    drawBar(g, rightArea, displayR, peakR, nameR);
  }

  void resized() override {
    peakL = 0.0f;
    peakR = 0.0f;
    repaint();
  }

private:
  static constexpr float kMinDb = -60.0f;
  static constexpr float kMaxDb = 6.0f;

  static float normFromLinear(float level) {
    if (level <= 0.0f)
      return 0.0f;
    const float db = juce::Decibels::gainToDecibels(level, kMinDb);
    return juce::jlimit(0.0f, 1.0f, juce::jmap(db, kMinDb, kMaxDb, 0.0f, 1.0f));
  }

  static float yForDb(const juce::Rectangle<float>& r, float db) {
    const float t = juce::jmap(db, kMinDb, kMaxDb, 0.0f, 1.0f);
    return r.getBottom() - r.getHeight() * t;
  }

  void drawBar(juce::Graphics& g,
               juce::Rectangle<float> r,
               float levelLinear,
               float peakLinear,
               const juce::String& name) {
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.fillRoundedRectangle(r, 2.0f);

    g.setColour(juce::Colours::white.withAlpha(0.25f));
    g.drawRoundedRectangle(r.reduced(0.5f), 2.0f, 1.0f);

    const float norm = normFromLinear(levelLinear);
    if (norm > 0.0f) {
      auto fillHeight = r.getHeight() * norm;
      auto fill = r.withTop(r.getBottom() - fillHeight);
      juce::ColourGradient grad(juce::Colours::green, r.getX(), r.getBottom(),
                                juce::Colours::red, r.getX(), r.getY(), false);
      grad.addColour(0.7, juce::Colours::yellow);
      g.setGradientFill(grad);
      g.fillRect(fill);
    }

    const float peakNorm = normFromLinear(peakLinear);
    if (peakNorm > 0.0f) {
      const float y = r.getBottom() - r.getHeight() * peakNorm;
      g.setColour(juce::Colours::white.withAlpha(0.85f));
      g.fillRect(r.getX(), y, r.getWidth(), 1.5f);
    }

    struct Mark {
      float db;
      const char* text;
    };
    static const Mark marks[] = {
        {+3.0f, "+3"},   {-3.0f, "-3"},   {-6.0f, "-6"},  {-12.0f, "-12"},
        {-24.0f, "-24"}, {-36.0f, "-36"}, {-48.0f, "-48"}};

    for (auto& m : marks) {
      const float y = yForDb(r, m.db);
      g.setColour(juce::Colours::white.withAlpha(0.18f));
      g.fillRect(r.getX(), y, r.getWidth(), 0.5f);
      g.setColour(juce::Colours::white.withAlpha(0.55f));
      g.setFont(9.0f);
      g.drawText(m.text,
                 juce::Rectangle<float>(r.getX(), y - 5.0f, r.getWidth() - 2.0f,
                                        10.0f),
                 juce::Justification::centredRight, false);
    }

    // 0 dB 线
    {
      const float y = yForDb(r, 0.0f);
      g.setColour(juce::Colours::orangered.withAlpha(0.35f));
      g.fillRect(r.getX(), y - 2.0f, r.getWidth(), 4.0f);
      g.setColour(juce::Colours::orangered);
      g.fillRect(r.getX(), y - 1.0f, r.getWidth(), 2.0f);
      g.setColour(juce::Colours::orange);
      g.fillRect(r.getX(), y - 2.5f, 2.5f, 5.0f);
      g.fillRect(r.getRight() - 2.5f, y - 2.5f, 2.5f, 5.0f);
      g.setFont(10.0f);
      g.drawText("0",
                 juce::Rectangle<float>(r.getX(), y - 6.0f, r.getWidth() - 2.0f,
                                        12.0f),
                 juce::Justification::centredRight, false);
    }

    // 表下方通道名（L/R 或 M/S），随模式切换
    if (name.isNotEmpty()) {
      g.setColour(juce::Colours::white.withAlpha(0.85f));
      g.setFont(11.0f);
      g.drawText(name,
                 juce::Rectangle<float>(r.getX(), r.getBottom() + 2.0f,
                                        r.getWidth(), 14.0f),
                 juce::Justification::centred, false);
    }
  }

  void timerCallback() override {
    auto smooth = [](float& display, float target) {
      if (target > display)
        display = target;
      else
        display = display * 0.82f + target * 0.18f;
    };
    smooth(displayL, targetL);
    smooth(displayR, targetR);

    auto updatePeak = [](float& peak, float level, float decay) {
      peak = juce::jmax(level, peak - decay);
    };
    updatePeak(peakL, displayL, 0.01f);
    updatePeak(peakR, displayR, 0.01f);

    if (displayL < 1.0e-5f)
      displayL = 0.0f;
    if (displayR < 1.0e-5f)
      displayR = 0.0f;
    repaint();
  }

  float targetL = 0.0f, targetR = 0.0f;
  float displayL = 0.0f, displayR = 0.0f;
  float peakL = 0.0f, peakR = 0.0f;
  juce::String nameL{"L"}, nameR{"R"};
};

}  // namespace audio_plugin