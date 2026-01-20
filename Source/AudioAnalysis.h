#pragma once

#include <JuceHeader.h>
#include <vector>

class AudioAnalysis {
public:
  struct AnalysisResults {
    double bpm = 0.0;
    double frequency = 0.0;
    std::vector<int> onsets;
  };

  static AnalysisResults analyze(const juce::AudioBuffer<float> &buffer,
                                 double sampleRate);

private:
  static double detectBPM(const juce::AudioBuffer<float> &buffer,
                          double sampleRate);
  static double detectFrequency(const juce::AudioBuffer<float> &buffer,
                                double sampleRate);
  static std::vector<int> findOnsets(const juce::AudioBuffer<float> &buffer,
                                     double sampleRate);
};
