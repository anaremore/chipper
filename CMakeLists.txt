cmake_minimum_required(VERSION 3.22)

project(Chipper VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CHIPPER_BUILD_PLUGIN "Build the JUCE VST3 plugin" ON)
find_package(Python3 COMPONENTS Interpreter)

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
add_test(NAME chipper_render_dmg_smoke
    COMMAND chipper_render --chip dmg --accuracy authentic --macro lead --clock 4194304 --rate 48000 --seconds 0.10 --note 72 --out dmg-smoke.wav --debug dmg-smoke.json
)
add_test(NAME chipper_render_dmg_noise_envelope_smoke
    COMMAND chipper_render --chip dmg --accuracy authentic --macro manual --clock 4194304 --rate 48000 --seconds 0.20 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/dmg-noise-envelope-events.txt --out dmg-noise-envelope-smoke.wav --debug dmg-noise-envelope-smoke.json
)
add_test(NAME chipper_render_dmg_power_wave_smoke
    COMMAND chipper_render --chip dmg --accuracy authentic --macro manual --clock 4194304 --rate 48000 --seconds 0.02 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/dmg-power-wave-events.txt --out dmg-power-wave-smoke.wav --debug dmg-power-wave-smoke.json
)
add_test(NAME chipper_render_nes_drum_envelope_smoke
    COMMAND chipper_render --chip nes --accuracy authentic --macro drum --clock 1789773 --rate 48000 --seconds 0.20 --note 48 --out nes-drum-envelope-smoke.wav --debug nes-drum-envelope-smoke.json
)

if (Python3_Interpreter_FOUND)
    add_test(NAME chipper_render_nes_json_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/nes-smoke.json --implemented-contains partial --macro Manual --min-peak 0.1 --min-rms 0.05 --register-writes 0 --note-events 0 --min-zero-crossings 30 --max-zero-crossings 120
    )
    set_tests_properties(chipper_render_nes_json_assert PROPERTIES DEPENDS chipper_render_nes_smoke)

    add_test(NAME chipper_render_dmg_json_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/dmg-smoke.json --implemented-contains partial --macro Lead --min-peak 0.1 --min-rms 0.03 --register-writes 0 --note-events 0 --min-zero-crossings 90 --max-zero-crossings 160 --core-min enabled0=1 --core-max enabled0=1 --core-min enabledNoise=0 --core-max enabledNoise=0
    )
    set_tests_properties(chipper_render_dmg_json_assert PROPERTIES DEPENDS chipper_render_dmg_smoke)

    add_test(NAME chipper_render_dmg_noise_envelope_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/dmg-noise-envelope-smoke.json --implemented-contains partial --macro Manual --min-peak 0.1 --min-rms 0.1 --register-writes 5 --note-events 1 --min-zero-crossings 250 --max-zero-crossings 400 --core-min envelopeNoise=2 --core-max envelopeNoise=4 --core-min lengthNoise=10 --core-max lengthNoise=16 --core-min enabledNoise=1 --core-max enabledNoise=1
    )
    set_tests_properties(chipper_render_dmg_noise_envelope_assert PROPERTIES DEPENDS chipper_render_dmg_noise_envelope_smoke)

    add_test(NAME chipper_render_dmg_power_wave_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/dmg-power-wave-smoke.json --implemented-contains partial --macro Manual --register-writes 2 --note-events 1 --core-min waveRam0=171 --core-max waveRam0=171 --core-min enabled0=0 --core-max enabled0=0 --core-min enabledNoise=0 --core-max enabledNoise=0
    )
    set_tests_properties(chipper_render_dmg_power_wave_assert PROPERTIES DEPENDS chipper_render_dmg_power_wave_smoke)

    add_test(NAME chipper_render_event_json_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/nes-events-smoke.json --implemented-contains partial --macro Lead --min-peak 0.1 --min-rms 0.03 --register-writes 1 --note-events 4
    )
    set_tests_properties(chipper_render_event_json_assert PROPERTIES DEPENDS chipper_render_nes_event_trace_smoke)

    add_test(NAME chipper_render_nes_drum_envelope_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/nes-drum-envelope-smoke.json --implemented-contains partial --macro Drum --min-peak 0.1 --core-max envelopeNoise=15 --core-max length0=10
    )
    set_tests_properties(chipper_render_nes_drum_envelope_assert PROPERTIES DEPENDS chipper_render_nes_drum_envelope_smoke)
endif()

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
