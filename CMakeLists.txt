cmake_minimum_required(VERSION 3.22)
project(EonMiniEEF VERSION 0.1.0)

include(CTest)
enable_testing()

include(FetchContent)
set(JUCE_SOURCE_DIR "" CACHE PATH "JUCE 8 source directory (optional)")
if (JUCE_SOURCE_DIR)
    add_subdirectory(${JUCE_SOURCE_DIR} ${CMAKE_BINARY_DIR}/juce)
else()
    FetchContent_Declare(juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.14
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(juce)
endif()

juce_add_plugin(EonMiniEEF
    COMPANY_NAME "EON Audio"
    BUNDLE_ID com.eonaudio.eefjp8000
    IS_SYNTH TRUE
    IS_MIDI_EFFECT FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_MANUFACTURER_CODE EonA
    PLUGIN_CODE MnEf
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "EEF-JP8000")
juce_generate_juce_header(EonMiniEEF)

target_sources(EonMiniEEF PRIVATE
    Source/PluginProcessor.cpp Source/PluginProcessor.h
    Source/PluginEditor.cpp Source/PluginEditor.h
    Source/FactoryPresets.cpp Source/FactoryPresets.h
    Source/Identifiers/ParamIDs.h)
target_compile_features(EonMiniEEF PRIVATE cxx_std_20)
target_compile_definitions(EonMiniEEF PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(EonMiniEEF PRIVATE juce::juce_audio_utils juce::juce_dsp)

# Small offline renderer used by CI/design reviews to verify the editor skin
# without opening a DAW. It never participates in the plugin targets.
add_executable(EonMiniEEF_UISnapshot tools/UiSnapshot.cpp)
# Renders the editor at an arbitrary window size passed on the command line, so
# any supported size can be checked without adding a target per size.  It exits
# with 2 when the requested size is below the editor's resize limits, so a
# clamped render can never be mistaken for a passing one.
add_executable(EonMiniEEF_UISnapshotAt tools/UiSnapshotAt.cpp)
target_compile_features(EonMiniEEF_UISnapshotAt PRIVATE cxx_std_20)
target_compile_definitions(EonMiniEEF_UISnapshotAt PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_include_directories(EonMiniEEF_UISnapshotAt PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/EonMiniEEF_artefacts/JuceLibraryCode)
target_link_libraries(EonMiniEEF_UISnapshotAt PRIVATE EonMiniEEF juce::juce_audio_utils juce::juce_dsp)
target_compile_features(EonMiniEEF_UISnapshot PRIVATE cxx_std_20)
target_compile_definitions(EonMiniEEF_UISnapshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_include_directories(EonMiniEEF_UISnapshot PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/EonMiniEEF_artefacts/JuceLibraryCode)
target_link_libraries(EonMiniEEF_UISnapshot PRIVATE EonMiniEEF juce::juce_audio_utils juce::juce_dsp)

# Headless checks for factory patch state and an exporter for versioned
# .eonpreset files distributed with the source tree.
add_executable(EonMiniEEF_PresetSmoke tools/PresetSmoke.cpp)
target_compile_features(EonMiniEEF_PresetSmoke PRIVATE cxx_std_20)
target_compile_definitions(EonMiniEEF_PresetSmoke PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_include_directories(EonMiniEEF_PresetSmoke PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/EonMiniEEF_artefacts/JuceLibraryCode)
target_link_libraries(EonMiniEEF_PresetSmoke PRIVATE EonMiniEEF juce::juce_audio_utils juce::juce_dsp)

add_executable(EonMiniEEF_FactoryPresetExport tools/ExportFactoryPresets.cpp)
target_compile_features(EonMiniEEF_FactoryPresetExport PRIVATE cxx_std_20)
target_compile_definitions(EonMiniEEF_FactoryPresetExport PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_include_directories(EonMiniEEF_FactoryPresetExport PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/EonMiniEEF_artefacts/JuceLibraryCode)
target_link_libraries(EonMiniEEF_FactoryPresetExport PRIVATE EonMiniEEF juce::juce_audio_utils juce::juce_dsp)

# Fast, headless regression checks.  These are intentionally registered with
# CTest so CI can run `ctest --test-dir Build -C Release --output-on-failure`
# without launching a host or opening a plugin window.
add_test(NAME preset_state_and_audio_smoke
    COMMAND $<TARGET_FILE:EonMiniEEF_PresetSmoke>)
set_tests_properties(preset_state_and_audio_smoke PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

# Guard the audio callback against accidentally falling back to JUCE
# Synthesiser entry points that acquire a CriticalSection internally.
add_test(NAME realtime_audio_contract
    COMMAND ${CMAKE_COMMAND}
        -DPROCESSOR_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/Source/PluginProcessor.cpp
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tools/RealtimeAudioContract.cmake)

find_package(Python3 COMPONENTS Interpreter QUIET)
if (Python3_Interpreter_FOUND)
    add_test(NAME dsp_numeric_sanity
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_dsp_sanity.py")
    set_tests_properties(dsp_numeric_sanity PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

# Sweeps a sine through the real voice and measures how much of it folds back
# below Nyquist, per sample rate and per oversampling mode.  This is the
# measurement DESIGN.md asks for under "음질 계측": it quantifies the PolyBLEP
# response instead of asserting it.  Not registered with CTest: it prints a
# table, and the numbers move with every DSP change, so it belongs to a report
# rather than to a pass/fail gate.
add_executable(EonMiniEEF_SawSweep tools/SawSweep.cpp)
target_compile_features(EonMiniEEF_SawSweep PRIVATE cxx_std_20)
target_compile_definitions(EonMiniEEF_SawSweep PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_include_directories(EonMiniEEF_SawSweep PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/EonMiniEEF_artefacts/JuceLibraryCode)
target_link_libraries(EonMiniEEF_SawSweep PRIVATE EonMiniEEF juce::juce_audio_utils juce::juce_dsp)

# Times the real processBlock() under a worst-case voice load.  The older
# cpu-smoke in run_dsp_sanity.py times a Python re-implementation, which says
# nothing about the shipping code; this one calls the processor.
add_executable(EonMiniEEF_CpuProbe tools/CpuProbe.cpp)
target_compile_features(EonMiniEEF_CpuProbe PRIVATE cxx_std_20)
target_compile_definitions(EonMiniEEF_CpuProbe PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_include_directories(EonMiniEEF_CpuProbe PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/EonMiniEEF_artefacts/JuceLibraryCode)
target_link_libraries(EonMiniEEF_CpuProbe PRIVATE EonMiniEEF juce::juce_audio_utils juce::juce_dsp)

# Offline WAV renderer for ears-on verification without a DAW.  Renders a
# 4-note chord through the selected factory preset so users can listen to the
# synth offline.  Run: EonMiniEEF_RenderWav <preset 0-33> <out.wav>
add_executable(EonMiniEEF_RenderWav tools/RenderWav.cpp)
target_compile_features(EonMiniEEF_RenderWav PRIVATE cxx_std_20)
target_compile_definitions(EonMiniEEF_RenderWav PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_include_directories(EonMiniEEF_RenderWav PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/EonMiniEEF_artefacts/JuceLibraryCode)
target_link_libraries(EonMiniEEF_RenderWav PRIVATE EonMiniEEF juce::juce_audio_utils juce::juce_dsp juce::juce_audio_formats)

# Roadmap step A: drive-stage THD/harmonic baseline.  Run directly:
#   ./Build-Git2/EonMiniEEF_ThdProbe
add_executable(EonMiniEEF_ThdProbe tools/ThdProbe.cpp)
target_compile_features(EonMiniEEF_ThdProbe PRIVATE cxx_std_20)
target_compile_definitions(EonMiniEEF_ThdProbe PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_include_directories(EonMiniEEF_ThdProbe PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/EonMiniEEF_artefacts/JuceLibraryCode)
target_link_libraries(EonMiniEEF_ThdProbe PRIVATE EonMiniEEF juce::juce_audio_utils juce::juce_dsp)

# Per-waveform harmonic baseline.  Unlike ThdProbe, which drives a sine to
# isolate the saturation stage, this one renders each oscillator shape on its
# own so the saw / square / triangle spectra have a recorded reference.  The
# gates only assert that the measurement is sound and reproducible; the
# numbers are the baseline that later waveform work is compared against.
add_executable(EonMiniEEF_WaveHarmonics tools/WaveHarmonics.cpp)
target_compile_features(EonMiniEEF_WaveHarmonics PRIVATE cxx_std_20)
target_compile_definitions(EonMiniEEF_WaveHarmonics PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_include_directories(EonMiniEEF_WaveHarmonics PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/EonMiniEEF_artefacts/JuceLibraryCode)
target_link_libraries(EonMiniEEF_WaveHarmonics PRIVATE EonMiniEEF juce::juce_audio_utils juce::juce_dsp)

add_test(NAME wave_harmonic_baseline
    COMMAND $<TARGET_FILE:EonMiniEEF_WaveHarmonics>)

# Pitch bend +/-2 semitones and CC64 sustain. Headless processor render.
add_executable(EonMiniEEF_MidiContract tools/MidiContract.cpp)
target_compile_features(EonMiniEEF_MidiContract PRIVATE cxx_std_20)
target_compile_definitions(EonMiniEEF_MidiContract PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_include_directories(EonMiniEEF_MidiContract PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/EonMiniEEF_artefacts/JuceLibraryCode)
target_link_libraries(EonMiniEEF_MidiContract PRIVATE EonMiniEEF juce::juce_audio_utils juce::juce_dsp)
add_test(NAME midi_contract COMMAND $<TARGET_FILE:EonMiniEEF_MidiContract>)
set_tests_properties(wave_harmonic_baseline PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
