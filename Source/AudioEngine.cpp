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
}

void AudioEngine::releaseResources() { transportSource.releaseResources(); }

void AudioEngine::processBlock(juce::AudioBuffer<float> &buffer,
                               juce::MidiBuffer &midiMessages) {
  juce::ScopedNoDenormals noDenormals;
  auto totalNumInputChannels = getTotalNumInputChannels();
  auto totalNumOutputChannels = getTotalNumOutputChannels();

  for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
    buffer.clear(i, 0, buffer.getNumSamples());

  transportSource.getNextAudioBlock(juce::AudioSourceChannelInfo(buffer));

  if (stopAtPosition > 0 &&
      transportSource.getCurrentPosition() >= stopAtPosition) {
    transportSource.stop();
    stopAtPosition = -1.0;
  }
}

void AudioEngine::loadFile(const juce::File &file) {
  transportSource.stop();
  transportSource.setSource(nullptr);
  readerSource.reset();

  if (auto *reader = formatManager.createReaderFor(file)) {
    fileSampleRate = reader->sampleRate;
    readerSource =
        std::make_unique<juce::AudioFormatReaderSource>(reader, true);
    transportSource.setSource(readerSource.get(), 0, nullptr, fileSampleRate);
    thumbnail.setSource(new juce::FileInputSource(file));

    // Read into buffer for analysis
    loadedBuffer.setSize((int)reader->numChannels,
                         (int)reader->lengthInSamples);
    reader->read(&loadedBuffer, 0, (int)reader->lengthInSamples, 0, true, true);

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
  if (sliceIndex >= 0 && sliceIndex < (int)analysisResults.onsets.size()) {
    transportSource.stop();
    double startTime =
        (double)analysisResults.onsets[sliceIndex] / fileSampleRate;

    // Determine end time (next onset or end of file)
    if (sliceIndex + 1 < (int)analysisResults.onsets.size()) {
      stopAtPosition =
          (double)analysisResults.onsets[sliceIndex + 1] / fileSampleRate;
    } else {
      stopAtPosition = (double)loadedBuffer.getNumSamples() / fileSampleRate;
    }

    transportSource.setPosition(startTime);
    transportSource.start();
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

    if (auto writer =
            std::unique_ptr<juce::AudioFormatWriter>(wavFormat.createWriterFor(
                new juce::FileOutputStream(sliceFile), getSampleRate(),
                loadedBuffer.getNumChannels(), 16, {}, 0))) {
      writer->writeFromAudioSampleBuffer(loadedBuffer, startSample, numSamples);
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
