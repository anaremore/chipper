cmake_minimum_required(VERSION 3.22)

project(Chipper VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CHIPPER_BUILD_PLUGIN "Build the JUCE VST3 plugin" ON)

add_library(chipper_engine STATIC
    Source/Engine/ChipDescriptors.cpp
    Source/Engine/ChipDescriptors.h
    Source/Engine/ChipCore.cpp
    Source/Engine/ChipCore.h
)

target_include_directories(chipper_engine PUBLIC Source)
target_compile_features(chipper_engine PUBLIC cxx_std_20)

add_executable(chipper_render tools/chipper_render.cpp)
target_link_libraries(chipper_render PRIVATE chipper_engine)

enable_testing()
add_test(NAME chipper_render_nes_smoke
    COMMAND chipper_render --chip nes --accuracy authentic --clock 1789773 --rate 48000 --seconds 0.10 --note 69 --out nes-smoke.wav --debug nes-smoke.json
)
add_test(NAME chipper_render_ym2149_smoke
    COMMAND chipper_render --chip ym2149 --accuracy authentic --clock 2000000 --rate 48000 --seconds 0.10 --note 72 --out ym-smoke.wav --debug ym-smoke.json
)
add_test(NAME chipper_render_sn76489_smoke
    COMMAND chipper_render --chip sn76489 --accuracy authentic --clock 3579545 --rate 48000 --seconds 0.10 --note 76 --out sn-smoke.wav --debug sn-smoke.json
)
add_test(NAME chipper_render_nes_coin_macro_smoke
    COMMAND chipper_render --chip nes --accuracy authentic --macro coin --clock 1789773 --rate 48000 --seconds 0.10 --note 69 --out nes-coin-smoke.wav --debug nes-coin-smoke.json
)
add_test(NAME chipper_render_nes_event_trace_smoke
    COMMAND chipper_render --chip nes --accuracy authentic --macro lead --clock 1789773 --rate 48000 --seconds 0.10 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/nes-note-events.txt --out nes-events-smoke.wav --debug nes-events-smoke.json
)

if (NOT CHIPPER_BUILD_PLUGIN)
    return()
endif()

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.13
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Chipper
    VERSION ${PROJECT_VERSION}
    COMPANY_NAME "Anaremore"
    BUNDLE_ID "com.anaremore.chipper"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Anrm
    PLUGIN_CODE Chpr
    FORMATS VST3 Standalone
    PRODUCT_NAME "Chipper"
)

juce_generate_juce_header(Chipper)

target_sources(Chipper
    PRIVATE
        Source/Parameters.cpp
        Source/Parameters.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/Presets.cpp
        Source/Presets.h
)

target_include_directories(Chipper PRIVATE Source)

target_compile_definitions(Chipper
    PUBLIC
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Chipper
    PRIVATE
        chipper_engine
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
