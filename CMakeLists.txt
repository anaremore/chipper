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
add_test(NAME chipper_render_dmg_noise_divisor0_smoke
    COMMAND chipper_render --chip dmg --accuracy authentic --macro manual --clock 4194304 --rate 48000 --seconds 0.02 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/dmg-noise-divisor0-events.txt --out dmg-noise-divisor0-smoke.wav --debug dmg-noise-divisor0-smoke.json
)
add_test(NAME chipper_render_dmg_noise_shift_stop_smoke
    COMMAND chipper_render --chip dmg --accuracy authentic --macro manual --clock 4194304 --rate 48000 --seconds 0.02 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/dmg-noise-shift-stop-events.txt --out dmg-noise-shift-stop-smoke.wav --debug dmg-noise-shift-stop-smoke.json
)
add_test(NAME chipper_render_dmg_power_wave_smoke
    COMMAND chipper_render --chip dmg --accuracy authentic --macro manual --clock 4194304 --rate 48000 --seconds 0.02 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/dmg-power-wave-events.txt --out dmg-power-wave-smoke.wav --debug dmg-power-wave-smoke.json
)
add_test(NAME chipper_render_dmg_sweep_smoke
    COMMAND chipper_render --chip dmg --accuracy authentic --macro manual --clock 4194304 --rate 48000 --seconds 0.02 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/dmg-sweep-events.txt --out dmg-sweep-smoke.wav --debug dmg-sweep-smoke.json
)
add_test(NAME chipper_render_dmg_sweep_overflow_smoke
    COMMAND chipper_render --chip dmg --accuracy authentic --macro manual --clock 4194304 --rate 48000 --seconds 0.01 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/dmg-sweep-overflow-events.txt --out dmg-sweep-overflow-smoke.wav --debug dmg-sweep-overflow-smoke.json
)
add_test(NAME chipper_render_dmg_stereo_left_smoke
    COMMAND chipper_render --chip dmg --accuracy authentic --macro manual --clock 4194304 --rate 48000 --seconds 0.05 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/dmg-stereo-left-events.txt --out dmg-stereo-left-smoke.wav --debug dmg-stereo-left-smoke.json
)
add_test(NAME chipper_render_dmg_stereo_right_smoke
    COMMAND chipper_render --chip dmg --accuracy authentic --macro manual --clock 4194304 --rate 48000 --seconds 0.05 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/dmg-stereo-right-events.txt --out dmg-stereo-right-smoke.wav --debug dmg-stereo-right-smoke.json
)
add_test(NAME chipper_render_nes_drum_envelope_smoke
    COMMAND chipper_render --chip nes --accuracy authentic --macro drum --clock 1789773 --rate 48000 --seconds 0.20 --note 48 --out nes-drum-envelope-smoke.wav --debug nes-drum-envelope-smoke.json
)
add_test(NAME chipper_render_nes_sweep_add_smoke
    COMMAND chipper_render --chip nes --accuracy authentic --macro manual --clock 1789773 --rate 48000 --seconds 0.012 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/nes-sweep-add-events.txt --out nes-sweep-add-smoke.wav --debug nes-sweep-add-smoke.json
)
add_test(NAME chipper_render_nes_sweep_negate_smoke
    COMMAND chipper_render --chip nes --accuracy authentic --macro manual --clock 1789773 --rate 48000 --seconds 0.012 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/nes-sweep-negate-events.txt --out nes-sweep-negate-smoke.wav --debug nes-sweep-negate-smoke.json
)
add_test(NAME chipper_render_nes_sweep_mute_smoke
    COMMAND chipper_render --chip nes --accuracy authentic --macro manual --clock 1789773 --rate 48000 --seconds 0.004 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/nes-sweep-mute-events.txt --out nes-sweep-mute-smoke.wav --debug nes-sweep-mute-smoke.json
)
add_test(NAME chipper_render_nes_triangle_linear_reload_smoke
    COMMAND chipper_render --chip nes --accuracy authentic --macro manual --clock 1789773 --rate 48000 --seconds 0.020 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/nes-triangle-linear-reload-events.txt --out nes-triangle-linear-reload-smoke.wav --debug nes-triangle-linear-reload-smoke.json
)
add_test(NAME chipper_render_nes_triangle_linear_decay_smoke
    COMMAND chipper_render --chip nes --accuracy authentic --macro manual --clock 1789773 --rate 48000 --seconds 0.020 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/nes-triangle-linear-decay-events.txt --out nes-triangle-linear-decay-smoke.wav --debug nes-triangle-linear-decay-smoke.json
)
add_test(NAME chipper_render_nes_triangle_linear_mute_smoke
    COMMAND chipper_render --chip nes --accuracy authentic --macro manual --clock 1789773 --rate 48000 --seconds 0.020 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/nes-triangle-linear-mute-events.txt --out nes-triangle-linear-mute-smoke.wav --debug nes-triangle-linear-mute-smoke.json
)

if (Python3_Interpreter_FOUND)
    add_test(NAME chipper_render_nes_json_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/nes-smoke.json --implemented-contains partial --macro Manual --min-peak 0.1 --min-rms 0.05 --register-writes 0 --note-events 0 --min-zero-crossings 30 --max-zero-crossings 120
    )
    set_tests_properties(chipper_render_nes_json_assert PROPERTIES DEPENDS chipper_render_nes_smoke)

    add_test(NAME chipper_render_dmg_json_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/dmg-smoke.json --implemented-contains partial --macro Lead --min-peak 0.1 --min-rms 0.03 --register-writes 0 --note-events 0 --min-zero-crossings 30 --max-zero-crossings 80 --core-min pulse1FreqReg=800 --core-max pulse1FreqReg=860 --core-min sweepEnabled=1 --core-max sweepEnabled=1 --core-min enabled0=1 --core-max enabled0=1 --core-min enabledNoise=0 --core-max enabledNoise=0
    )
    set_tests_properties(chipper_render_dmg_json_assert PROPERTIES DEPENDS chipper_render_dmg_smoke)

    add_test(NAME chipper_render_dmg_noise_envelope_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/dmg-noise-envelope-smoke.json --implemented-contains partial --macro Manual --min-peak 0.1 --min-rms 0.1 --register-writes 5 --note-events 1 --min-zero-crossings 3000 --max-zero-crossings 3350 --core-min noiseClockHz=32767 --core-max noiseClockHz=32769 --core-min noiseShift=2 --core-max noiseShift=2 --core-min noiseDivisorCode=2 --core-max noiseDivisorCode=2 --core-min envelopeNoise=2 --core-max envelopeNoise=4 --core-min lengthNoise=10 --core-max lengthNoise=16 --core-min enabledNoise=1 --core-max enabledNoise=1
    )
    set_tests_properties(chipper_render_dmg_noise_envelope_assert PROPERTIES DEPENDS chipper_render_dmg_noise_envelope_smoke)

    add_test(NAME chipper_render_dmg_noise_divisor0_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/dmg-noise-divisor0-smoke.json --implemented-contains partial --macro Manual --min-rms 0.1 --register-writes 4 --note-events 1 --core-min noiseClockHz=524287 --core-max noiseClockHz=524289 --core-min noiseShift=0 --core-max noiseShift=0 --core-min noiseDivisorCode=0 --core-max noiseDivisorCode=0 --core-min enabledNoise=1 --core-max enabledNoise=1
    )
    set_tests_properties(chipper_render_dmg_noise_divisor0_assert PROPERTIES DEPENDS chipper_render_dmg_noise_divisor0_smoke)

    add_test(NAME chipper_render_dmg_noise_shift_stop_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/dmg-noise-shift-stop-smoke.json --implemented-contains partial --macro Manual --min-rms 0.1 --register-writes 4 --note-events 1 --max-zero-crossings 1 --core-min noiseClockHz=0 --core-max noiseClockHz=0 --core-min noiseShift=14 --core-max noiseShift=14 --core-min noiseDivisorCode=2 --core-max noiseDivisorCode=2 --core-min enabledNoise=1 --core-max enabledNoise=1
    )
    set_tests_properties(chipper_render_dmg_noise_shift_stop_assert PROPERTIES DEPENDS chipper_render_dmg_noise_shift_stop_smoke)

    add_test(NAME chipper_render_dmg_power_wave_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/dmg-power-wave-smoke.json --implemented-contains partial --macro Manual --register-writes 2 --note-events 1 --core-min waveRam0=171 --core-max waveRam0=171 --core-min enabled0=0 --core-max enabled0=0 --core-min enabledNoise=0 --core-max enabledNoise=0
    )
    set_tests_properties(chipper_render_dmg_power_wave_assert PROPERTIES DEPENDS chipper_render_dmg_power_wave_smoke)

    add_test(NAME chipper_render_dmg_sweep_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/dmg-sweep-smoke.json --implemented-contains partial --macro Manual --min-peak 0.1 --register-writes 6 --note-events 1 --core-min pulse1FreqReg=255 --core-max pulse1FreqReg=257 --core-min sweepShadow=255 --core-max sweepShadow=257 --core-min sweepEnabled=1 --core-max sweepEnabled=1 --core-min enabled0=1 --core-max enabled0=1
    )
    set_tests_properties(chipper_render_dmg_sweep_assert PROPERTIES DEPENDS chipper_render_dmg_sweep_smoke)

    add_test(NAME chipper_render_dmg_sweep_overflow_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/dmg-sweep-overflow-smoke.json --implemented-contains partial --macro Manual --register-writes 6 --note-events 1 --core-min pulse1FreqReg=1792 --core-max pulse1FreqReg=1792 --core-min sweepShadow=1792 --core-max sweepShadow=1792 --core-min sweepEnabled=1 --core-max sweepEnabled=1 --core-min enabled0=0 --core-max enabled0=0
    )
    set_tests_properties(chipper_render_dmg_sweep_overflow_assert PROPERTIES DEPENDS chipper_render_dmg_sweep_overflow_smoke)

    add_test(NAME chipper_render_dmg_stereo_left_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/dmg-stereo-left-smoke.json --implemented-contains partial --macro Manual --register-writes 2 --note-events 1 --min-left-rms 0.1 --max-right-rms 0.02 --core-min nr51=16 --core-max nr51=16 --core-min leftVolume=1 --core-max leftVolume=1 --core-min rightVolume=1 --core-max rightVolume=1
    )
    set_tests_properties(chipper_render_dmg_stereo_left_assert PROPERTIES DEPENDS chipper_render_dmg_stereo_left_smoke)

    add_test(NAME chipper_render_dmg_stereo_right_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/dmg-stereo-right-smoke.json --implemented-contains partial --macro Manual --register-writes 2 --note-events 1 --max-left-rms 0.02 --min-right-rms 0.1 --core-min nr51=1 --core-max nr51=1 --core-min leftVolume=1 --core-max leftVolume=1 --core-min rightVolume=1 --core-max rightVolume=1
    )
    set_tests_properties(chipper_render_dmg_stereo_right_assert PROPERTIES DEPENDS chipper_render_dmg_stereo_right_smoke)

    add_test(NAME chipper_render_event_json_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/nes-events-smoke.json --implemented-contains partial --macro Lead --min-peak 0.1 --min-rms 0.03 --register-writes 1 --note-events 4
    )
    set_tests_properties(chipper_render_event_json_assert PROPERTIES DEPENDS chipper_render_nes_event_trace_smoke)

    add_test(NAME chipper_render_nes_drum_envelope_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/nes-drum-envelope-smoke.json --implemented-contains partial --macro Drum --min-peak 0.1 --core-max envelopeNoise=15 --core-max length0=10
    )
    set_tests_properties(chipper_render_nes_drum_envelope_assert PROPERTIES DEPENDS chipper_render_nes_drum_envelope_smoke)

    add_test(NAME chipper_render_nes_sweep_add_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/nes-sweep-add-smoke.json --implemented-contains partial --macro Manual --register-writes 5 --note-events 0 --core-min pulseTimer1=382 --core-max pulseTimer1=382 --core-min sweepTarget0=573 --core-max sweepTarget0=573 --core-min sweepEnabled0=1 --core-max sweepEnabled0=1 --core-min sweepMuted0=0 --core-max sweepMuted0=0
    )
    set_tests_properties(chipper_render_nes_sweep_add_assert PROPERTIES DEPENDS chipper_render_nes_sweep_add_smoke)

    add_test(NAME chipper_render_nes_sweep_negate_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/nes-sweep-negate-smoke.json --implemented-contains partial --macro Manual --register-writes 5 --note-events 0 --core-min pulseTimer1=127 --core-max pulseTimer1=127 --core-min sweepTarget0=63 --core-max sweepTarget0=63 --core-min sweepEnabled0=1 --core-max sweepEnabled0=1 --core-min sweepMuted0=0 --core-max sweepMuted0=0
    )
    set_tests_properties(chipper_render_nes_sweep_negate_assert PROPERTIES DEPENDS chipper_render_nes_sweep_negate_smoke)

    add_test(NAME chipper_render_nes_sweep_mute_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/nes-sweep-mute-smoke.json --implemented-contains partial --macro Manual --register-writes 5 --note-events 0 --core-min pulseTimer1=1024 --core-max pulseTimer1=1024 --core-min sweepTarget0=2048 --core-max sweepTarget0=2048 --core-min sweepEnabled0=0 --core-max sweepEnabled0=0 --core-min sweepMuted0=1 --core-max sweepMuted0=1
    )
    set_tests_properties(chipper_render_nes_sweep_mute_assert PROPERTIES DEPENDS chipper_render_nes_sweep_mute_smoke)

    add_test(NAME chipper_render_nes_triangle_linear_reload_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/nes-triangle-linear-reload-smoke.json --implemented-contains partial --macro Manual --register-writes 4 --note-events 0 --core-min linearCounter=3 --core-max linearCounter=3 --core-min linearReloadValue=3 --core-max linearReloadValue=3 --core-min linearReloadFlag=1 --core-max linearReloadFlag=1 --core-min linearControlFlag=1 --core-max linearControlFlag=1 --core-min triangleActive=1 --core-max triangleActive=1
    )
    set_tests_properties(chipper_render_nes_triangle_linear_reload_assert PROPERTIES DEPENDS chipper_render_nes_triangle_linear_reload_smoke)

    add_test(NAME chipper_render_nes_triangle_linear_decay_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/nes-triangle-linear-decay-smoke.json --implemented-contains partial --macro Manual --register-writes 4 --note-events 0 --core-min linearCounter=0 --core-max linearCounter=0 --core-min linearReloadValue=2 --core-max linearReloadValue=2 --core-min linearReloadFlag=0 --core-max linearReloadFlag=0 --core-min linearControlFlag=0 --core-max linearControlFlag=0 --core-min triangleActive=0 --core-max triangleActive=0
    )
    set_tests_properties(chipper_render_nes_triangle_linear_decay_assert PROPERTIES DEPENDS chipper_render_nes_triangle_linear_decay_smoke)

    add_test(NAME chipper_render_nes_triangle_linear_mute_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/nes-triangle-linear-mute-smoke.json --implemented-contains partial --macro Manual --register-writes 4 --note-events 0 --core-min linearCounter=0 --core-max linearCounter=0 --core-min linearReloadValue=0 --core-max linearReloadValue=0 --core-min linearReloadFlag=1 --core-max linearReloadFlag=1 --core-min linearControlFlag=1 --core-max linearControlFlag=1 --core-min triangleActive=0 --core-max triangleActive=0
    )
    set_tests_properties(chipper_render_nes_triangle_linear_mute_assert PROPERTIES DEPENDS chipper_render_nes_triangle_linear_mute_smoke)
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
