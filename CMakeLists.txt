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
    Source/Presets.cpp
    Source/Presets.h
)

target_include_directories(chipper_engine PUBLIC Source)
target_compile_features(chipper_engine PUBLIC cxx_std_20)

add_executable(chipper_render tools/chipper_render.cpp)
target_link_libraries(chipper_render PRIVATE chipper_engine)

enable_testing()
add_test(NAME chipper_render_nes_smoke
    COMMAND chipper_render --chip nes --accuracy authentic --clock 1789773 --rate 48000 --seconds 0.10 --note 69 --out nes-smoke.wav --debug nes-smoke.json
)
add_test(NAME chipper_render_sid_chip_poly_fallback_smoke
    COMMAND chipper_render --chip sid --accuracy authentic --play-mode chip-poly --clock 985248 --rate 48000 --seconds 0.01 --note 69 --out sid-chip-poly-fallback-smoke.wav --debug sid-chip-poly-fallback-smoke.json
)
add_test(NAME chipper_render_nes_chip_poly_smoke
    COMMAND chipper_render --chip nes --accuracy authentic --macro manual --play-mode chip-poly --clock 1789773 --rate 48000 --seconds 0.02 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/nes-chip-poly-events.txt --out nes-chip-poly-smoke.wav --debug nes-chip-poly-smoke.json
)
add_test(NAME chipper_render_nes_duty_12_smoke
    COMMAND chipper_render --chip nes --accuracy authentic --macro manual --control1 0.0 --clock 1789773 --rate 48000 --seconds 0.01 --note 69 --out nes-duty-12-smoke.wav --debug nes-duty-12-smoke.json
)
add_test(NAME chipper_render_nes_duty_75_smoke
    COMMAND chipper_render --chip nes --accuracy authentic --macro manual --control1 1.0 --clock 1789773 --rate 48000 --seconds 0.01 --note 69 --out nes-duty-75-smoke.wav --debug nes-duty-75-smoke.json
)
add_test(NAME chipper_render_nes_sources_off_smoke
    COMMAND chipper_render --chip nes --accuracy authentic --macro manual --source1 0 --source2 0 --source3 0 --source4 0 --clock 1789773 --rate 48000 --seconds 0.01 --note 69 --out nes-sources-off-smoke.wav --debug nes-sources-off-smoke.json
)
add_test(NAME chipper_render_nes_envelope_decay_smoke
    COMMAND chipper_render --chip nes --accuracy authentic --macro manual --source3 0 --source4 0 --envelope-decay 1.0 --clock 1789773 --rate 48000 --seconds 0.05 --note 69 --out nes-envelope-decay-smoke.wav --debug nes-envelope-decay-smoke.json
)
add_test(NAME chipper_render_ym2149_smoke
    COMMAND chipper_render --chip ym2149 --accuracy authentic --clock 2000000 --rate 48000 --seconds 0.10 --note 72 --out ym-smoke.wav --debug ym-smoke.json
)
add_test(NAME chipper_render_ym2149_chip_poly_smoke
    COMMAND chipper_render --chip ym2149 --accuracy authentic --macro manual --play-mode chip-poly --clock 2000000 --rate 48000 --seconds 0.02 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/ym2149-chip-poly-events.txt --out ym-chip-poly-smoke.wav --debug ym-chip-poly-smoke.json
)
add_test(NAME chipper_render_ym2149_source_a_off_smoke
    COMMAND chipper_render --chip ym2149 --accuracy authentic --macro manual --source1 0 --clock 2000000 --rate 48000 --seconds 0.03 --note 72 --out ym-source-a-off-smoke.wav --debug ym-source-a-off-smoke.json
)
add_test(NAME chipper_render_ym2149_sources_off_smoke
    COMMAND chipper_render --chip ym2149 --accuracy authentic --macro manual --source1 0 --source2 0 --source3 0 --source4 0 --clock 2000000 --rate 48000 --seconds 0.03 --note 72 --out ym-sources-off-smoke.wav --debug ym-sources-off-smoke.json
)
add_test(NAME chipper_render_ym2149_noise_source_off_smoke
    COMMAND chipper_render --chip ym2149 --accuracy authentic --macro drum --source4 0 --clock 2000000 --rate 48000 --seconds 0.03 --note 72 --out ym-noise-source-off-smoke.wav --debug ym-noise-source-off-smoke.json
)
add_test(NAME chipper_render_ym2149_envelope_fall_smoke
    COMMAND chipper_render --chip ym2149 --accuracy authentic --macro manual --ym-envelope-shape fall --clock 2000000 --rate 48000 --seconds 0.05 --note 72 --out ym-envelope-fall-smoke.wav --debug ym-envelope-fall-smoke.json
)
add_test(NAME chipper_render_ym2149_envelope_triangle_smoke
    COMMAND chipper_render --chip ym2149 --accuracy authentic --macro manual --ym-envelope-shape triangle --clock 2000000 --rate 48000 --seconds 0.05 --note 72 --out ym-envelope-triangle-smoke.wav --debug ym-envelope-triangle-smoke.json
)
add_test(NAME chipper_render_sn76489_smoke
    COMMAND chipper_render --chip sn76489 --accuracy authentic --clock 3579545 --rate 48000 --seconds 0.10 --note 76 --out sn-smoke.wav --debug sn-smoke.json
)
add_test(NAME chipper_render_sn76489_chip_poly_smoke
    COMMAND chipper_render --chip sn76489 --accuracy authentic --macro manual --play-mode chip-poly --clock 3579545 --rate 48000 --seconds 0.02 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/sn76489-chip-poly-events.txt --out sn-chip-poly-smoke.wav --debug sn-chip-poly-smoke.json
)
add_test(NAME chipper_render_sn76489_source0_off_smoke
    COMMAND chipper_render --chip sn76489 --accuracy authentic --macro manual --source1 0 --clock 3579545 --rate 48000 --seconds 0.03 --note 76 --out sn-source0-off-smoke.wav --debug sn-source0-off-smoke.json
)
add_test(NAME chipper_render_sn76489_sources_off_smoke
    COMMAND chipper_render --chip sn76489 --accuracy authentic --macro manual --source1 0 --source2 0 --source3 0 --source4 0 --clock 3579545 --rate 48000 --seconds 0.03 --note 76 --out sn-sources-off-smoke.wav --debug sn-sources-off-smoke.json
)
add_test(NAME chipper_render_sn76489_noise_source_off_smoke
    COMMAND chipper_render --chip sn76489 --accuracy authentic --macro drum --source4 0 --sn-noise-mode white-t3 --clock 3579545 --rate 48000 --seconds 0.03 --note 60 --out sn-noise-source-off-smoke.wav --debug sn-noise-source-off-smoke.json
)
add_test(NAME chipper_render_sn76489_noise_macro_smoke
    COMMAND chipper_render --chip sn76489 --accuracy authentic --macro drum --sn-noise-mode macro --clock 3579545 --rate 48000 --seconds 0.04 --note 60 --out sn-noise-macro-smoke.wav --debug sn-noise-macro-smoke.json
)
add_test(NAME chipper_render_sn76489_noise_periodic_smoke
    COMMAND chipper_render --chip sn76489 --accuracy authentic --macro drum --sn-noise-mode periodic-hi --clock 3579545 --rate 48000 --seconds 0.04 --note 60 --out sn-noise-periodic-smoke.wav --debug sn-noise-periodic-smoke.json
)
add_test(NAME chipper_render_sn76489_noise_white_t3_smoke
    COMMAND chipper_render --chip sn76489 --accuracy authentic --macro drum --sn-noise-mode white-t3 --clock 3579545 --rate 48000 --seconds 0.04 --note 60 --out sn-noise-white-t3-smoke.wav --debug sn-noise-white-t3-smoke.json
)
add_test(NAME chipper_render_preset_nes_hero_smoke
    COMMAND chipper_render --preset nes-hero-pulse --rate 48000 --seconds 0.04 --note 69 --out preset-nes-hero-smoke.wav --debug preset-nes-hero-smoke.json
)
add_test(NAME chipper_render_preset_dmg_wave_bass_smoke
    COMMAND chipper_render --preset dmg-wave-bass --rate 48000 --seconds 0.04 --note 48 --out preset-dmg-wave-bass-smoke.wav --debug preset-dmg-wave-bass-smoke.json
)
add_test(NAME chipper_render_preset_ym_noise_hat_smoke
    COMMAND chipper_render --preset ym-noise-hat --rate 48000 --seconds 0.04 --note 72 --out preset-ym-noise-hat-smoke.wav --debug preset-ym-noise-hat-smoke.json
)
add_test(NAME chipper_render_preset_sn_noise_hit_smoke
    COMMAND chipper_render --preset sn-noise-hit --rate 48000 --seconds 0.04 --note 60 --out preset-sn-noise-hit-smoke.wav --debug preset-sn-noise-hit-smoke.json
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
add_test(NAME chipper_render_dmg_duty_12_smoke
    COMMAND chipper_render --chip dmg --accuracy authentic --macro manual --control1 0.0 --clock 4194304 --rate 48000 --seconds 0.01 --note 69 --out dmg-duty-12-smoke.wav --debug dmg-duty-12-smoke.json
)
add_test(NAME chipper_render_dmg_duty_75_smoke
    COMMAND chipper_render --chip dmg --accuracy authentic --macro manual --control1 1.0 --clock 4194304 --rate 48000 --seconds 0.01 --note 69 --out dmg-duty-75-smoke.wav --debug dmg-duty-75-smoke.json
)
add_test(NAME chipper_render_dmg_sources_off_smoke
    COMMAND chipper_render --chip dmg --accuracy authentic --macro manual --source1 0 --source2 0 --source3 0 --source4 0 --clock 4194304 --rate 48000 --seconds 0.01 --note 69 --out dmg-sources-off-smoke.wav --debug dmg-sources-off-smoke.json
)
add_test(NAME chipper_render_dmg_envelope_decay_smoke
    COMMAND chipper_render --chip dmg --accuracy authentic --macro manual --source3 0 --source4 0 --control4 1.0 --envelope-decay 1.0 --clock 4194304 --rate 48000 --seconds 0.05 --note 69 --out dmg-envelope-decay-smoke.wav --debug dmg-envelope-decay-smoke.json
)
add_test(NAME chipper_render_dmg_wave_triangle_smoke
    COMMAND chipper_render --chip dmg --accuracy authentic --macro bass --source1 0 --source2 0 --source4 0 --wave-shape triangle --clock 4194304 --rate 48000 --seconds 0.04 --note 72 --out dmg-wave-triangle-smoke.wav --debug dmg-wave-triangle-smoke.json
)
add_test(NAME chipper_render_dmg_wave_pulse_smoke
    COMMAND chipper_render --chip dmg --accuracy authentic --macro bass --source1 0 --source2 0 --source4 0 --wave-shape pulse --clock 4194304 --rate 48000 --seconds 0.04 --note 72 --out dmg-wave-pulse-smoke.wav --debug dmg-wave-pulse-smoke.json
)
add_test(NAME chipper_render_dmg_chip_poly_smoke
    COMMAND chipper_render --chip dmg --accuracy authentic --macro manual --play-mode chip-poly --clock 4194304 --rate 48000 --seconds 0.02 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/dmg-chip-poly-events.txt --out dmg-chip-poly-smoke.wav --debug dmg-chip-poly-smoke.json
)
add_test(NAME chipper_render_dmg_nr52_status_bits_smoke
    COMMAND chipper_render --chip dmg --accuracy authentic --macro manual --clock 4194304 --rate 48000 --seconds 0.02 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/dmg-nr52-status-bits-events.txt --out dmg-nr52-status-bits-smoke.wav --debug dmg-nr52-status-bits-smoke.json
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
add_test(NAME chipper_render_nes_dmc_direct_low_smoke
    COMMAND chipper_render --chip nes --accuracy authentic --macro manual --clock 1789773 --rate 48000 --seconds 0.004 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/nes-dmc-direct-load-low-events.txt --out nes-dmc-direct-low-smoke.wav --debug nes-dmc-direct-low-smoke.json
)
add_test(NAME chipper_render_nes_dmc_direct_high_smoke
    COMMAND chipper_render --chip nes --accuracy authentic --macro manual --clock 1789773 --rate 48000 --seconds 0.004 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/nes-dmc-direct-load-high-events.txt --out nes-dmc-direct-high-smoke.wav --debug nes-dmc-direct-high-smoke.json
)
add_test(NAME chipper_render_nes_frame_counter_5step_smoke
    COMMAND chipper_render --chip nes --accuracy authentic --macro manual --clock 1789773 --rate 48000 --seconds 0.001 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/nes-frame-counter-5step-events.txt --out nes-frame-counter-5step-smoke.wav --debug nes-frame-counter-5step-smoke.json
)
add_test(NAME chipper_render_nes_frame_counter_irq_smoke
    COMMAND chipper_render --chip nes --accuracy authentic --macro manual --clock 1789773 --rate 48000 --seconds 0.017 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/nes-frame-counter-irq-events.txt --out nes-frame-counter-irq-smoke.wav --debug nes-frame-counter-irq-smoke.json
)
add_test(NAME chipper_render_nes_frame_counter_inhibit_smoke
    COMMAND chipper_render --chip nes --accuracy authentic --macro manual --clock 1789773 --rate 48000 --seconds 0.017 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/nes-frame-counter-inhibit-events.txt --out nes-frame-counter-inhibit-smoke.wav --debug nes-frame-counter-inhibit-smoke.json
)

if (Python3_Interpreter_FOUND)
    add_test(NAME chipper_render_nes_json_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/nes-smoke.json --implemented-contains partial --macro Manual --min-peak 0.1 --min-rms 0.05 --register-writes 0 --note-events 0 --min-zero-crossings 30 --max-zero-crossings 120
    )
    set_tests_properties(chipper_render_nes_json_assert PROPERTIES DEPENDS chipper_render_nes_smoke)

    add_test(NAME chipper_render_sid_chip_poly_fallback_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sid-chip-poly-fallback-smoke.json --implemented-contains "not implemented" --macro Manual --play-mode "Big Mono" --max-peak 0.001 --max-rms 0.001 --register-writes 0 --note-events 0
    )
    set_tests_properties(chipper_render_sid_chip_poly_fallback_assert PROPERTIES DEPENDS chipper_render_sid_chip_poly_fallback_smoke)

    add_test(NAME chipper_render_nes_chip_poly_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/nes-chip-poly-smoke.json --implemented-contains partial --macro Manual --play-mode "Chip Poly" --register-writes 0 --note-events 3 --min-peak 0.1 --core-min activeChannels=3 --core-max activeChannels=3 --core-min assignedNotePulse1=60 --core-max assignedNotePulse1=60 --core-min assignedNotePulse2=64 --core-max assignedNotePulse2=64 --core-min assignedNoteTriangle=67 --core-max assignedNoteTriangle=67 --core-min pulseTimer1=427 --core-max pulseTimer1=427 --core-min pulseTimer2=338 --core-max pulseTimer2=338 --core-min triangleTimer=142 --core-max triangleTimer=142 --core-min triangleActive=1 --core-max triangleActive=1
    )
    set_tests_properties(chipper_render_nes_chip_poly_assert PROPERTIES DEPENDS chipper_render_nes_chip_poly_smoke)

    add_test(NAME chipper_render_nes_duty_12_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/nes-duty-12-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --min-peak 0.1 --core-min pulseDuty1=0 --core-max pulseDuty1=0 --core-min pulseDuty2=1 --core-max pulseDuty2=1
    )
    set_tests_properties(chipper_render_nes_duty_12_assert PROPERTIES DEPENDS chipper_render_nes_duty_12_smoke)

    add_test(NAME chipper_render_nes_duty_75_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/nes-duty-75-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --min-peak 0.1 --core-min pulseDuty1=3 --core-max pulseDuty1=3 --core-min pulseDuty2=3 --core-max pulseDuty2=3
    )
    set_tests_properties(chipper_render_nes_duty_75_assert PROPERTIES DEPENDS chipper_render_nes_duty_75_smoke)

    add_test(NAME chipper_render_nes_sources_off_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/nes-sources-off-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --max-peak 0.001 --max-rms 0.001 --core-min sourceEnabled1=0 --core-max sourceEnabled1=0 --core-min sourceEnabled2=0 --core-max sourceEnabled2=0 --core-min sourceEnabled3=0 --core-max sourceEnabled3=0 --core-min sourceEnabled4=0 --core-max sourceEnabled4=0 --core-min enabled0=0 --core-max enabled0=0 --core-min enabled1=0 --core-max enabled1=0 --core-min enabled2=0 --core-max enabled2=0 --core-min enabled3=0 --core-max enabled3=0
    )
    set_tests_properties(chipper_render_nes_sources_off_assert PROPERTIES DEPENDS chipper_render_nes_sources_off_smoke)

    add_test(NAME chipper_render_nes_envelope_decay_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/nes-envelope-decay-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --min-peak 0.1 --core-min envelopeDecayControl=1 --core-max envelopeDecayControl=1 --core-min envelopePeriod0=1 --core-max envelopePeriod0=1 --core-min envelopeConstant0=0 --core-max envelopeConstant0=0
    )
    set_tests_properties(chipper_render_nes_envelope_decay_assert PROPERTIES DEPENDS chipper_render_nes_envelope_decay_smoke)

    add_test(NAME chipper_render_ym2149_chip_poly_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/ym-chip-poly-smoke.json --implemented-contains partial --macro Manual --play-mode "Chip Poly" --register-writes 0 --note-events 3 --min-peak 0.1 --core-min activeChannels=3 --core-max activeChannels=3 --core-min assignedNoteA=60 --core-max assignedNoteA=60 --core-min assignedNoteB=64 --core-max assignedNoteB=64 --core-min assignedNoteC=67 --core-max assignedNoteC=67 --core-min periodA=477 --core-max periodA=479 --core-min periodB=378 --core-max periodB=380 --core-min periodC=318 --core-max periodC=320
    )
    set_tests_properties(chipper_render_ym2149_chip_poly_assert PROPERTIES DEPENDS chipper_render_ym2149_chip_poly_smoke)

    add_test(NAME chipper_render_ym2149_source_a_off_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/ym-source-a-off-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --min-peak 0.05 --core-min sourceEnabledA=0 --core-max sourceEnabledA=0 --core-min sourceEnabledB=1 --core-max sourceEnabledB=1 --core-min sourceEnabledC=1 --core-max sourceEnabledC=1 --core-min sourceEnabledNoise=1 --core-max sourceEnabledNoise=1 --core-min mixer=57 --core-max mixer=57 --core-min volumeA=0 --core-max volumeA=0 --core-min volumeB=10 --core-max volumeB=10 --core-min volumeC=8 --core-max volumeC=8
    )
    set_tests_properties(chipper_render_ym2149_source_a_off_assert PROPERTIES DEPENDS chipper_render_ym2149_source_a_off_smoke)

    add_test(NAME chipper_render_ym2149_sources_off_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/ym-sources-off-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --max-peak 0.001 --max-rms 0.001 --core-min sourceEnabledA=0 --core-max sourceEnabledA=0 --core-min sourceEnabledB=0 --core-max sourceEnabledB=0 --core-min sourceEnabledC=0 --core-max sourceEnabledC=0 --core-min sourceEnabledNoise=0 --core-max sourceEnabledNoise=0 --core-min mixer=63 --core-max mixer=63 --core-min volumeA=0 --core-max volumeA=0 --core-min volumeB=0 --core-max volumeB=0 --core-min volumeC=0 --core-max volumeC=0
    )
    set_tests_properties(chipper_render_ym2149_sources_off_assert PROPERTIES DEPENDS chipper_render_ym2149_sources_off_smoke)

    add_test(NAME chipper_render_ym2149_noise_source_off_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/ym-noise-source-off-smoke.json --implemented-contains partial --macro Drum --register-writes 0 --note-events 0 --max-peak 0.001 --max-rms 0.001 --core-min sourceEnabledNoise=0 --core-max sourceEnabledNoise=0 --core-min mixer=63 --core-max mixer=63 --core-min volumeA=0 --core-max volumeA=0 --core-min volumeB=0 --core-max volumeB=0 --core-min volumeC=0 --core-max volumeC=0
    )
    set_tests_properties(chipper_render_ym2149_noise_source_off_assert PROPERTIES DEPENDS chipper_render_ym2149_noise_source_off_smoke)

    add_test(NAME chipper_render_ym2149_envelope_fall_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/ym-envelope-fall-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --min-peak 0.02 --core-min envelopeShapeChoice=1 --core-max envelopeShapeChoice=1 --core-min envelopeShape=9 --core-max envelopeShape=9 --core-min envelopeEnabledA=1 --core-max envelopeEnabledA=1 --core-min envelopeEnabledB=1 --core-max envelopeEnabledB=1 --core-min envelopeEnabledC=1 --core-max envelopeEnabledC=1 --core-min volumeA=16 --core-max volumeA=16
    )
    set_tests_properties(chipper_render_ym2149_envelope_fall_assert PROPERTIES DEPENDS chipper_render_ym2149_envelope_fall_smoke)

    add_test(NAME chipper_render_ym2149_envelope_triangle_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/ym-envelope-triangle-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --min-peak 0.02 --core-min envelopeShapeChoice=4 --core-max envelopeShapeChoice=4 --core-min envelopeShape=14 --core-max envelopeShape=14 --core-min envelopeEnabledA=1 --core-max envelopeEnabledA=1 --core-min envelopeEnabledB=1 --core-max envelopeEnabledB=1 --core-min envelopeEnabledC=1 --core-max envelopeEnabledC=1 --core-min envelopePeriod=640 --core-max envelopePeriod=640
    )
    set_tests_properties(chipper_render_ym2149_envelope_triangle_assert PROPERTIES DEPENDS chipper_render_ym2149_envelope_triangle_smoke)

    add_test(NAME chipper_render_sn76489_chip_poly_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sn-chip-poly-smoke.json --implemented-contains partial --macro Manual --play-mode "Chip Poly" --register-writes 0 --note-events 3 --min-peak 0.1 --core-min activeChannels=3 --core-max activeChannels=3 --core-min assignedNote0=60 --core-max assignedNote0=60 --core-min assignedNote1=64 --core-max assignedNote1=64 --core-min assignedNote2=67 --core-max assignedNote2=67 --core-min period0=428 --core-max period0=428 --core-min period1=339 --core-max period1=339 --core-min period2=285 --core-max period2=285
    )
    set_tests_properties(chipper_render_sn76489_chip_poly_assert PROPERTIES DEPENDS chipper_render_sn76489_chip_poly_smoke)

    add_test(NAME chipper_render_sn76489_source0_off_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sn-source0-off-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --min-peak 0.03 --core-min sourceEnabled0=0 --core-max sourceEnabled0=0 --core-min sourceEnabled1=1 --core-max sourceEnabled1=1 --core-min sourceEnabled2=1 --core-max sourceEnabled2=1 --core-min sourceEnabledNoise=1 --core-max sourceEnabledNoise=1 --core-min attenuation0=15 --core-max attenuation0=15 --core-min attenuation1=7 --core-max attenuation1=7 --core-min attenuation2=9 --core-max attenuation2=9 --core-min attenuationNoise=8 --core-max attenuationNoise=8
    )
    set_tests_properties(chipper_render_sn76489_source0_off_assert PROPERTIES DEPENDS chipper_render_sn76489_source0_off_smoke)

    add_test(NAME chipper_render_sn76489_sources_off_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sn-sources-off-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --max-peak 0.001 --max-rms 0.001 --core-min sourceEnabled0=0 --core-max sourceEnabled0=0 --core-min sourceEnabled1=0 --core-max sourceEnabled1=0 --core-min sourceEnabled2=0 --core-max sourceEnabled2=0 --core-min sourceEnabledNoise=0 --core-max sourceEnabledNoise=0 --core-min attenuation0=15 --core-max attenuation0=15 --core-min attenuation1=15 --core-max attenuation1=15 --core-min attenuation2=15 --core-max attenuation2=15 --core-min attenuationNoise=15 --core-max attenuationNoise=15
    )
    set_tests_properties(chipper_render_sn76489_sources_off_assert PROPERTIES DEPENDS chipper_render_sn76489_sources_off_smoke)

    add_test(NAME chipper_render_sn76489_noise_source_off_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sn-noise-source-off-smoke.json --implemented-contains partial --macro Drum --register-writes 0 --note-events 0 --max-peak 0.001 --max-rms 0.001 --core-min sourceEnabledNoise=0 --core-max sourceEnabledNoise=0 --core-min noiseModeChoice=4 --core-max noiseModeChoice=4 --core-min noiseControl=7 --core-max noiseControl=7 --core-min attenuation0=15 --core-max attenuation0=15 --core-min attenuation1=15 --core-max attenuation1=15 --core-min attenuation2=15 --core-max attenuation2=15 --core-min attenuationNoise=15 --core-max attenuationNoise=15
    )
    set_tests_properties(chipper_render_sn76489_noise_source_off_assert PROPERTIES DEPENDS chipper_render_sn76489_noise_source_off_smoke)

    add_test(NAME chipper_render_sn76489_noise_macro_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sn-noise-macro-smoke.json --implemented-contains partial --macro Drum --register-writes 0 --note-events 0 --min-peak 0.05 --core-min noiseModeChoice=0 --core-max noiseModeChoice=0 --core-min noiseControl=4 --core-max noiseControl=4 --core-min noiseWhite=1 --core-max noiseWhite=1 --core-min noiseRate=0 --core-max noiseRate=0 --core-min attenuationNoise=2 --core-max attenuationNoise=2
    )
    set_tests_properties(chipper_render_sn76489_noise_macro_assert PROPERTIES DEPENDS chipper_render_sn76489_noise_macro_smoke)

    add_test(NAME chipper_render_sn76489_noise_periodic_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sn-noise-periodic-smoke.json --implemented-contains partial --macro Drum --register-writes 0 --note-events 0 --min-peak 0.05 --core-min noiseModeChoice=2 --core-max noiseModeChoice=2 --core-min noiseControl=2 --core-max noiseControl=2 --core-min noiseWhite=0 --core-max noiseWhite=0 --core-min noiseRate=2 --core-max noiseRate=2 --core-min attenuationNoise=2 --core-max attenuationNoise=2
    )
    set_tests_properties(chipper_render_sn76489_noise_periodic_assert PROPERTIES DEPENDS chipper_render_sn76489_noise_periodic_smoke)

    add_test(NAME chipper_render_sn76489_noise_white_t3_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sn-noise-white-t3-smoke.json --implemented-contains partial --macro Drum --register-writes 0 --note-events 0 --min-peak 0.05 --core-min noiseModeChoice=4 --core-max noiseModeChoice=4 --core-min noiseControl=7 --core-max noiseControl=7 --core-min noiseWhite=1 --core-max noiseWhite=1 --core-min noiseRate=3 --core-max noiseRate=3 --core-min attenuationNoise=2 --core-max attenuationNoise=2
    )
    set_tests_properties(chipper_render_sn76489_noise_white_t3_assert PROPERTIES DEPENDS chipper_render_sn76489_noise_white_t3_smoke)

    add_test(NAME chipper_render_preset_nes_hero_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-nes-hero-smoke.json --implemented-contains partial --preset nes-hero-pulse --macro Lead --play-mode "Big Mono" --register-writes 0 --note-events 0 --min-peak 0.1 --core-min pulseDuty1=2 --core-max pulseDuty1=2 --core-min sourceEnabled1=1 --core-max sourceEnabled1=1 --core-min sourceEnabled2=1 --core-max sourceEnabled2=1 --core-min sourceEnabled3=1 --core-max sourceEnabled3=1 --core-min sourceEnabled4=0 --core-max sourceEnabled4=0
    )
    set_tests_properties(chipper_render_preset_nes_hero_assert PROPERTIES DEPENDS chipper_render_preset_nes_hero_smoke)

    add_test(NAME chipper_render_preset_dmg_wave_bass_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-dmg-wave-bass-smoke.json --implemented-contains partial --preset dmg-wave-bass --macro Bass --play-mode "Big Mono" --register-writes 0 --note-events 0 --min-peak 0.1 --core-min sourceEnabled1=0 --core-max sourceEnabled1=0 --core-min sourceEnabled2=0 --core-max sourceEnabled2=0 --core-min sourceEnabled3=1 --core-max sourceEnabled3=1 --core-min sourceEnabled4=0 --core-max sourceEnabled4=0 --core-min waveShape=1 --core-max waveShape=1 --core-min enabledWave=1 --core-max enabledWave=1
    )
    set_tests_properties(chipper_render_preset_dmg_wave_bass_assert PROPERTIES DEPENDS chipper_render_preset_dmg_wave_bass_smoke)

    add_test(NAME chipper_render_preset_ym_noise_hat_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-ym-noise-hat-smoke.json --implemented-contains partial --preset ym-noise-hat --macro Drum --play-mode "Big Mono" --register-writes 0 --note-events 0 --min-peak 0.1 --min-zero-crossings 100 --core-min sourceEnabledA=1 --core-max sourceEnabledA=1 --core-min sourceEnabledB=0 --core-max sourceEnabledB=0 --core-min sourceEnabledC=0 --core-max sourceEnabledC=0 --core-min sourceEnabledNoise=1 --core-max sourceEnabledNoise=1 --core-min mixer=55 --core-max mixer=55 --core-min envelopeShapeChoice=1 --core-max envelopeShapeChoice=1
    )
    set_tests_properties(chipper_render_preset_ym_noise_hat_assert PROPERTIES DEPENDS chipper_render_preset_ym_noise_hat_smoke)

    add_test(NAME chipper_render_preset_sn_noise_hit_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-sn-noise-hit-smoke.json --implemented-contains partial --preset sn-noise-hit --macro Drum --play-mode "Big Mono" --register-writes 0 --note-events 0 --min-peak 0.05 --min-zero-crossings 50 --core-min sourceEnabled0=0 --core-max sourceEnabled0=0 --core-min sourceEnabled1=0 --core-max sourceEnabled1=0 --core-min sourceEnabled2=0 --core-max sourceEnabled2=0 --core-min sourceEnabledNoise=1 --core-max sourceEnabledNoise=1 --core-min noiseModeChoice=3 --core-max noiseModeChoice=3 --core-min noiseControl=4 --core-max noiseControl=4 --core-min attenuationNoise=2 --core-max attenuationNoise=2
    )
    set_tests_properties(chipper_render_preset_sn_noise_hit_assert PROPERTIES DEPENDS chipper_render_preset_sn_noise_hit_smoke)

    add_test(NAME chipper_render_dmg_json_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/dmg-smoke.json --implemented-contains partial --macro Lead --min-peak 0.1 --min-rms 0.03 --register-writes 0 --note-events 0 --min-zero-crossings 30 --max-zero-crossings 80 --core-min pulse1FreqReg=800 --core-max pulse1FreqReg=860 --core-min sweepEnabled=1 --core-max sweepEnabled=1 --core-min enabled0=1 --core-max enabled0=1 --core-min enabledNoise=0 --core-max enabledNoise=0
    )
    set_tests_properties(chipper_render_dmg_json_assert PROPERTIES DEPENDS chipper_render_dmg_smoke)

    add_test(NAME chipper_render_dmg_duty_12_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/dmg-duty-12-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --min-peak 0.1 --core-min pulseDuty1=0 --core-max pulseDuty1=0 --core-min pulseDuty2=1 --core-max pulseDuty2=1
    )
    set_tests_properties(chipper_render_dmg_duty_12_assert PROPERTIES DEPENDS chipper_render_dmg_duty_12_smoke)

    add_test(NAME chipper_render_dmg_duty_75_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/dmg-duty-75-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --min-peak 0.1 --core-min pulseDuty1=3 --core-max pulseDuty1=3 --core-min pulseDuty2=3 --core-max pulseDuty2=3
    )
    set_tests_properties(chipper_render_dmg_duty_75_assert PROPERTIES DEPENDS chipper_render_dmg_duty_75_smoke)

    add_test(NAME chipper_render_dmg_sources_off_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/dmg-sources-off-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --max-peak 0.001 --max-rms 0.001 --core-min sourceEnabled1=0 --core-max sourceEnabled1=0 --core-min sourceEnabled2=0 --core-max sourceEnabled2=0 --core-min sourceEnabled3=0 --core-max sourceEnabled3=0 --core-min sourceEnabled4=0 --core-max sourceEnabled4=0 --core-min enabled0=0 --core-max enabled0=0 --core-min enabled1=0 --core-max enabled1=0 --core-min enabledWave=0 --core-max enabledWave=0 --core-min enabledNoise=0 --core-max enabledNoise=0
    )
    set_tests_properties(chipper_render_dmg_sources_off_assert PROPERTIES DEPENDS chipper_render_dmg_sources_off_smoke)

    add_test(NAME chipper_render_dmg_envelope_decay_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/dmg-envelope-decay-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --min-peak 0.1 --core-min envelopeDecayControl=1 --core-max envelopeDecayControl=1 --core-min envelopePeriod0=1 --core-max envelopePeriod0=1 --core-min envelopeDirection0=0 --core-max envelopeDirection0=0 --core-min initialEnvelope0=15 --core-max initialEnvelope0=15
    )
    set_tests_properties(chipper_render_dmg_envelope_decay_assert PROPERTIES DEPENDS chipper_render_dmg_envelope_decay_smoke)

    add_test(NAME chipper_render_dmg_wave_triangle_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/dmg-wave-triangle-smoke.json --implemented-contains partial --macro Bass --register-writes 0 --note-events 0 --min-peak 0.03 --core-min waveShape=1 --core-max waveShape=1 --core-min waveRam0=1 --core-max waveRam0=1 --core-min waveRam15=16 --core-max waveRam15=16 --core-min enabledWave=1 --core-max enabledWave=1
    )
    set_tests_properties(chipper_render_dmg_wave_triangle_assert PROPERTIES DEPENDS chipper_render_dmg_wave_triangle_smoke)

    add_test(NAME chipper_render_dmg_wave_pulse_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/dmg-wave-pulse-smoke.json --implemented-contains partial --macro Bass --register-writes 0 --note-events 0 --min-peak 0.03 --core-min waveShape=3 --core-max waveShape=3 --core-min waveRam0=255 --core-max waveRam0=255 --core-min waveRam15=0 --core-max waveRam15=0 --core-min enabledWave=1 --core-max enabledWave=1
    )
    set_tests_properties(chipper_render_dmg_wave_pulse_assert PROPERTIES DEPENDS chipper_render_dmg_wave_pulse_smoke)

    add_test(NAME chipper_render_dmg_chip_poly_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/dmg-chip-poly-smoke.json --implemented-contains partial --macro Manual --play-mode "Chip Poly" --register-writes 0 --note-events 3 --min-peak 0.1 --core-min activeChannels=3 --core-max activeChannels=3 --core-min assignedNotePulse1=60 --core-max assignedNotePulse1=60 --core-min assignedNotePulse2=64 --core-max assignedNotePulse2=64 --core-min assignedNoteWave=67 --core-max assignedNoteWave=67 --core-min pulse1FreqReg=1547 --core-max pulse1FreqReg=1547 --core-min pulse2FreqReg=1650 --core-max pulse2FreqReg=1650 --core-min waveFreqReg=1881 --core-max waveFreqReg=1881 --core-min enabled0=1 --core-max enabled0=1 --core-min enabled1=1 --core-max enabled1=1 --core-min enabledWave=1 --core-max enabledWave=1 --core-min enabledNoise=0 --core-max enabledNoise=0
    )
    set_tests_properties(chipper_render_dmg_chip_poly_assert PROPERTIES DEPENDS chipper_render_dmg_chip_poly_smoke)

    add_test(NAME chipper_render_dmg_nr52_status_bits_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/dmg-nr52-status-bits-smoke.json --implemented-contains partial --macro Manual --register-writes 8 --note-events 1 --min-peak 0.1 --core-min enabled1=1 --core-max enabled1=1 --core-min pulse2FreqReg=1024 --core-max pulse2FreqReg=1024
    )
    set_tests_properties(chipper_render_dmg_nr52_status_bits_assert PROPERTIES DEPENDS chipper_render_dmg_nr52_status_bits_smoke)

    add_test(NAME chipper_render_dmg_noise_envelope_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/dmg-noise-envelope-smoke.json --implemented-contains partial --macro Manual --min-peak 0.1 --min-rms 0.1 --register-writes 8 --note-events 1 --min-zero-crossings 3000 --max-zero-crossings 3350 --core-min noiseClockHz=32767 --core-max noiseClockHz=32769 --core-min noiseShift=2 --core-max noiseShift=2 --core-min noiseDivisorCode=2 --core-max noiseDivisorCode=2 --core-min envelopeNoise=2 --core-max envelopeNoise=4 --core-min lengthNoise=10 --core-max lengthNoise=16 --core-min enabledNoise=1 --core-max enabledNoise=1
    )
    set_tests_properties(chipper_render_dmg_noise_envelope_assert PROPERTIES DEPENDS chipper_render_dmg_noise_envelope_smoke)

    add_test(NAME chipper_render_dmg_noise_divisor0_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/dmg-noise-divisor0-smoke.json --implemented-contains partial --macro Manual --min-rms 0.1 --register-writes 7 --note-events 1 --core-min noiseClockHz=524287 --core-max noiseClockHz=524289 --core-min noiseShift=0 --core-max noiseShift=0 --core-min noiseDivisorCode=0 --core-max noiseDivisorCode=0 --core-min enabledNoise=1 --core-max enabledNoise=1
    )
    set_tests_properties(chipper_render_dmg_noise_divisor0_assert PROPERTIES DEPENDS chipper_render_dmg_noise_divisor0_smoke)

    add_test(NAME chipper_render_dmg_noise_shift_stop_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/dmg-noise-shift-stop-smoke.json --implemented-contains partial --macro Manual --min-rms 0.1 --register-writes 7 --note-events 1 --max-zero-crossings 1 --core-min noiseClockHz=0 --core-max noiseClockHz=0 --core-min noiseShift=14 --core-max noiseShift=14 --core-min noiseDivisorCode=2 --core-max noiseDivisorCode=2 --core-min enabledNoise=1 --core-max enabledNoise=1
    )
    set_tests_properties(chipper_render_dmg_noise_shift_stop_assert PROPERTIES DEPENDS chipper_render_dmg_noise_shift_stop_smoke)

    add_test(NAME chipper_render_dmg_power_wave_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/dmg-power-wave-smoke.json --implemented-contains partial --macro Manual --register-writes 2 --note-events 1 --core-min waveRam0=171 --core-max waveRam0=171 --core-min enabled0=0 --core-max enabled0=0 --core-min enabledNoise=0 --core-max enabledNoise=0
    )
    set_tests_properties(chipper_render_dmg_power_wave_assert PROPERTIES DEPENDS chipper_render_dmg_power_wave_smoke)

    add_test(NAME chipper_render_dmg_sweep_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/dmg-sweep-smoke.json --implemented-contains partial --macro Manual --min-peak 0.1 --register-writes 9 --note-events 1 --core-min pulse1FreqReg=255 --core-max pulse1FreqReg=257 --core-min sweepShadow=255 --core-max sweepShadow=257 --core-min sweepEnabled=1 --core-max sweepEnabled=1 --core-min enabled0=1 --core-max enabled0=1
    )
    set_tests_properties(chipper_render_dmg_sweep_assert PROPERTIES DEPENDS chipper_render_dmg_sweep_smoke)

    add_test(NAME chipper_render_dmg_sweep_overflow_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/dmg-sweep-overflow-smoke.json --implemented-contains partial --macro Manual --register-writes 9 --note-events 1 --core-min pulse1FreqReg=1792 --core-max pulse1FreqReg=1792 --core-min sweepShadow=1792 --core-max sweepShadow=1792 --core-min sweepEnabled=1 --core-max sweepEnabled=1 --core-min enabled0=0 --core-max enabled0=0
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

    add_test(NAME chipper_render_nes_dmc_direct_low_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/nes-dmc-direct-low-smoke.json --implemented-contains partial --macro Manual --register-writes 2 --note-events 0 --max-peak 0.001 --max-rms 0.001 --core-min dmcLevel=0 --core-max dmcLevel=0
    )
    set_tests_properties(chipper_render_nes_dmc_direct_low_assert PROPERTIES DEPENDS chipper_render_nes_dmc_direct_low_smoke)

    add_test(NAME chipper_render_nes_dmc_direct_high_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/nes-dmc-direct-high-smoke.json --implemented-contains partial --macro Manual --register-writes 2 --note-events 0 --min-peak 0.65 --max-peak 0.95 --core-min dmcLevel=127 --core-max dmcLevel=127
    )
    set_tests_properties(chipper_render_nes_dmc_direct_high_assert PROPERTIES DEPENDS chipper_render_nes_dmc_direct_high_smoke)

    add_test(NAME chipper_render_nes_frame_counter_5step_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/nes-frame-counter-5step-smoke.json --implemented-contains partial --macro Manual --register-writes 5 --note-events 0 --core-min frameCounterFiveStep=1 --core-max frameCounterFiveStep=1 --core-min frameIrqInhibit=0 --core-max frameIrqInhibit=0 --core-min frameIrqFlag=0 --core-max frameIrqFlag=0 --core-min frameSequenceStep=0 --core-max frameSequenceStep=0 --core-min linearCounter=2 --core-max linearCounter=2 --core-min linearReloadFlag=0 --core-max linearReloadFlag=0 --core-min lengthTriangle=9 --core-max lengthTriangle=9
    )
    set_tests_properties(chipper_render_nes_frame_counter_5step_assert PROPERTIES DEPENDS chipper_render_nes_frame_counter_5step_smoke)

    add_test(NAME chipper_render_nes_frame_counter_irq_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/nes-frame-counter-irq-smoke.json --implemented-contains partial --macro Manual --register-writes 1 --note-events 0 --core-min frameCounterFiveStep=0 --core-max frameCounterFiveStep=0 --core-min frameIrqInhibit=0 --core-max frameIrqInhibit=0 --core-min frameIrqFlag=1 --core-max frameIrqFlag=1
    )
    set_tests_properties(chipper_render_nes_frame_counter_irq_assert PROPERTIES DEPENDS chipper_render_nes_frame_counter_irq_smoke)

    add_test(NAME chipper_render_nes_frame_counter_inhibit_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/nes-frame-counter-inhibit-smoke.json --implemented-contains partial --macro Manual --register-writes 1 --note-events 0 --core-min frameCounterFiveStep=0 --core-max frameCounterFiveStep=0 --core-min frameIrqInhibit=1 --core-max frameIrqInhibit=1 --core-min frameIrqFlag=0 --core-max frameIrqFlag=0
    )
    set_tests_properties(chipper_render_nes_frame_counter_inhibit_assert PROPERTIES DEPENDS chipper_render_nes_frame_counter_inhibit_smoke)
endif()

if (NOT CHIPPER_BUILD_PLUGIN)
    return()
endif()

set(CHIPPER_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(CHIPPER_BUILD_INFO_HEADER "${CHIPPER_GENERATED_DIR}/ChipperBuildInfo.h")
file(MAKE_DIRECTORY "${CHIPPER_GENERATED_DIR}")

execute_process(
    COMMAND ${CMAKE_COMMAND}
        -DOUTPUT_FILE=${CHIPPER_BUILD_INFO_HEADER}
        -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -DCHIPPER_VERSION=${PROJECT_VERSION}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/WriteBuildInfo.cmake
)

add_custom_target(chipper_build_info
    COMMAND ${CMAKE_COMMAND}
        -DOUTPUT_FILE=${CHIPPER_BUILD_INFO_HEADER}
        -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -DCHIPPER_VERSION=${PROJECT_VERSION}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/WriteBuildInfo.cmake
    BYPRODUCTS "${CHIPPER_BUILD_INFO_HEADER}"
    VERBATIM
)

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

add_dependencies(Chipper chipper_build_info)

target_sources(Chipper
    PRIVATE
        Source/Parameters.cpp
        Source/Parameters.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
)

target_include_directories(Chipper PRIVATE Source "${CHIPPER_GENERATED_DIR}")

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
