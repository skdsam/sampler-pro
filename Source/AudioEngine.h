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

  void setTempo(double newBpm) { targetBpm = newBpm; }
  double getTempo() const {
    return targetBpm > 0 ? targetBpm : analysisResults.bpm;
  }

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

private:
  juce::AudioFormatManager formatManager;
  std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
  juce::AudioTransportSource transportSource;

  juce::AudioThumbnailCache thumbnailCache{5};
  juce::AudioThumbnail thumbnail{512, formatManager, thumbnailCache};

  AudioAnalysis::AnalysisResults analysisResults;
  juce::AudioBuffer<float> loadedBuffer;
  double targetBpm = 0.0;
  double fileSampleRate = 44100.0;
  double stopAtPosition = -1.0;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
