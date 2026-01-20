cmake_minimum_required(VERSION 3.15)

project(SamplerPro VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add JUCE
add_subdirectory(libs/JUCE)

# Create the Standalone App
juce_add_gui_app(SamplerPro
    COMPANY_NAME "Antigravity"
    PRODUCT_NAME "Sampler Pro"
    BUNDLE_ID "com.antigravity.samplerpro"
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_CONTENT_PREVIEW TRUE
)

# Source files
target_sources(SamplerPro PRIVATE
    Source/Main.cpp
    Source/MainComponent.h
    Source/MainComponent.cpp
    Source/AudioEngine.h
    Source/AudioEngine.cpp
    Source/WaveformComponent.h
    Source/AudioAnalysis.h
    Source/AudioAnalysis.cpp
    Source/MidiSequencerComponent.h
)

juce_generate_juce_header(SamplerPro)

target_include_directories(SamplerPro PRIVATE
    Source
    ${CMAKE_CURRENT_BINARY_DIR}/SamplerPro_artefacts/JuceLibraryCode
)

# Compile flags
target_compile_definitions(SamplerPro PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

# Link modules
target_link_libraries(SamplerPro PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce_recommended_config_flags
    juce_recommended_lto_flags
    juce_recommended_warning_flags
)
