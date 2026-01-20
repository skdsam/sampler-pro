#pragma once

#include "AudioAnalysis.h"
#include <JuceHeader.h>
#include <memory>

class AudioEngine : public juce::AudioProcessor,
                    public juce::Thread,
                    public juce::ChangeBroadcaster {
public:
  AudioEngine();
  ~AudioEngine() override;

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override;
  void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

  void loadFile(const juce::File &file);
  void play();
  void stop();
  void playSlice(int sliceIndex);
  void setPosition(double seconds) { transportSource.setPosition(seconds); }

  bool isPlaying() const { return transportSource.isPlaying(); }
  double getCurrentPosition() const {
    return transportSource.getCurrentPosition();
  }
  double getLengthInSeconds() const {
    return transportSource.getLengthInSeconds();
  }

  juce::AudioThumbnail &getThumbnail() { return thumbnail; }

  const AudioAnalysis::AnalysisResults &getAnalysis() const {
    return analysisResults;
  }
  AudioAnalysis::AnalysisResults &getAnalysis() { return analysisResults; }
  void runAnalysis();
  double getFileSampleRate() const { return fileSampleRate; }

  void run() override; // Thread run method
  bool isProcessing() const { return threadShouldExit() || isThreadRunning(); }

  void exportSlices(const juce::File &directory);
  void exportMidi(const juce::File &file);

  void setTempo(double newBpm) { targetBPM = newBpm; }
  double getTempo() const {
    return targetBPM > 0 ? targetBPM : analysisResults.bpm;
  }
  void setLooping(bool shouldLoop) { memorySource->setLooping(shouldLoop); }

  juce::AudioProcessorEditor *createEditor() override { return nullptr; }
  bool hasEditor() const override { return false; }

  const juce::String getName() const override { return "AudioEngine"; }
  bool acceptsMidi() const override { return true; }
  bool producesMidi() const override { return true; }
  bool isMidiEffect() const override { return false; }
  double getTailLengthSeconds() const override { return 0.0; }

  int getNumPrograms() override { return 1; }
  int getCurrentProgram() override { return 0; }
  void setCurrentProgram(int index) override {}
  const juce::String getProgramName(int index) override { return {}; }
  void changeProgramName(int index, const juce::String &newName) override {}

  void getStateInformation(juce::MemoryBlock &destData) override {}
  void setStateInformation(const void *data, int sizeInBytes) override {}

  // Sequencer - Simplified API
  void setSequencerEnabled(bool enabled);
  bool isSequencerActive() const { return sequencerEnabled; }
  void
  setSequenceStep(int step,
                  int sliceIndex); // Set which slice plays on step (-1 = none)
  int getSequenceStep(int step) const;
  void clearSequence();
  int getCurrentStep() const { return currentStep; }
  int getNumSlices() const { return (int)analysisResults.onsets.size(); }

private:
  void updateSequencerLogic(int numSamples);

  juce::AudioFormatManager formatManager;
  std::unique_ptr<juce::MemoryAudioSource> memorySource;
  juce::AudioTransportSource transportSource;

  juce::AudioThumbnailCache thumbnailCache{5};
  juce::AudioThumbnail thumbnail{512, formatManager, thumbnailCache};

  AudioAnalysis::AnalysisResults analysisResults;
  juce::AudioBuffer<float> loadedBuffer;
  double fileSampleRate = 44100.0;
  double currentSampleRate = 44100.0;
  double stopAtPosition = -1.0;

  double targetBPM = 120.0;

  // Sequencer State
  bool sequencerEnabled = false;
  int currentStep = 0;
  double samplesPerStep = 0.0;
  double stepAccumulator = 0.0;
  int sequencePattern[32] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                             -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                             -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

  // Multi-voice sampler
  struct SamplerVoice {
    int startSample = 0;
    int currentSample = 0;
    int endSample = 0;
    bool isActive = false;
  };
  static constexpr int NUM_VOICES = 8;
  SamplerVoice voices[NUM_VOICES];
  void triggerVoice(int sliceIndex);
  void processVoices(juce::AudioBuffer<float> &buffer);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
