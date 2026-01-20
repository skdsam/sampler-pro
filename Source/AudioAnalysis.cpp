#include "AudioAnalysis.h"
#include <algorithm>
#include <cmath>
#include <numeric>

AudioAnalysis::AnalysisResults
AudioAnalysis::analyze(const juce::AudioBuffer<float> &buffer,
                       double sampleRate) {
  AnalysisResults results;

  results.onsets = findOnsets(buffer, sampleRate);

  // Detect BPM using ODF and Autocorrelation
  results.bpm = detectBPM(buffer, sampleRate);

  results.frequency = detectFrequency(buffer, sampleRate);

  return results;
}

std::vector<int>
AudioAnalysis::findOnsets(const juce::AudioBuffer<float> &buffer,
                          double sampleRate) {
  std::vector<int> onsets;
  if (sampleRate <= 0)
    return onsets;

  const int numSamples = buffer.getNumSamples();
  const int windowSize =
      (int)(0.005 * sampleRate); // 5ms window for better transient detail
  const float threshold = 0.02f; // Lowered threshold for better sensitivity

  float lastEnergy = 0.0f;

  for (int i = 0; i < numSamples - windowSize; i += windowSize / 2) {
    float energy = 0.0f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
      auto *data = buffer.getReadPointer(channel, i);
      for (int s = 0; s < windowSize; ++s)
        energy += data[s] * data[s];
    }

    energy /= (windowSize * buffer.getNumChannels());
    energy = std::sqrt(energy);

    if (energy > threshold && energy > lastEnergy * 1.2f) {
      // Simple peak picking: find the actual local max within this window
      onsets.push_back(i);
      int skip = (int)(0.05 * sampleRate); // Minimum 50ms between onsets (allow
                                           // faster slices)
      i += std::max(1, skip);
    }

    lastEnergy = energy;
  }

  return onsets;
}

double AudioAnalysis::detectBPM(const juce::AudioBuffer<float> &buffer,
                                double sampleRate) {
  if (sampleRate <= 0 || buffer.getNumSamples() < 1024)
    return 0.0;

  // 1. Create Onset Detection Function (ODF)
  const float hopSeconds = 0.005f; // 5ms hops for better transient resolution
  const int hopSize = (int)(hopSeconds * sampleRate);
  const int numSamples = buffer.getNumSamples();
  std::vector<float> odf;
  float lastEnergy = 0.0f;

  for (int i = 0; i < numSamples - hopSize; i += hopSize) {
    float energy = 0.0f;
    for (int c = 0; c < buffer.getNumChannels(); ++c) {
      auto *data = buffer.getReadPointer(c, i);
      for (int s = 0; s < hopSize; ++s)
        energy += data[s] * data[s];
    }
    energy = std::sqrt(energy / (hopSize * buffer.getNumChannels()));

    float flux = std::max(0.0f, energy - lastEnergy);
    odf.push_back(flux);
    lastEnergy = energy;
  }

  if (odf.size() < 20)
    return 0.0;

  // 2. Autocorrelation on ODF
  // Pulse range: 60 BPM (1s) to 220 BPM (~0.27s)
  int minLag = (int)(0.27 / hopSeconds);
  int maxLag = (int)(1.1 / hopSeconds);
  maxLag = std::min((int)odf.size() - 2, maxLag);

  struct Peak {
    int lag;
    float value;
  };
  std::vector<Peak> allPeaks;

  std::vector<float> acResult(maxLag + 1, 0.0f);
  for (int lag = minLag; lag <= maxLag; ++lag) {
    float corr = 0.0f;
    int count = 0;
    for (int i = 0; i < (int)odf.size() - lag; ++i) {
      corr += odf[i] * odf[i + lag];
      count++;
    }
    if (count > 0)
      acResult[lag] = corr / (float)count;
  }

  // Find all local maxima (peaks)
  for (int lag = minLag + 1; lag < maxLag; ++lag) {
    if (acResult[lag] > acResult[lag - 1] &&
        acResult[lag] > acResult[lag + 1]) {
      // Weighting to slightly prefer the 100-150 BPM range
      double bpmAtLag = 60.0 / (lag * hopSeconds);
      double weight = 1.0;
      if (bpmAtLag >= 100.0 && bpmAtLag <= 150.0)
        weight = 1.2;
      else if (bpmAtLag > 150.0 && bpmAtLag <= 200.0)
        weight = 1.1;

      allPeaks.push_back({lag, acResult[lag] * (float)weight});
    }
  }

  if (allPeaks.empty())
    return 0.0;

  // Sort peaks by value
  std::sort(allPeaks.begin(), allPeaks.end(),
            [](const Peak &a, const Peak &b) { return a.value > b.value; });

  // 3. Harmonic Check
  // If the best peak is a 2/3 or 1/2 multiplier of a slightly weaker but
  // standard peak, prefer the standard one.
  int bestLag = allPeaks[0].lag;
  float maxVal = allPeaks[0].value;

  for (size_t i = 1; i < std::min((size_t)5, allPeaks.size()); ++i) {
    float ratio = (float)allPeaks[i].lag / (float)bestLag;

    // Check for 3:2 (where 140 vs 93.3 happens) or 2:1 relationships
    // 140 is ~0.428s (85 bins at 5ms), 93.3 is ~0.642s (128 bins) -> lag ratio
    // ~0.66
    bool isHarmonic = (std::abs(ratio - 0.5f) < 0.05f) ||
                      (std::abs(ratio - 0.666f) < 0.05f) ||
                      (std::abs(ratio - 0.75f) < 0.05f);

    // If the shorter lag (higher BPM) is at least 60% as strong as the max
    // peak, it's likely the "intended" tempo
    if (isHarmonic && allPeaks[i].value > maxVal * 0.6f) {
      bestLag = allPeaks[i].lag;
      maxVal = allPeaks[i].value;
      break;
    }
  }

  double finalBpm = 60.0 / (bestLag * hopSeconds);
  return std::round(finalBpm * 10.0) / 10.0;
}

double AudioAnalysis::detectFrequency(const juce::AudioBuffer<float> &buffer,
                                      double sampleRate) {
  // Simplified Autocorrelation for Pitch Detection
  const int maxSamples =
      std::min(buffer.getNumSamples(), (int)(4096)); // Analyze first ~90ms
  if (maxSamples < 512 || sampleRate <= 0)
    return 0.0;

  std::vector<float> ac(maxSamples, 0.0f);
  auto *data = buffer.getReadPointer(0); // Use first channel

  for (int lag = 0; lag < maxSamples; ++lag) {
    for (int i = 0; i < maxSamples - lag; ++i)
      ac[lag] += data[i] * data[i + lag];
  }

  // Find first significant peak after the zero-lag peak
  int peakLag = 0;
  float maxVal = 0.0f;
  bool passedZeroLag = false;

  for (int i = 1; i < maxSamples - 1; ++i) {
    if (!passedZeroLag) {
      if (ac[i] < ac[i - 1])
        passedZeroLag = true;
      continue;
    }

    if (ac[i] > ac[i - 1] && ac[i] > ac[i + 1]) {
      if (ac[i] > maxVal) {
        maxVal = ac[i];
        peakLag = i;
      }
    }
  }

  if (peakLag > 0)
    return sampleRate / (double)peakLag;

  return 0.0;
}
