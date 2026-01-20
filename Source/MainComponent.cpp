#include "MainComponent.h"

MainComponent::MainComponent()
    : waveformComponent(audioEngine.getThumbnail(),
                        &audioEngine.getAnalysis().onsets) {
  addAndMakeVisible(openButton);
  addAndMakeVisible(playButton);
  addAndMakeVisible(stopButton);
  addAndMakeVisible(exportMidiButton);
  addAndMakeVisible(exportSlicesButton);
  addAndMakeVisible(tempoSlider);
  addAndMakeVisible(tempoLabel);
  addAndMakeVisible(waveformComponent);
  addAndMakeVisible(statusLabel);
  addAndMakeVisible(zoomSlider);
  addAndMakeVisible(zoomLabel);

  // Styling
  zoomLabel.setFont(juce::Font(12.0f));
  zoomLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
  zoomLabel.setJustificationType(juce::Justification::centred);

  auto setupButton = [this](juce::TextButton &b, juce::Colour c) {
    b.setColour(juce::TextButton::buttonColourId, darkHeaderColor);
    b.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    b.setColour(juce::TextButton::buttonOnColourId, c);
  };

  setupButton(openButton, juce::Colours::grey);
  setupButton(playButton, juce::Colours::darkgreen);
  setupButton(stopButton, juce::Colours::darkred);
  setupButton(exportMidiButton, juce::Colours::darkorange);
  setupButton(exportSlicesButton, juce::Colours::darkblue);

  statusLabel.setColour(juce::Label::textColourId, juce::Colours::white);
  tempoLabel.setColour(juce::Label::textColourId, juce::Colours::white);

  openButton.onClick = [this] {
    fileChooser = std::make_unique<juce::FileChooser>(
        "Select a Sample...",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory),
        "*.wav;*.aif;*.mp3");

    auto flags = juce::FileBrowserComponent::openMode |
                 juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync(flags, [this](const juce::FileChooser &chooser) {
      auto file = chooser.getResult();
      if (file != juce::File{}) {
        statusLabel.setText("Analyzing Sample...", juce::dontSendNotification);
        audioEngine.loadFile(file);
      }
    });
  };

  playButton.onClick = [this] { audioEngine.play(); };
  stopButton.onClick = [this] { audioEngine.stop(); };

  exportMidiButton.onClick = [this] {
    fileChooser = std::make_unique<juce::FileChooser>(
        "Export MIDI...",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory), "*.mid");

    auto flags = juce::FileBrowserComponent::saveMode |
                 juce::FileBrowserComponent::warnAboutOverwriting;

    fileChooser->launchAsync(flags, [this](const juce::FileChooser &chooser) {
      auto file = chooser.getResult();
      if (file != juce::File{})
        audioEngine.exportMidi(file);
    });
  };

  exportSlicesButton.onClick = [this] {
    fileChooser = std::make_unique<juce::FileChooser>(
        "Select Export Directory...",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory), "*");

    auto flags = juce::FileBrowserComponent::openMode |
                 juce::FileBrowserComponent::canSelectDirectories;

    fileChooser->launchAsync(flags, [this](const juce::FileChooser &chooser) {
      auto file = chooser.getResult();
      if (file.isDirectory())
        audioEngine.exportSlices(file);
    });
  };

  tempoSlider.setRange(20.0, 280.0, 0.1);
  tempoSlider.onValueChange = [this] {
    audioEngine.setTempo(tempoSlider.getValue());
  };
  tempoLabel.attachToComponent(&tempoSlider, true);

  zoomSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  zoomSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
  zoomSlider.setRange(1.0, 100.0, 1.0);
  zoomSlider.setValue(1.0);
  zoomSlider.onValueChange = [this] {
    waveformComponent.setZoomLevel(zoomSlider.getValue());
  };

  waveformComponent.onZoomChanged = [this] {
    zoomSlider.setValue(waveformComponent.getZoomLevel(),
                        juce::dontSendNotification);
  };

  statusLabel.setText("Sampler Pro - Ready (Drag & Drop Supported)",
                      juce::dontSendNotification);
  statusLabel.setJustificationType(juce::Justification::centred);

  audioEngine.addChangeListener(this);

  waveformComponent.onSliceClicked = [this](int index) {
    audioEngine.playSlice(index);
  };

  setWantsKeyboardFocus(true);

  startTimerHz(60);

  // Initialize audio
  setAudioChannels(0, 2);

  setSize(900, 600);
}

MainComponent::~MainComponent() {
  audioEngine.removeChangeListener(this);
  shutdownAudio();
}

void MainComponent::prepareToPlay(int samplesPerBlockExpected,
                                  double sampleRate) {
  audioEngine.prepareToPlay(sampleRate, samplesPerBlockExpected);
}

void MainComponent::getNextAudioBlock(
    const juce::AudioSourceChannelInfo &bufferToFill) {
  juce::MidiBuffer midi;
  audioEngine.processBlock(*bufferToFill.buffer, midi);
}

void MainComponent::releaseResources() { audioEngine.releaseResources(); }

void MainComponent::paint(juce::Graphics &g) { g.fillAll(darkBgColor); }

void MainComponent::resized() {
  auto bounds = getLocalBounds();
  auto headerArea = bounds.removeFromTop(100).reduced(10, 5);

  auto buttonArea = headerArea.removeFromTop(40);
  int btnWidth = 80;
  openButton.setBounds(buttonArea.removeFromLeft(btnWidth).reduced(2));
  playButton.setBounds(buttonArea.removeFromLeft(btnWidth).reduced(2));
  stopButton.setBounds(buttonArea.removeFromLeft(btnWidth).reduced(2));
  exportMidiButton.setBounds(buttonArea.removeFromLeft(btnWidth).reduced(2));
  exportSlicesButton.setBounds(buttonArea.removeFromLeft(btnWidth).reduced(2));

  auto controlArea = headerArea;
  auto tempoArea = controlArea.removeFromLeft(200);
  tempoLabel.setBounds(tempoArea.removeFromLeft(60));
  tempoSlider.setBounds(tempoArea);

  auto zoomArea = controlArea.removeFromLeft(200);
  zoomLabel.setBounds(zoomArea.removeFromLeft(60));
  zoomSlider.setBounds(zoomArea);

  statusLabel.setBounds(controlArea);

  bounds.reduce(20, 10);
  waveformComponent.setBounds(bounds.removeFromTop(300));
  statusLabel.setBounds(bounds.removeFromBottom(40));
}

bool MainComponent::isInterestedInFileDrag(const juce::StringArray &files) {
  for (auto &f : files) {
    if (f.endsWith(".wav") || f.endsWith(".mp3") || f.endsWith(".aif") ||
        f.endsWith(".aiff"))
      return true;
  }
  return false;
}

void MainComponent::filesDropped(const juce::StringArray &files, int x, int y) {
  if (files.size() > 0) {
    statusLabel.setText("Analyzing Sample...", juce::dontSendNotification);
    audioEngine.loadFile(juce::File(files[0]));
  }
}

void MainComponent::changeListenerCallback(juce::ChangeBroadcaster *source) {
  if (source == &audioEngine) {
    auto &analysis = audioEngine.getAnalysis();
    statusLabel.setText("BPM: " + juce::String(analysis.bpm, 1) + " | Pitch: " +
                            juce::String(analysis.frequency, 1) + " Hz",
                        juce::dontSendNotification);

    tempoSlider.setValue(analysis.bpm, juce::dontSendNotification);
    waveformComponent.setSampleRate(audioEngine.getFileSampleRate());
    waveformComponent.setOnsets(
        &analysis.onsets); // Refresh the pointer/reference
    waveformComponent.repaint();
  }
}

void MainComponent::timerCallback() {
  waveformComponent.setPlayheadTime(audioEngine.getCurrentPosition());
}

bool MainComponent::keyPressed(const juce::KeyPress &key) {
  if (key.getKeyCode() == juce::KeyPress::spaceKey) {
    if (audioEngine.isPlaying())
      audioEngine.stop();
    else
      audioEngine.play();
    return true;
  }
  return false;
}
