#pragma once

#include <JuceHeader.h>
#include <functional>
#include <vector>

class WaveformComponent : public juce::Component,
                          public juce::ChangeListener,
                          public juce::Timer {
public:
  WaveformComponent(juce::AudioThumbnail &thumbnailToUse,
                    std::vector<int> *onsetsToUse)
      : thumbnail(thumbnailToUse), onsets(onsetsToUse) {
    thumbnail.addChangeListener(this);
    startTimerHz(60);
  }

  WaveformComponent() : thumbnail(dummyThumbnail), onsets(&dummyOnsets) {
    startTimerHz(60);
  }

  void setOnsets(std::vector<int> *newOnsets) {
    onsets = newOnsets;
    repaint();
  }
  std::function<void(int)> onSliceClicked;

  void setSampleRate(double newSampleRate) { sampleRate = newSampleRate; }
  void setPlayheadTime(double time) {
    playheadTime = time;
    repaint();
  }
  void setZoomLevel(double newZoom) {
    zoomLevel = juce::jlimit(1.0, 100.0, newZoom);
    repaint();
  }
  double getZoomLevel() const { return zoomLevel; }

  ~WaveformComponent() override { thumbnail.removeChangeListener(this); }

  void paint(juce::Graphics &g) override {
    auto bounds = getLocalBounds();

    g.setColour(juce::Colour::greyLevel(0.1f));
    g.fillRoundedRectangle(bounds.toFloat(), 4.0f);

    if (thumbnail.getNumChannels() == 0) {
      g.setColour(juce::Colours::white.withAlpha(0.3f));
      g.drawFittedText("Drop a sample here", bounds,
                       juce::Justification::centred, 1);
    } else {
      double totalDuration = thumbnail.getTotalLength();
      double displayedDuration = totalDuration / zoomLevel;
      double startTime = scrollPos * (totalDuration - displayedDuration);
      double endTime = startTime + displayedDuration;

      g.setColour(juce::Colours::lightgreen.withAlpha(0.8f));
      thumbnail.drawChannels(g, bounds.reduced(2), startTime, endTime, 1.0f);

      g.setColour(juce::Colours::white.withAlpha(0.2f));

      if (onsets != nullptr) {
        for (int onsetSample : *onsets) {
          double onsetTime =
              (double)onsetSample / (sampleRate > 0 ? sampleRate : 44100.0);
          if (onsetTime >= startTime && onsetTime <= endTime) {
            float x = (float)((onsetTime - startTime) / displayedDuration *
                              bounds.getWidth());
            g.drawVerticalLine((int)x, (float)bounds.getY(),
                               (float)bounds.getBottom());
          }
        }
      }

      // Draw Playhead
      if (playheadTime >= startTime && playheadTime <= endTime) {
        g.setColour(juce::Colours::red);
        float playheadX = (float)((playheadTime - startTime) /
                                  displayedDuration * bounds.getWidth());
        g.drawVerticalLine((int)playheadX, (float)bounds.getY(),
                           (float)bounds.getBottom());
      }
    }
  }

  void mouseDown(const juce::MouseEvent &event) override {
    auto bounds = getLocalBounds();
    double totalDuration = thumbnail.getTotalLength();
    if (totalDuration <= 0)
      return;

    double displayedDuration = totalDuration / zoomLevel;
    double startTime = scrollPos * (totalDuration - displayedDuration);
    float clickX = (float)event.x;
    double clickTime =
        startTime + (clickX / bounds.getWidth()) * displayedDuration;
    int clickSample =
        (int)(clickTime * (sampleRate > 0 ? sampleRate : 44100.0));

    // Check if we are clicking near an onset to drag
    draggingOnsetIndex = -1;
    if (onsets != nullptr) {
      const float dragToleranceSamples =
          (float)(0.02 *
                  (sampleRate > 0 ? sampleRate : 44100.0)); // 20ms tolerance

      for (int i = 0; i < (int)onsets->size(); ++i) {
        if (std::abs((*onsets)[i] - clickSample) < dragToleranceSamples) {
          draggingOnsetIndex = i;
          return;
        }
      }
    }

    if (onSliceClicked == nullptr || onsets == nullptr || onsets->empty())
      return;

    // Find the slice index that starts before or at clickSample
    int sliceIndex = -1;
    for (int i = 0; i < (int)onsets->size(); ++i) {
      if ((*onsets)[i] <= clickSample) {
        sliceIndex = i;
      } else {
        break;
      }
    }

    if (sliceIndex != -1)
      onSliceClicked(sliceIndex);
  }

  void mouseDrag(const juce::MouseEvent &event) override {
    if (draggingOnsetIndex == -1)
      return;

    auto bounds = getLocalBounds();
    double totalDuration = thumbnail.getTotalLength();
    double displayedDuration = totalDuration / zoomLevel;
    double startTime = scrollPos * (totalDuration - displayedDuration);

    float dragX = (float)event.x;
    double dragTime =
        startTime + (dragX / bounds.getWidth()) * displayedDuration;
    int dragSample = (int)(dragTime * (sampleRate > 0 ? sampleRate : 44100.0));

    if (onsets != nullptr) {
      (*onsets)[draggingOnsetIndex] = juce::jlimit(
          0, (int)thumbnail.getTotalLength() * (int)sampleRate, dragSample);
      repaint();
    }
  }

  void mouseUp(const juce::MouseEvent &) override { draggingOnsetIndex = -1; }

  std::function<void()> onZoomChanged;

  void mouseWheelMove(const juce::MouseEvent &,
                      const juce::MouseWheelDetails &wheel) override {
    if (thumbnail.getTotalLength() <= 0)
      return;

    if (wheel.deltaY != 0) {
      zoomLevel = juce::jlimit(1.0, 100.0, zoomLevel + wheel.deltaY * 5.0);
      if (onZoomChanged)
        onZoomChanged();
    }

    if (wheel.deltaX != 0) {
      scrollPos = juce::jlimit(0.0, 1.0, scrollPos - wheel.deltaX * 0.1);
    }

    repaint();
  }

  void changeListenerCallback(juce::ChangeBroadcaster *source) override {
    if (source == &thumbnail)
      repaint();
  }

  void timerCallback() override {
    // Parent should call setPlayheadTime
  }

private:
  juce::AudioThumbnail &thumbnail;
  std::vector<int> *onsets;
  double sampleRate = 44100.0;
  double playheadTime = 0.0;
  double zoomLevel = 1.0;
  double scrollPos = 0.0; // 0.0 to 1.0
  int draggingOnsetIndex = -1;

  // Dummies for default constructor
  juce::AudioFormatManager dummyManager;
  juce::AudioThumbnailCache dummyCache{1};
  juce::AudioThumbnail dummyThumbnail{1, dummyManager, dummyCache};
  std::vector<int> dummyOnsets;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformComponent)
};
