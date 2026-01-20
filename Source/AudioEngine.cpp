#include "AudioEngine.h"

AudioEngine::AudioEngine()
    : juce::AudioProcessor(
          BusesProperties()
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      juce::Thread("AnalysisThread") {
  formatManager.registerBasicFormats();
}

AudioEngine::~AudioEngine() { stopThread(4000); }

void AudioEngine::prepareToPlay(double sampleRate, int samplesPerBlock) {
  transportSource.prepareToPlay(samplesPerBlock, sampleRate);
  currentSampleRate = sampleRate; // [NEW]
}

void AudioEngine::releaseResources() { transportSource.releaseResources(); }

void AudioEngine::processBlock(juce::AudioBuffer<float> &buffer,
                               juce::MidiBuffer &midiMessages) {
  buffer.clear();

  updateSequencerLogic(buffer.getNumSamples());
  processVoices(buffer); // Mix active voices into buffer

  // Also mix in transport source for main playback (Play button)
  if (transportSource.isPlaying()) {
    juce::AudioBuffer<float> transportBuffer(buffer.getNumChannels(),
                                             buffer.getNumSamples());
    transportBuffer.clear();
    transportSource.getNextAudioBlock(
        juce::AudioSourceChannelInfo(transportBuffer));
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
      buffer.addFrom(ch, 0, transportBuffer, ch, 0, buffer.getNumSamples());
    }
  }
}

void AudioEngine::loadFile(const juce::File &file) {
  transportSource.stop();
  transportSource.setSource(nullptr);
  memorySource.reset();

  if (auto *reader = formatManager.createReaderFor(file)) {
    fileSampleRate = reader->sampleRate;

    // Read into buffer for analysis and playback
    loadedBuffer.setSize((int)reader->numChannels,
                         (int)reader->lengthInSamples);
    reader->read(&loadedBuffer, 0, (int)reader->lengthInSamples, 0, true, true);

    // Create MemoryAudioSource from loadedBuffer
    memorySource =
        std::make_unique<juce::MemoryAudioSource>(loadedBuffer, true, false);
    transportSource.setSource(memorySource.get(), 0, nullptr, fileSampleRate);

    thumbnail.setSource(new juce::FileInputSource(file));

    runAnalysis();
  }
}

void AudioEngine::runAnalysis() {
  if (isThreadRunning())
    stopThread(2000);

  startThread();
}

void AudioEngine::run() {
  if (loadedBuffer.getNumSamples() > 0) {
    analysisResults = AudioAnalysis::analyze(loadedBuffer, fileSampleRate);
    sendChangeMessage();
  }
}

void AudioEngine::play() { transportSource.start(); }

void AudioEngine::stop() { transportSource.stop(); }

void AudioEngine::playSlice(int sliceIndex) {
  if (analysisResults.onsets.empty())
    return;

  int startSample = 0;
  if (sliceIndex >= 0 && sliceIndex < analysisResults.onsets.size()) {
    startSample = analysisResults.onsets[sliceIndex];

    int endSample = loadedBuffer.getNumSamples();
    // Stop at next onset if available
    if (sliceIndex + 1 < analysisResults.onsets.size()) {
      endSample = analysisResults.onsets[sliceIndex + 1];
    }

    stopAtPosition = (double)endSample;
  }

  transportSource.setPosition((double)startSample / fileSampleRate);
  transportSource.start();
}

void AudioEngine::setSequencerEnabled(bool enabled) {
  sequencerEnabled = enabled;
  if (!enabled) {
    currentStep = 0;
    stepAccumulator = 0.0;
    // Stop all voices
    for (auto &voice : voices) {
      voice.isActive = false;
    }
  }
}

void AudioEngine::setSequenceStep(int step, int sliceIndex) {
  if (step >= 0 && step < 32) {
    sequencePattern[step] = sliceIndex;
  }
}

int AudioEngine::getSequenceStep(int step) const {
  if (step >= 0 && step < 32) {
    return sequencePattern[step];
  }
  return -1;
}

void AudioEngine::clearSequence() {
  for (int i = 0; i < 32; ++i) {
    sequencePattern[i] = -1;
  }
}

void AudioEngine::triggerVoice(int sliceIndex) {
  if (sliceIndex < 0 || sliceIndex >= (int)analysisResults.onsets.size())
    return;

  int startSample = analysisResults.onsets[sliceIndex];
  int endSample = loadedBuffer.getNumSamples();
  if (sliceIndex + 1 < (int)analysisResults.onsets.size()) {
    endSample = analysisResults.onsets[sliceIndex + 1];
  }

  // Find a free voice
  for (auto &voice : voices) {
    if (!voice.isActive) {
      voice.startSample = startSample;
      voice.currentSample = startSample;
      voice.endSample = endSample;
      voice.isActive = true;
      return;
    }
  }
  // No free voice - steal the oldest (first one)
  voices[0].startSample = startSample;
  voices[0].currentSample = startSample;
  voices[0].endSample = endSample;
  voices[0].isActive = true;
}

void AudioEngine::processVoices(juce::AudioBuffer<float> &buffer) {
  if (loadedBuffer.getNumSamples() == 0)
    return;

  int numChannels =
      juce::jmin(buffer.getNumChannels(), loadedBuffer.getNumChannels());
  int numSamples = buffer.getNumSamples();

  for (auto &voice : voices) {
    if (!voice.isActive)
      continue;

    for (int i = 0; i < numSamples; ++i) {
      if (voice.currentSample >= voice.endSample) {
        voice.isActive = false;
        break;
      }

      for (int ch = 0; ch < numChannels; ++ch) {
        buffer.addSample(ch, i,
                         loadedBuffer.getSample(ch, voice.currentSample));
      }
      voice.currentSample++;
    }
  }
}

void AudioEngine::updateSequencerLogic(int numSamples) {
  if (!sequencerEnabled || targetBPM <= 0.0 || currentSampleRate <= 0.0)
    return;

  // 16th notes: (60 / BPM) / 4 seconds per step
  samplesPerStep = (60.0 / targetBPM / 4.0) * currentSampleRate;

  if (samplesPerStep <= 0.0)
    return;

  stepAccumulator += numSamples;

  while (stepAccumulator >= samplesPerStep) {
    stepAccumulator -= samplesPerStep;
    currentStep = (currentStep + 1) % 32;

    // Trigger slice for this step
    int sliceToPlay = sequencePattern[currentStep];
    if (sliceToPlay >= 0) {
      triggerVoice(sliceToPlay);
    }

    sendChangeMessage(); // Notify UI of step change
  }
}

void AudioEngine::exportSlices(const juce::File &directory) {
  if (loadedBuffer.getNumSamples() == 0 || analysisResults.onsets.empty())
    return;

  juce::WavAudioFormat wavFormat;

  for (size_t i = 0; i < analysisResults.onsets.size(); ++i) {
    int startSample = analysisResults.onsets[i];
    int endSample = (i + 1 < analysisResults.onsets.size())
                        ? analysisResults.onsets[i + 1]
                        : loadedBuffer.getNumSamples();

    int numSamples = endSample - startSample;
    if (numSamples <= 0)
      continue;

    juce::File sliceFile =
        directory.getChildFile("Slice_" + juce::String(i + 1) + ".wav");
    sliceFile.deleteFile();

    auto outputStream = std::make_unique<juce::FileOutputStream>(sliceFile);
    if (outputStream->openedOk()) {
      if (auto writer = std::unique_ptr<juce::AudioFormatWriter>(
              wavFormat.createWriterFor(
                  outputStream.release(), fileSampleRate,
                  (unsigned int)loadedBuffer.getNumChannels(), 16, {}, 0))) {
        writer->writeFromAudioSampleBuffer(loadedBuffer, startSample,
                                           numSamples);
      }
    }
  }
}

void AudioEngine::exportMidi(const juce::File &file) {
  if (analysisResults.onsets.empty())
    return;

  juce::MidiFile midiFile;
  juce::MidiMessageSequence sequence;

  double bpm = getTempo();
  double secondsPerSample = 1.0 / getSampleRate();

  for (size_t i = 0; i < analysisResults.onsets.size(); ++i) {
    double startTimeInSeconds = analysisResults.onsets[i] * secondsPerSample;
    int noteNumber =
        60 + (int)i; // Map slices to chromatic notes starting at C3

    sequence.addEvent(juce::MidiMessage::noteOn(1, noteNumber, 1.0f),
                      startTimeInSeconds);
    sequence.addEvent(juce::MidiMessage::noteOff(1, noteNumber, 1.0f),
                      startTimeInSeconds + 0.1); // Fixed short duration
  }

  midiFile.setTicksPerQuarterNote(960);
  midiFile.addTrack(sequence);

  if (auto out =
          std::unique_ptr<juce::FileOutputStream>(file.createOutputStream())) {
    midiFile.writeTo(*out);
  }
}
