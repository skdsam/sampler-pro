#pragma once

#include "AudioEngine.h"
#include "MidiSequencerComponent.h"
#include "WaveformComponent.h"
#include <JuceHeader.h>
#include <memory>

class MainComponent : public juce::AudioAppComponent,
                      public juce::FileDragAndDropTarget,
                      public juce::ChangeListener,
                      public juce::Timer {
public:
  MainComponent();
  ~MainComponent() override;

  // AudioAppComponent
  void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
  void
  getNextAudioBlock(const juce::AudioSourceChannelInfo &bufferToFill) override;
  void releaseResources() override;

  void paint(juce::Graphics &) override;
  void resized() override;
  bool keyPressed(const juce::KeyPress &key) override;

  // FileDragAndDropTarget
  bool isInterestedInFileDrag(const juce::StringArray &files) override;
  void filesDropped(const juce::StringArray &files, int x, int y) override;

  // ChangeListener
  void changeListenerCallback(juce::ChangeBroadcaster *source) override;

  // Timer
  void timerCallback() override;

private:
  AudioEngine audioEngine;
  WaveformComponent waveformComponent;

  juce::TextButton openButton{"LOAD"};
  juce::TextButton playButton{"PLAY"};
  juce::TextButton stopButton{"STOP"};
  juce::TextButton exportMidiButton{"MIDI"};
  juce::TextButton exportSlicesButton{"SLICES"};

  juce::Slider tempoSlider;
  juce::Slider zoomSlider;
  juce::Label zoomLabel{"zoom", "ZOOM"};
  juce::ToggleButton sequencerToggle{"Sequencer Mode"};
  juce::ToggleButton loopToggle{"Loop"};
  MidiSequencerComponent sequencerComponent;
  juce::Label tempoLabel{"Tempo:", "Tempo:"};

  juce::Label statusLabel;

  std::unique_ptr<juce::FileChooser> fileChooser;

  // Custom native aesthetic colors
  const juce::Colour darkHeaderColor = juce::Colour::greyLevel(0.1f);
  const juce::Colour darkBgColor = juce::Colour::greyLevel(0.15f);
  const juce::Colour accentColor = juce::Colours::lightgreen.withAlpha(0.8f);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
