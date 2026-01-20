#pragma once

#include <JuceHeader.h>
#include <functional>

class MidiSequencerComponent : public juce::Component,
                               public juce::DragAndDropTarget {
public:
  static constexpr int NUM_STEPS = 32; // 2 bars

  MidiSequencerComponent() {
    for (int i = 0; i < NUM_STEPS; ++i)
      stepPattern[i] = -1;
  }

  void setNumSlices(int slices) {
    numSlices = slices;
    repaint();
  }

  void setCurrentStep(int step) {
    if (currentStep != step) {
      currentStep = step;
      repaint();
    }
  }

  std::function<void(int step, int sliceIndex)> onStepChanged;

  void setPattern(const int *pattern, int numSteps) {
    for (int i = 0; i < juce::jmin(numSteps, NUM_STEPS); ++i) {
      stepPattern[i] = pattern[i];
    }
    repaint();
  }

  // DragAndDropTarget implementation
  bool isInterestedInDragSource(const SourceDetails &) override { return true; }

  void itemDropped(const SourceDetails &dragSourceDetails) override {
    juce::String desc = dragSourceDetails.description.toString();
    if (desc.startsWith("Slice:")) {
      int sliceIndex = desc.substring(6).getIntValue();
      int step = getStepAt(dragSourceDetails.localPosition.x);
      if (step >= 0 && step < NUM_STEPS && sliceIndex >= 0 &&
          sliceIndex < numSlices) {
        stepPattern[step] = sliceIndex;
        if (onStepChanged)
          onStepChanged(step, sliceIndex);
        repaint();
      }
    }
  }

  void itemDragEnter(const SourceDetails &) override { repaint(); }
  void itemDragExit(const SourceDetails &) override { repaint(); }

  void paint(juce::Graphics &g) override {
    auto bounds = getLocalBounds();
    g.fillAll(juce::Colour(0xFF1A1A1A));

    if (numSlices == 0) {
      g.setColour(juce::Colours::grey);
      g.setFont(16.0f);
      g.drawFittedText("Load a sample to use the sequencer", bounds,
                       juce::Justification::centred, 1);
      return;
    }

    float padWidth = bounds.getWidth() / (float)NUM_STEPS;
    float padHeight =
        (float)bounds.getHeight() - 16.0f; // Leave room for beat markers

    for (int step = 0; step < NUM_STEPS; ++step) {
      auto padRect = juce::Rectangle<float>((float)step * padWidth, 0, padWidth,
                                            padHeight);
      auto innerRect = padRect.reduced(1);

      // Background - highlight current step and bar boundaries
      if (step == currentStep) {
        g.setColour(juce::Colour(0xFF4A4A7A));
      } else if (step % 16 == 0) {
        g.setColour(juce::Colour(0xFF2A3A2A)); // Bar start
      } else if (step % 4 == 0) {
        g.setColour(juce::Colour(0xFF2A2A2A)); // Beat start
      } else {
        g.setColour(juce::Colour(0xFF1E1E1E));
      }
      g.fillRoundedRectangle(innerRect, 2.0f);

      // Draw slice assignment
      int sliceIndex = stepPattern[step];
      if (sliceIndex >= 0) {
        g.setColour(juce::Colour(0xFFFF9900).withAlpha(0.85f));
        g.fillRoundedRectangle(innerRect.reduced(2), 2.0f);

        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(11.0f).boldened());
        g.drawFittedText(juce::String(sliceIndex + 1), innerRect.toNearestInt(),
                         juce::Justification::centred, 1);
      }

      // Beat marker at bottom
      if (step % 4 == 0) {
        g.setColour(juce::Colours::grey.withAlpha(0.6f));
        g.setFont(9.0f);
        int beatNum = (step / 4) + 1;
        g.drawText(juce::String(beatNum), (int)padRect.getX(), (int)padHeight,
                   (int)padWidth, 14, juce::Justification::centred);
      }
    }

    // Playhead line
    if (currentStep >= 0 && currentStep < NUM_STEPS) {
      float x = currentStep * padWidth + padWidth / 2;
      g.setColour(juce::Colours::white.withAlpha(0.7f));
      g.drawLine(x, 0, x, padHeight, 2.0f);
    }
  }

  void mouseDown(const juce::MouseEvent &event) override {
    if (numSlices == 0)
      return;

    int step = getStepAt(event.x);
    if (step < 0 || step >= NUM_STEPS)
      return;

    if (event.mods.isRightButtonDown()) {
      // Right-click clears the pad
      stepPattern[step] = -1;
      if (onStepChanged)
        onStepChanged(step, -1);
      repaint();
    } else {
      // Start drag tracking
      dragStartY = event.y;
      dragStartSlice = stepPattern[step];
      draggingStep = step;
    }
  }

  void mouseDrag(const juce::MouseEvent &event) override {
    if (draggingStep < 0 || numSlices == 0)
      return;

    // Vertical drag changes slice number
    int deltaY = dragStartY - event.y;
    int sliceChange = deltaY / 15; // 15 pixels per slice

    int newSlice = dragStartSlice + sliceChange;
    if (newSlice < -1)
      newSlice = -1;
    if (newSlice >= numSlices)
      newSlice = numSlices - 1;

    if (stepPattern[draggingStep] != newSlice) {
      stepPattern[draggingStep] = newSlice;
      if (onStepChanged)
        onStepChanged(draggingStep, newSlice);
      repaint();
    }
  }

  void mouseUp(const juce::MouseEvent &) override { draggingStep = -1; }

private:
  int getStepAt(float x) const {
    float padWidth = getWidth() / (float)NUM_STEPS;
    return (int)(x / padWidth);
  }

  int numSlices = 0;
  int currentStep = 0;
  int stepPattern[NUM_STEPS];

  // Drag state
  int draggingStep = -1;
  int dragStartY = 0;
  int dragStartSlice = -1;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiSequencerComponent)
};
