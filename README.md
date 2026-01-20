# Sampler Pro

A professional standalone audio sampler application built with C++ and the JUCE framework. Designed for quick sample analysis, robust BPM detection, and interactive slicing.

![Sampler Pro](https://raw.githubusercontent.com/skdsam/sampler-pro/main/preview.png)

## Key Features

- **Advanced BPM Detection**:
  - Uses an **Onset Detection Function (ODF)** combined with **Autocorrelation** to analyze energy flux.
  - Multi-hypothesis testing to resolve harmonic aliasing (e.g., distinguishing 140 BPM from 93.8 BPM).
  - High-precision 5ms analysis window.
- **Interactive Waveform**:
  - **Zoom & Scroll**: Use the slider or mouse wheel for precise editing.
  - **Manual Slicing**: Drag white spread markers to adjust slice points in real-time.
  - **Red Playhead**: High-visibility playback tracking.
- **Playback & Export**:
  - **One-Shot Slicing**: Click any slice on the waveform to play it instantly.
  - **Export Options**: Export sliced regions as individual WAVs or generate a MIDI map.
  - **Drag & Drop**: Load samples directly from your file explorer.

## Build Instructions (Windows)

Prerequisites:
- [CMake](https://cmake.org/download/) (3.15+)
- Visual Studio 2022 (with C++ Desktop Development workload)

1.  **Clone the Repository**:
    ```powershell
    git clone https://github.com/skdsam/sampler-pro.git
    cd sampler-pro
    ```

2.  **Configure the Project**:
    ```powershell
    cmake -S . -B build
    ```

3.  **Build**:
    ```powershell
    cmake --build build --config Release --target SamplerPro
    ```

4.  **Run**:
    The executable will be located in `build/SamplerPro_artefacts/Release/Sampler Pro.exe`.

## Project Structure

- `Source/`: Main C++ application code.
  - `AudioAnalysis`: BPM and Pitch detection algorithms.
  - `AudioEngine`: Handle playback, voices, and audio transport.
  - `WaveformComponent`: Custom UI component for rendering and interaction.
  - `MainComponent`: UI Layout and control logic.
- `libs/JUCE`: The JUCE framework (submodule or local copy).

## License

This project is built using the JUCE Framework (GPLv3).
