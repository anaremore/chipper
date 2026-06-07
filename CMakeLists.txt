cmake_minimum_required(VERSION 3.22)

project(Chipper VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CHIPPER_BUILD_PLUGIN "Build the JUCE VST3 plugin" ON)
find_package(Python3 COMPONENTS Interpreter)

add_library(chipper_engine STATIC
    Source/Engine/ControlRegistry.cpp
    Source/Engine/ControlRegistry.h
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

add_executable(chipper_descriptor_smoke tests/descriptor_smoke.cpp)
target_link_libraries(chipper_descriptor_smoke PRIVATE chipper_engine)

enable_testing()
add_test(NAME chipper_descriptor_smoke
    COMMAND chipper_descriptor_smoke
)
add_test(NAME chipper_render_nes_smoke
    COMMAND chipper_render --chip nes --accuracy authentic --clock 1789773 --rate 48000 --seconds 0.10 --note 69 --out nes-smoke.wav --debug nes-smoke.json
)
add_test(NAME chipper_render_nes_output_trim_smoke
    COMMAND chipper_render --chip nes --accuracy authentic --clock 1789773 --rate 48000 --seconds 0.04 --note 69 --output-db -24 --out nes-output-trim-smoke.wav --debug nes-output-trim-smoke.json
)
add_test(NAME chipper_render_sid_smoke
    COMMAND chipper_render --chip sid --accuracy authentic --macro lead --clock 985248 --rate 48000 --seconds 0.06 --note 69 --out sid-smoke.wav --debug sid-smoke.json
)
add_test(NAME chipper_render_sid_chip_poly_smoke
    COMMAND chipper_render --chip sid --accuracy authentic --macro manual --play-mode chip-poly --clock 985248 --rate 48000 --seconds 0.06 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/sid-chip-poly-events.txt --out sid-chip-poly-smoke.wav --debug sid-chip-poly-smoke.json
)
add_test(NAME chipper_render_sid_wave_noise_smoke
    COMMAND chipper_render --chip sid --accuracy authentic --macro manual --wave-shape noise --source1 0 --source2 0 --source3 1 --clock 985248 --rate 48000 --seconds 0.03 --note 69 --out sid-wave-noise-smoke.wav --debug sid-wave-noise-smoke.json
)
add_test(NAME chipper_render_sid_voice_waves_smoke
    COMMAND chipper_render --chip sid --accuracy authentic --macro manual --wave-shape pulse --sid-voice2-wave saw --sid-voice3-wave noise --clock 985248 --rate 48000 --seconds 0.04 --note 69 --out sid-voice-waves-smoke.wav --debug sid-voice-waves-smoke.json
)
add_test(NAME chipper_render_sid_combined_waves_smoke
    COMMAND chipper_render --chip sid --accuracy authentic --macro manual --wave-shape tri+saw --sid-voice2-wave saw+pulse --sid-voice3-wave tri+saw+pulse --clock 985248 --rate 48000 --seconds 0.04 --note 69 --out sid-combined-waves-smoke.wav --debug sid-combined-waves-smoke.json
)
add_test(NAME chipper_render_sid_voice3_big_mono_smoke
    COMMAND chipper_render --chip sid --accuracy authentic --macro bass --source1 0 --source2 0 --source3 1 --level3 1.0 --wave-shape pulse --sid-voice3-wave saw --sid-filter-mode off --sid-filter-routing none --clock 985248 --rate 48000 --seconds 0.05 --note 69 --out sid-voice3-big-mono-smoke.wav --debug sid-voice3-big-mono-smoke.json
)
add_test(NAME chipper_render_sid_per_voice_pulse_width_smoke
    COMMAND chipper_render --chip sid --accuracy authentic --macro manual --control1 0.5 --sid-voice2-pulse-width 0.25 --sid-voice3-pulse-width 0.75 --wave-shape pulse --sid-voice2-wave pulse --sid-voice3-wave pulse --clock 985248 --rate 48000 --seconds 0.04 --note 69 --out sid-per-voice-pw-smoke.wav --debug sid-per-voice-pw-smoke.json
)
add_test(NAME chipper_render_sid_mod_sync_smoke
    COMMAND chipper_render --chip sid --accuracy authentic --macro manual --wave-shape saw --sid-mod-mode sync --clock 985248 --rate 48000 --seconds 0.04 --note 69 --out sid-mod-sync-smoke.wav --debug sid-mod-sync-smoke.json
)
add_test(NAME chipper_render_sid_mod_ring_smoke
    COMMAND chipper_render --chip sid --accuracy authentic --macro manual --wave-shape tri --sid-mod-mode ring --clock 985248 --rate 48000 --seconds 0.04 --note 69 --out sid-mod-ring-smoke.wav --debug sid-mod-ring-smoke.json
)
add_test(NAME chipper_render_sid_model_6581_smoke
    COMMAND chipper_render --chip sid --accuracy authentic --macro manual --sid-model 6581 --wave-shape pulse --sid-filter-mode lp --control3 0.65 --stereo-spread 0.7 --clock 985248 --rate 48000 --seconds 0.04 --note 69 --out sid-model-6581-smoke.wav --debug sid-model-6581-smoke.json
)
add_test(NAME chipper_render_sid_model_8580_smoke
    COMMAND chipper_render --chip sid --accuracy authentic --macro manual --sid-model 8580 --wave-shape pulse --sid-filter-mode lp --control3 0.65 --stereo-spread 0.7 --clock 985248 --rate 48000 --seconds 0.04 --note 69 --out sid-model-8580-smoke.wav --debug sid-model-8580-smoke.json
)
add_test(NAME chipper_render_sid_adsr_fast_smoke
    COMMAND chipper_render --chip sid --accuracy authentic --macro manual --wave-shape pulse --sid-adsr-speed 1.0 --control4 0.8 --clock 985248 --rate 48000 --seconds 0.04 --note 69 --out sid-adsr-fast-smoke.wav --debug sid-adsr-fast-smoke.json
)
add_test(NAME chipper_render_sid_adsr_nibbles_smoke
    COMMAND chipper_render --chip sid --accuracy authentic --macro manual --wave-shape pulse --sid-adsr-speed 0.5 --sid-attack 15 --sid-decay 3 --sid-sustain 12 --sid-release 0 --sid-voice2-attack 6 --sid-voice2-decay 2 --sid-voice2-sustain 9 --sid-voice2-release 4 --sid-voice3-attack 1 --sid-voice3-decay 5 --sid-voice3-sustain 3 --sid-voice3-release 8 --control4 0.5 --clock 985248 --rate 48000 --seconds 0.04 --note 69 --out sid-adsr-nibbles-smoke.wav --debug sid-adsr-nibbles-smoke.json
)
add_test(NAME chipper_render_sid_pulse_width_min_smoke
    COMMAND chipper_render --chip sid --accuracy authentic --macro manual --control1 0.0 --wave-shape pulse --clock 985248 --rate 48000 --seconds 0.03 --note 69 --out sid-pulse-width-min-smoke.wav --debug sid-pulse-width-min-smoke.json
)
add_test(NAME chipper_render_sid_pulse_width_max_smoke
    COMMAND chipper_render --chip sid --accuracy authentic --macro manual --control1 1.0 --wave-shape pulse --clock 985248 --rate 48000 --seconds 0.03 --note 69 --out sid-pulse-width-max-smoke.wav --debug sid-pulse-width-max-smoke.json
)
add_test(NAME chipper_render_sid_sources_off_smoke
    COMMAND chipper_render --chip sid --accuracy authentic --macro manual --source1 0 --source2 0 --source3 0 --clock 985248 --rate 48000 --seconds 0.03 --note 69 --out sid-sources-off-smoke.wav --debug sid-sources-off-smoke.json
)
add_test(NAME chipper_render_sid_filter_bp_smoke
    COMMAND chipper_render --chip sid --accuracy authentic --macro manual --wave-shape pulse --sid-filter-mode bp --stereo-spread 1.0 --control3 0.55 --clock 985248 --rate 48000 --seconds 0.04 --note 69 --out sid-filter-bp-smoke.wav --debug sid-filter-bp-smoke.json
)
add_test(NAME chipper_render_sid_filter_bypass_smoke
    COMMAND chipper_render --chip sid --accuracy authentic --macro manual --wave-shape pulse --sid-filter-mode bypass --stereo-spread 0.0 --control3 0.0 --clock 985248 --rate 48000 --seconds 0.04 --note 69 --out sid-filter-bypass-smoke.wav --debug sid-filter-bypass-smoke.json
)
add_test(NAME chipper_render_sid_filter_notch_smoke
    COMMAND chipper_render --chip sid --accuracy authentic --macro manual --wave-shape pulse --sid-filter-mode notch --stereo-spread 1.0 --control3 0.55 --clock 985248 --rate 48000 --seconds 0.04 --note 69 --out sid-filter-notch-smoke.wav --debug sid-filter-notch-smoke.json
)
add_test(NAME chipper_render_sid_filter_lpbp_smoke
    COMMAND chipper_render --chip sid --accuracy authentic --macro manual --wave-shape pulse --sid-filter-mode lp+bp --stereo-spread 1.0 --control3 0.55 --clock 985248 --rate 48000 --seconds 0.04 --note 69 --out sid-filter-lpbp-smoke.wav --debug sid-filter-lpbp-smoke.json
)
add_test(NAME chipper_render_sid_filter_bphp_smoke
    COMMAND chipper_render --chip sid --accuracy authentic --macro manual --wave-shape pulse --sid-filter-mode bp+hp --stereo-spread 1.0 --control3 0.55 --clock 985248 --rate 48000 --seconds 0.04 --note 69 --out sid-filter-bphp-smoke.wav --debug sid-filter-bphp-smoke.json
)
add_test(NAME chipper_render_sid_filter_all_smoke
    COMMAND chipper_render --chip sid --accuracy authentic --macro manual --wave-shape pulse --sid-filter-mode all --stereo-spread 1.0 --control3 0.55 --clock 985248 --rate 48000 --seconds 0.04 --note 69 --out sid-filter-all-smoke.wav --debug sid-filter-all-smoke.json
)
add_test(NAME chipper_render_sid_filter_route_v3_smoke
    COMMAND chipper_render --chip sid --accuracy authentic --macro manual --wave-shape pulse --sid-filter-mode lp --sid-filter-routing v3 --stereo-spread 0.0 --control3 0.45 --clock 985248 --rate 48000 --seconds 0.04 --note 69 --out sid-filter-route-v3-smoke.wav --debug sid-filter-route-v3-smoke.json
)
add_test(NAME chipper_render_sid_filter_exact_bits_smoke
    COMMAND chipper_render --chip sid --accuracy authentic --macro manual --wave-shape pulse --sid-filter-mode 0x50 --sid-filter-routing 0x05 --stereo-spread 0.0 --control3 0.55 --clock 985248 --rate 48000 --seconds 0.04 --note 69 --out sid-filter-exact-bits-smoke.wav --debug sid-filter-exact-bits-smoke.json
)
add_test(NAME chipper_render_sid_follow_aliases_smoke
    COMMAND chipper_render --chip sid --accuracy authentic --macro lead --wave-shape follow --sid-voice2-wave follow --sid-voice3-wave follow --sid-filter-mode follow --sid-filter-routing follow --sid-mod-mode follow --sid-model follow --sid-attack follow --sid-decay follow --sid-sustain follow --sid-release follow --clock 985248 --rate 48000 --seconds 0.04 --note 69 --out sid-follow-aliases-smoke.wav --debug sid-follow-aliases-smoke.json
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
add_test(NAME chipper_render_nes_pulse2_duty_override_smoke
    COMMAND chipper_render --chip nes --accuracy authentic --macro manual --control1 0.0 --nes-pulse2-duty 75 --clock 1789773 --rate 48000 --seconds 0.01 --note 69 --out nes-pulse2-duty-smoke.wav --debug nes-pulse2-duty-smoke.json
)
add_test(NAME chipper_render_nes_pulse2_duty_follow_smoke
    COMMAND chipper_render --chip nes --accuracy authentic --macro manual --control1 0.0 --nes-pulse2-duty follow --clock 1789773 --rate 48000 --seconds 0.01 --note 69 --out nes-pulse2-duty-follow-smoke.wav --debug nes-pulse2-duty-follow-smoke.json
)
add_test(NAME chipper_render_nes_sources_off_smoke
    COMMAND chipper_render --chip nes --accuracy authentic --macro manual --source1 0 --source2 0 --source3 0 --source4 0 --clock 1789773 --rate 48000 --seconds 0.01 --note 69 --out nes-sources-off-smoke.wav --debug nes-sources-off-smoke.json
)
add_test(NAME chipper_render_nes_envelope_decay_smoke
    COMMAND chipper_render --chip nes --accuracy authentic --macro manual --source3 0 --source4 0 --envelope-decay 1.0 --clock 1789773 --rate 48000 --seconds 0.05 --note 69 --out nes-envelope-decay-smoke.wav --debug nes-envelope-decay-smoke.json
)
add_test(NAME chipper_render_nes_noise_long_smoke
    COMMAND chipper_render --chip nes --accuracy authentic --macro drum --control3 1.0 --sn-noise-mode long --clock 1789773 --rate 48000 --seconds 0.01 --note 69 --out nes-noise-long-smoke.wav --debug nes-noise-long-smoke.json
)
add_test(NAME chipper_render_nes_noise_short_smoke
    COMMAND chipper_render --chip nes --accuracy authentic --macro drum --control3 0.0 --sn-noise-mode short --clock 1789773 --rate 48000 --seconds 0.01 --note 69 --out nes-noise-short-smoke.wav --debug nes-noise-short-smoke.json
)
add_test(NAME chipper_render_ym2149_smoke
    COMMAND chipper_render --chip ym2149 --accuracy authentic --clock 2000000 --rate 48000 --seconds 0.10 --note 72 --out ym-smoke.wav --debug ym-smoke.json
)
add_test(NAME chipper_render_ym2149_stereo_spread_smoke
    COMMAND chipper_render --chip ym2149 --accuracy authentic --macro manual --source2 0 --source3 0 --source4 0 --stereo-spread 1.0 --clock 2000000 --rate 48000 --seconds 0.04 --note 72 --out ym-stereo-spread-smoke.wav --debug ym-stereo-spread-smoke.json
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
add_test(NAME chipper_render_ym2149_source_level_mute_smoke
    COMMAND chipper_render --chip ym2149 --accuracy authentic --macro manual --source1 0 --source3 0 --source4 0 --level2 0 --clock 2000000 --rate 48000 --seconds 0.03 --note 72 --out ym-source-level-mute-smoke.wav --debug ym-source-level-mute-smoke.json
)
add_test(NAME chipper_render_ym2149_noise_source_off_smoke
    COMMAND chipper_render --chip ym2149 --accuracy authentic --macro drum --source4 0 --clock 2000000 --rate 48000 --seconds 0.03 --note 72 --out ym-noise-source-off-smoke.wav --debug ym-noise-source-off-smoke.json
)
add_test(NAME chipper_render_ym2149_noise_period_slow_smoke
    COMMAND chipper_render --chip ym2149 --accuracy authentic --macro manual --control3 0.0 --clock 2000000 --rate 48000 --seconds 0.03 --note 72 --out ym-noise-period-slow-smoke.wav --debug ym-noise-period-slow-smoke.json
)
add_test(NAME chipper_render_ym2149_noise_period_fast_smoke
    COMMAND chipper_render --chip ym2149 --accuracy authentic --macro manual --control3 1.0 --clock 2000000 --rate 48000 --seconds 0.03 --note 72 --out ym-noise-period-fast-smoke.wav --debug ym-noise-period-fast-smoke.json
)
add_test(NAME chipper_render_ym2149_mixer_noise_smoke
    COMMAND chipper_render --chip ym2149 --accuracy authentic --macro manual --control4 0.0 --clock 2000000 --rate 48000 --seconds 0.03 --note 72 --out ym-mixer-noise-smoke.wav --debug ym-mixer-noise-smoke.json
)
add_test(NAME chipper_render_ym2149_mixer_tone_smoke
    COMMAND chipper_render --chip ym2149 --accuracy authentic --macro manual --control4 0.5 --clock 2000000 --rate 48000 --seconds 0.03 --note 72 --out ym-mixer-tone-smoke.wav --debug ym-mixer-tone-smoke.json
)
add_test(NAME chipper_render_ym2149_mixer_both_smoke
    COMMAND chipper_render --chip ym2149 --accuracy authentic --macro manual --control4 1.0 --clock 2000000 --rate 48000 --seconds 0.03 --note 72 --out ym-mixer-both-smoke.wav --debug ym-mixer-both-smoke.json
)
add_test(NAME chipper_render_ym2149_channel_mix_smoke
    COMMAND chipper_render --chip ym2149 --accuracy authentic --macro manual --control4 0.5 --ym-channel-a-mix tone --ym-channel-b-mix noise --ym-channel-c-mix off --clock 2000000 --rate 48000 --seconds 0.03 --note 72 --out ym-channel-mix-smoke.wav --debug ym-channel-mix-smoke.json
)
add_test(NAME chipper_render_ym2149_channel_mix_follow_smoke
    COMMAND chipper_render --chip ym2149 --accuracy authentic --macro manual --control4 0.5 --ym-channel-a-mix follow --ym-channel-b-mix follow --ym-channel-c-mix follow --clock 2000000 --rate 48000 --seconds 0.03 --note 72 --out ym-channel-mix-follow-smoke.wav --debug ym-channel-mix-follow-smoke.json
)
add_test(NAME chipper_render_ym2149_envelope_fall_smoke
    COMMAND chipper_render --chip ym2149 --accuracy authentic --macro manual --ym-envelope-shape fall --clock 2000000 --rate 48000 --seconds 0.05 --note 72 --out ym-envelope-fall-smoke.wav --debug ym-envelope-fall-smoke.json
)
add_test(NAME chipper_render_ym2149_envelope_triangle_smoke
    COMMAND chipper_render --chip ym2149 --accuracy authentic --macro manual --ym-envelope-shape triangle --clock 2000000 --rate 48000 --seconds 0.05 --note 72 --out ym-envelope-triangle-smoke.wav --debug ym-envelope-triangle-smoke.json
)
add_test(NAME chipper_render_ym2149_envelope_code0f_smoke
    COMMAND chipper_render --chip ym2149 --accuracy authentic --macro manual --ym-envelope-shape 0x0f --clock 2000000 --rate 48000 --seconds 0.05 --note 72 --out ym-envelope-code0f-smoke.wav --debug ym-envelope-code0f-smoke.json
)
add_test(NAME chipper_render_ym2149_envelope_code10_smoke
    COMMAND chipper_render --chip ym2149 --accuracy authentic --macro manual --ym-envelope-shape code10 --clock 2000000 --rate 48000 --seconds 0.05 --note 72 --out ym-envelope-code10-smoke.wav --debug ym-envelope-code10-smoke.json
)
add_test(NAME chipper_render_ym2149_envelope_fast_smoke
    COMMAND chipper_render --chip ym2149 --accuracy authentic --macro manual --ym-envelope-shape triangle --envelope-decay 1.0 --clock 2000000 --rate 48000 --seconds 0.05 --note 72 --out ym-envelope-fast-smoke.wav --debug ym-envelope-fast-smoke.json
)
add_test(NAME chipper_render_ym2149_envelope_reset_smoke
    COMMAND chipper_render --chip ym2149 --accuracy authentic --macro manual --ym-envelope-shape fall --envelope-decay 1.0 --clock 2000000 --rate 48000 --seconds 0.001 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/ym2149-envelope-reset-events.txt --out ym-envelope-reset-smoke.wav --debug ym-envelope-reset-smoke.json
)
add_test(NAME chipper_render_ym2149_volume_curve_smoke
    COMMAND chipper_render --chip ym2149 --accuracy authentic --macro manual --clock 2000000 --rate 48000 --seconds 0.01 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/ym2149-volume-curve-events.txt --out ym-volume-curve-smoke.wav --debug ym-volume-curve-smoke.json
)
add_test(NAME chipper_render_sn76489_smoke
    COMMAND chipper_render --chip sn76489 --accuracy authentic --clock 3579545 --rate 48000 --seconds 0.10 --note 76 --out sn-smoke.wav --debug sn-smoke.json
)
add_test(NAME chipper_render_sn76489_stereo_spread_smoke
    COMMAND chipper_render --chip sn76489 --accuracy authentic --macro manual --source2 0 --source3 0 --source4 0 --stereo-spread 1.0 --clock 3579545 --rate 48000 --seconds 0.04 --note 76 --out sn-stereo-spread-smoke.wav --debug sn-stereo-spread-smoke.json
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
add_test(NAME chipper_render_sn76489_noise_bias_low_smoke
    COMMAND chipper_render --chip sn76489 --accuracy authentic --macro drum --sn-noise-mode macro --control3 0.0 --clock 3579545 --rate 48000 --seconds 0.04 --note 60 --out sn-noise-bias-low-smoke.wav --debug sn-noise-bias-low-smoke.json
)
add_test(NAME chipper_render_sn76489_noise_bias_high_smoke
    COMMAND chipper_render --chip sn76489 --accuracy authentic --macro drum --sn-noise-mode macro --control3 1.0 --clock 3579545 --rate 48000 --seconds 0.04 --note 60 --out sn-noise-bias-high-smoke.wav --debug sn-noise-bias-high-smoke.json
)
add_test(NAME chipper_render_sn76489_noise_periodic_smoke
    COMMAND chipper_render --chip sn76489 --accuracy authentic --macro drum --sn-noise-mode periodic-hi --clock 3579545 --rate 48000 --seconds 0.04 --note 60 --out sn-noise-periodic-smoke.wav --debug sn-noise-periodic-smoke.json
)
add_test(NAME chipper_render_sn76489_noise_white_t3_smoke
    COMMAND chipper_render --chip sn76489 --accuracy authentic --macro drum --sn-noise-mode white-t3 --clock 3579545 --rate 48000 --seconds 0.04 --note 60 --out sn-noise-white-t3-smoke.wav --debug sn-noise-white-t3-smoke.json
)
add_test(NAME chipper_render_sn76489_register_trace_smoke
    COMMAND chipper_render --chip sn76489 --accuracy authentic --macro manual --clock 3579545 --rate 48000 --seconds 0.04 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/sn76489-register-trace-events.txt --out sn-register-trace-smoke.wav --debug sn-register-trace-smoke.json
)
add_test(NAME chipper_render_sn76489_tone_zero_smoke
    COMMAND chipper_render --chip sn76489 --accuracy authentic --macro manual --source2 0 --source3 0 --source4 0 --clock 3579545 --rate 48000 --seconds 0.02 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/sn76489-tone-period-zero-events.txt --out sn-tone-zero-smoke.wav --debug sn-tone-zero-smoke.json
)
add_test(NAME chipper_render_sn76489_tone_one_smoke
    COMMAND chipper_render --chip sn76489 --accuracy authentic --macro manual --source2 0 --source3 0 --source4 0 --clock 3579545 --rate 48000 --seconds 0.02 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/sn76489-tone-period-one-events.txt --out sn-tone-one-smoke.wav --debug sn-tone-one-smoke.json
)
add_test(NAME chipper_render_sn76489_volume_data_smoke
    COMMAND chipper_render --chip sn76489 --accuracy authentic --macro manual --source2 0 --source3 0 --source4 0 --clock 3579545 --rate 48000 --seconds 0.02 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/sn76489-volume-data-events.txt --out sn-volume-data-smoke.wav --debug sn-volume-data-smoke.json
)
add_test(NAME chipper_render_sn76489_noise_level_min_smoke
    COMMAND chipper_render --chip sn76489 --accuracy authentic --macro manual --control4 0.0 --clock 3579545 --rate 48000 --seconds 0.03 --note 76 --out sn-noise-level-min-smoke.wav --debug sn-noise-level-min-smoke.json
)
add_test(NAME chipper_render_sn76489_noise_level_max_smoke
    COMMAND chipper_render --chip sn76489 --accuracy authentic --macro manual --control4 1.0 --clock 3579545 --rate 48000 --seconds 0.03 --note 76 --out sn-noise-level-max-smoke.wav --debug sn-noise-level-max-smoke.json
)
add_test(NAME chipper_render_preset_nes_hero_smoke
    COMMAND chipper_render --preset nes-hero-pulse --rate 48000 --seconds 0.04 --note 69 --out preset-nes-hero-smoke.wav --debug preset-nes-hero-smoke.json
)
add_test(NAME chipper_render_preset_nes_triangle_bass_smoke
    COMMAND chipper_render --preset nes-triangle-bass --rate 48000 --seconds 0.04 --note 48 --out preset-nes-triangle-bass-smoke.wav --debug preset-nes-triangle-bass-smoke.json
)
add_test(NAME chipper_render_preset_nes_noise_snare_smoke
    COMMAND chipper_render --preset nes-noise-snare --rate 48000 --seconds 0.04 --note 60 --out preset-nes-noise-snare-smoke.wav --debug preset-nes-noise-snare-smoke.json
)
add_test(NAME chipper_render_preset_nes_coin_blip_smoke
    COMMAND chipper_render --preset nes-coin-blip --rate 48000 --seconds 0.04 --note 76 --out preset-nes-coin-blip-smoke.wav --debug preset-nes-coin-blip-smoke.json
)
add_test(NAME chipper_render_preset_nes_boss_damage_smoke
    COMMAND chipper_render --preset nes-boss-damage --rate 48000 --seconds 0.04 --note 48 --out preset-nes-boss-damage-smoke.wav --debug preset-nes-boss-damage-smoke.json
)
add_test(NAME chipper_render_preset_nes_power_up_smoke
    COMMAND chipper_render --preset nes-power-up-rise --rate 48000 --seconds 0.04 --note 69 --out preset-nes-power-up-smoke.wav --debug preset-nes-power-up-smoke.json
)
add_test(NAME chipper_render_preset_dmg_wave_bass_smoke
    COMMAND chipper_render --preset dmg-wave-bass --rate 48000 --seconds 0.04 --note 48 --out preset-dmg-wave-bass-smoke.wav --debug preset-dmg-wave-bass-smoke.json
)
add_test(NAME chipper_render_preset_dmg_pocket_arp_smoke
    COMMAND chipper_render --preset dmg-pocket-arp --rate 48000 --seconds 0.04 --note 69 --out preset-dmg-pocket-arp-smoke.wav --debug preset-dmg-pocket-arp-smoke.json
)
add_test(NAME chipper_render_preset_dmg_noise_hat_smoke
    COMMAND chipper_render --preset dmg-noise-hat --rate 48000 --seconds 0.04 --note 72 --out preset-dmg-noise-hat-smoke.wav --debug preset-dmg-noise-hat-smoke.json
)
add_test(NAME chipper_render_preset_dmg_power_up_smoke
    COMMAND chipper_render --preset dmg-power-up-rise --rate 48000 --seconds 0.04 --note 69 --out preset-dmg-power-up-smoke.wav --debug preset-dmg-power-up-smoke.json
)
add_test(NAME chipper_render_preset_sid_dirty_bass_smoke
    COMMAND chipper_render --preset sid-dirty-bass --rate 48000 --seconds 0.04 --note 48 --out preset-sid-dirty-bass-smoke.wav --debug preset-sid-dirty-bass-smoke.json
)
add_test(NAME chipper_render_preset_sid_pwm_lead_smoke
    COMMAND chipper_render --preset sid-pwm-lead --rate 48000 --seconds 0.04 --note 69 --out preset-sid-pwm-lead-smoke.wav --debug preset-sid-pwm-lead-smoke.json
)
add_test(NAME chipper_render_preset_sid_robot_arp_smoke
    COMMAND chipper_render --preset sid-robot-arp --rate 48000 --seconds 0.04 --note 69 --out preset-sid-robot-arp-smoke.wav --debug preset-sid-robot-arp-smoke.json
)
add_test(NAME chipper_render_preset_sid_filter_pluck_smoke
    COMMAND chipper_render --preset sid-filter-pluck --rate 48000 --seconds 0.04 --note 72 --out preset-sid-filter-pluck-smoke.wav --debug preset-sid-filter-pluck-smoke.json
)
add_test(NAME chipper_render_preset_sid_notch_pwm_keys_smoke
    COMMAND chipper_render --preset sid-notch-pwm-keys --rate 48000 --seconds 0.04 --note 72 --out preset-sid-notch-pwm-keys-smoke.wav --debug preset-sid-notch-pwm-keys-smoke.json
)
add_test(NAME chipper_render_preset_sid_lpbp_sweep_stack_smoke
    COMMAND chipper_render --preset sid-lpbp-sweep-stack --rate 48000 --seconds 0.04 --note 69 --out preset-sid-lpbp-sweep-stack-smoke.wav --debug preset-sid-lpbp-sweep-stack-smoke.json
)
add_test(NAME chipper_render_preset_sid_bphp_metal_bell_smoke
    COMMAND chipper_render --preset sid-bphp-metal-bell --rate 48000 --seconds 0.04 --note 76 --out preset-sid-bphp-metal-bell-smoke.wav --debug preset-sid-bphp-metal-bell-smoke.json
)
add_test(NAME chipper_render_preset_sid_allmode_growl_smoke
    COMMAND chipper_render --preset sid-allmode-growl --rate 48000 --seconds 0.04 --note 48 --out preset-sid-allmode-growl-smoke.wav --debug preset-sid-allmode-growl-smoke.json
)
add_test(NAME chipper_render_preset_sid_pwm_bass_split_smoke
    COMMAND chipper_render --preset sid-pwm-bass-split --rate 48000 --seconds 0.04 --note 48 --out preset-sid-pwm-bass-split-smoke.wav --debug preset-sid-pwm-bass-split-smoke.json
)
add_test(NAME chipper_render_preset_sid_wide_pwm_pad_smoke
    COMMAND chipper_render --preset sid-wide-pwm-pad --rate 48000 --seconds 0.08 --note 60 --out preset-sid-wide-pwm-pad-smoke.wav --debug preset-sid-wide-pwm-pad-smoke.json
)
add_test(NAME chipper_render_preset_sid_hard_sync_saw_smoke
    COMMAND chipper_render --preset sid-hard-sync-saw --rate 48000 --seconds 0.04 --note 69 --out preset-sid-hard-sync-saw-smoke.wav --debug preset-sid-hard-sync-saw-smoke.json
)
add_test(NAME chipper_render_preset_sid_gated_noise_tom_smoke
    COMMAND chipper_render --preset sid-gated-noise-tom --rate 48000 --seconds 0.04 --note 43 --out preset-sid-gated-noise-tom-smoke.wav --debug preset-sid-gated-noise-tom-smoke.json
)
add_test(NAME chipper_render_preset_ym_noise_hat_smoke
    COMMAND chipper_render --preset ym-noise-hat --rate 48000 --seconds 0.04 --note 72 --out preset-ym-noise-hat-smoke.wav --debug preset-ym-noise-hat-smoke.json
)
add_test(NAME chipper_render_preset_ym_three_voice_arp_smoke
    COMMAND chipper_render --preset ym-three-voice-arp --rate 48000 --seconds 0.04 --note 60 --out preset-ym-three-voice-arp-smoke.wav --debug preset-ym-three-voice-arp-smoke.json
)
add_test(NAME chipper_render_preset_ym_fake_chord_smoke
    COMMAND chipper_render --preset ym-fake-chord-stack --rate 48000 --seconds 0.04 --note 60 --out preset-ym-fake-chord-smoke.wav --debug preset-ym-fake-chord-smoke.json
)
add_test(NAME chipper_render_preset_ym_menu_beep_smoke
    COMMAND chipper_render --preset ym-menu-beep --rate 48000 --seconds 0.04 --note 76 --out preset-ym-menu-beep-smoke.wav --debug preset-ym-menu-beep-smoke.json
)
add_test(NAME chipper_render_preset_ym_bright_beep_smoke
    COMMAND chipper_render --preset ym-bright-beep --rate 48000 --seconds 0.04 --note 76 --out preset-ym-bright-beep-smoke.wav --debug preset-ym-bright-beep-smoke.json
)
add_test(NAME chipper_render_preset_ym_envelope_bell_smoke
    COMMAND chipper_render --preset ym-envelope-bell --rate 48000 --seconds 0.04 --note 76 --out preset-ym-envelope-bell-smoke.wav --debug preset-ym-envelope-bell-smoke.json
)
add_test(NAME chipper_render_preset_ym_arcade_clang_smoke
    COMMAND chipper_render --preset ym-arcade-clang --rate 48000 --seconds 0.04 --note 48 --out preset-ym-arcade-clang-smoke.wav --debug preset-ym-arcade-clang-smoke.json
)
add_test(NAME chipper_render_preset_ym_fast_minor_arp_smoke
    COMMAND chipper_render --preset ym-fast-minor-arp --rate 48000 --seconds 0.04 --note 60 --out preset-ym-fast-minor-arp-smoke.wav --debug preset-ym-fast-minor-arp-smoke.json
)
add_test(NAME chipper_render_preset_sn_noise_hit_smoke
    COMMAND chipper_render --preset sn-noise-hit --rate 48000 --seconds 0.04 --note 60 --out preset-sn-noise-hit-smoke.wav --debug preset-sn-noise-hit-smoke.json
)
add_test(NAME chipper_render_preset_sn_psg_lead_smoke
    COMMAND chipper_render --preset sn-psg-lead --rate 48000 --seconds 0.04 --note 69 --out preset-sn-psg-lead-smoke.wav --debug preset-sn-psg-lead-smoke.json
)
add_test(NAME chipper_render_preset_sn_psg_coin_smoke
    COMMAND chipper_render --preset sn-psg-coin --rate 48000 --seconds 0.04 --note 76 --out preset-sn-psg-coin-smoke.wav --debug preset-sn-psg-coin-smoke.json
)
add_test(NAME chipper_render_preset_sn_arcade_laser_smoke
    COMMAND chipper_render --preset sn-arcade-laser --rate 48000 --seconds 0.04 --note 69 --out preset-sn-arcade-laser-smoke.wav --debug preset-sn-arcade-laser-smoke.json
)
add_test(NAME chipper_render_preset_sn_tone_stack_smoke
    COMMAND chipper_render --preset sn-tone-stack --rate 48000 --seconds 0.04 --note 69 --out preset-sn-tone-stack-smoke.wav --debug preset-sn-tone-stack-smoke.json
)
add_test(NAME chipper_render_preset_sn_arcade_bass_smoke
    COMMAND chipper_render --preset sn-arcade-bass --rate 48000 --seconds 0.04 --note 48 --out preset-sn-arcade-bass-smoke.wav --debug preset-sn-arcade-bass-smoke.json
)
add_test(NAME chipper_render_preset_sn_periodic_zap_smoke
    COMMAND chipper_render --preset sn-periodic-zap --rate 48000 --seconds 0.04 --note 69 --out preset-sn-periodic-zap-smoke.wav --debug preset-sn-periodic-zap-smoke.json
)
add_test(NAME chipper_render_preset_sn_warning_alarm_smoke
    COMMAND chipper_render --preset sn-warning-alarm --rate 48000 --seconds 0.04 --note 72 --out preset-sn-warning-alarm-smoke.wav --debug preset-sn-warning-alarm-smoke.json
)
add_test(NAME chipper_render_preset_nes_duty_bass_duo_smoke
    COMMAND chipper_render --preset nes-duty-bass-duo --rate 48000 --seconds 0.04 --note 48 --out preset-nes-duty-bass-duo-smoke.wav --debug preset-nes-duty-bass-duo-smoke.json
)
add_test(NAME chipper_render_preset_dmg_step_wave_arp_smoke
    COMMAND chipper_render --preset dmg-step-wave-arp --rate 48000 --seconds 0.04 --note 69 --out preset-dmg-step-wave-arp-smoke.wav --debug preset-dmg-step-wave-arp-smoke.json
)
add_test(NAME chipper_render_preset_dmg_pulse_lead_smoke
    COMMAND chipper_render --preset dmg-pulse-lead --rate 48000 --seconds 0.04 --note 69 --out preset-dmg-pulse-lead-smoke.wav --debug preset-dmg-pulse-lead-smoke.json
)
add_test(NAME chipper_render_preset_dmg_wave_glass_smoke
    COMMAND chipper_render --preset dmg-wave-glass --rate 48000 --seconds 0.04 --note 69 --out preset-dmg-wave-glass-smoke.wav --debug preset-dmg-wave-glass-smoke.json
)
add_test(NAME chipper_render_preset_dmg_noise_kick_smoke
    COMMAND chipper_render --preset dmg-noise-kick --rate 48000 --seconds 0.04 --note 48 --out preset-dmg-noise-kick-smoke.wav --debug preset-dmg-noise-kick-smoke.json
)
add_test(NAME chipper_render_preset_dmg_split_chord_smoke
    COMMAND chipper_render --preset dmg-split-chord --rate 48000 --seconds 0.04 --note 60 --out preset-dmg-split-chord-smoke.wav --debug preset-dmg-split-chord-smoke.json
)
add_test(NAME chipper_render_preset_sid_filtered_noise_sweep_smoke
    COMMAND chipper_render --preset sid-filtered-noise-sweep --rate 48000 --seconds 0.04 --note 69 --out preset-sid-filtered-noise-sweep-smoke.wav --debug preset-sid-filtered-noise-sweep-smoke.json
)
add_test(NAME chipper_render_preset_ym_triangle_env_keys_smoke
    COMMAND chipper_render --preset ym-triangle-env-keys --rate 48000 --seconds 0.04 --note 72 --out preset-ym-triangle-env-keys-smoke.wav --debug preset-ym-triangle-env-keys-smoke.json
)
add_test(NAME chipper_render_preset_sn_periodic_bass_click_smoke
    COMMAND chipper_render --preset sn-periodic-bass-click --rate 48000 --seconds 0.04 --note 48 --out preset-sn-periodic-bass-click-smoke.wav --debug preset-sn-periodic-bass-click-smoke.json
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
add_test(NAME chipper_render_dmg_sweep_shift_min_smoke
    COMMAND chipper_render --chip dmg --accuracy authentic --macro manual --control2 0.0 --source2 0 --source3 0 --source4 0 --clock 4194304 --rate 48000 --seconds 0.01 --note 69 --out dmg-sweep-shift-min-smoke.wav --debug dmg-sweep-shift-min-smoke.json
)
add_test(NAME chipper_render_dmg_sweep_shift_max_smoke
    COMMAND chipper_render --chip dmg --accuracy authentic --macro manual --control2 1.0 --source2 0 --source3 0 --source4 0 --clock 4194304 --rate 48000 --seconds 0.01 --note 69 --out dmg-sweep-shift-max-smoke.wav --debug dmg-sweep-shift-max-smoke.json
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
add_test(NAME chipper_render_dmg_envelope_level_low_smoke
    COMMAND chipper_render --chip dmg --accuracy authentic --macro manual --source2 0 --source3 0 --source4 0 --control4 0.0 --clock 4194304 --rate 48000 --seconds 0.01 --note 69 --out dmg-envelope-level-low-smoke.wav --debug dmg-envelope-level-low-smoke.json
)
add_test(NAME chipper_render_dmg_envelope_level_high_smoke
    COMMAND chipper_render --chip dmg --accuracy authentic --macro manual --source2 0 --source3 0 --source4 0 --control4 1.0 --clock 4194304 --rate 48000 --seconds 0.01 --note 69 --out dmg-envelope-level-high-smoke.wav --debug dmg-envelope-level-high-smoke.json
)
add_test(NAME chipper_render_dmg_wave_triangle_smoke
    COMMAND chipper_render --chip dmg --accuracy authentic --macro bass --source1 0 --source2 0 --source4 0 --wave-shape triangle --clock 4194304 --rate 48000 --seconds 0.04 --note 72 --out dmg-wave-triangle-smoke.wav --debug dmg-wave-triangle-smoke.json
)
add_test(NAME chipper_render_dmg_wave_pulse_smoke
    COMMAND chipper_render --chip dmg --accuracy authentic --macro bass --source1 0 --source2 0 --source4 0 --wave-shape pulse --clock 4194304 --rate 48000 --seconds 0.04 --note 72 --out dmg-wave-pulse-smoke.wav --debug dmg-wave-pulse-smoke.json
)
add_test(NAME chipper_render_dmg_wave_level_mute_smoke
    COMMAND chipper_render --chip dmg --accuracy authentic --macro bass --source1 0 --source2 0 --source4 0 --wave-shape triangle --dmg-wave-level mute --clock 4194304 --rate 48000 --seconds 0.04 --note 72 --out dmg-wave-level-mute-smoke.wav --debug dmg-wave-level-mute-smoke.json
)
add_test(NAME chipper_render_dmg_wave_level_full_smoke
    COMMAND chipper_render --chip dmg --accuracy authentic --macro bass --source1 0 --source2 0 --source4 0 --wave-shape triangle --dmg-wave-level 100 --clock 4194304 --rate 48000 --seconds 0.04 --note 72 --out dmg-wave-level-full-smoke.wav --debug dmg-wave-level-full-smoke.json
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
add_test(NAME chipper_render_dmg_noise_wide_smoke
    COMMAND chipper_render --chip dmg --accuracy authentic --macro drum --control3 1.0 --sn-noise-mode wide --clock 4194304 --rate 48000 --seconds 0.02 --note 60 --out dmg-noise-wide-smoke.wav --debug dmg-noise-wide-smoke.json
)
add_test(NAME chipper_render_dmg_noise_narrow_smoke
    COMMAND chipper_render --chip dmg --accuracy authentic --macro drum --control3 0.0 --sn-noise-mode narrow --clock 4194304 --rate 48000 --seconds 0.02 --note 60 --out dmg-noise-narrow-smoke.wav --debug dmg-noise-narrow-smoke.json
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
add_test(NAME chipper_render_dmg_route_left_smoke
    COMMAND chipper_render --chip dmg --accuracy authentic --macro manual --source2 0 --source3 0 --source4 0 --dmg-stereo-route left --clock 4194304 --rate 48000 --seconds 0.03 --note 69 --out dmg-route-left-smoke.wav --debug dmg-route-left-smoke.json
)
add_test(NAME chipper_render_dmg_route_right_smoke
    COMMAND chipper_render --chip dmg --accuracy authentic --macro manual --source2 0 --source3 0 --source4 0 --dmg-stereo-route right --clock 4194304 --rate 48000 --seconds 0.03 --note 69 --out dmg-route-right-smoke.wav --debug dmg-route-right-smoke.json
)
add_test(NAME chipper_render_dmg_route_split_smoke
    COMMAND chipper_render --chip dmg --accuracy authentic --macro manual --source2 0 --source3 0 --source4 0 --dmg-stereo-route split --clock 4194304 --rate 48000 --seconds 0.03 --note 69 --out dmg-route-split-smoke.wav --debug dmg-route-split-smoke.json
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
add_test(NAME chipper_render_nes_dmc_direct_parameter_low_smoke
    COMMAND chipper_render --chip nes --accuracy authentic --macro manual --source1 0 --source2 0 --source3 0 --source4 0 --nes-dmc-direct-level 0.0 --clock 1789773 --rate 48000 --seconds 0.004 --out nes-dmc-direct-parameter-low-smoke.wav --debug nes-dmc-direct-parameter-low-smoke.json
)
add_test(NAME chipper_render_nes_dmc_direct_parameter_high_smoke
    COMMAND chipper_render --chip nes --accuracy authentic --macro manual --source1 0 --source2 0 --source3 0 --source4 0 --nes-dmc-direct-level 1.0 --clock 1789773 --rate 48000 --seconds 0.004 --out nes-dmc-direct-parameter-high-smoke.wav --debug nes-dmc-direct-parameter-high-smoke.json
)
if (Python3_EXECUTABLE)
    add_test(NAME chipper_generate_nes_dmc_fixture
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/write_binary_fixture.py ${CMAKE_CURRENT_BINARY_DIR}/generated-nes-dmc-sample.dmc "55 aa f0 0f"
    )
    add_test(NAME chipper_render_nes_dmc_sample_smoke
        COMMAND chipper_render --chip nes --accuracy authentic --macro manual --source1 0 --source2 0 --source3 0 --source4 1 --nes-dmc-direct-level 0.5 --nes-dmc-sample ${CMAKE_CURRENT_BINARY_DIR}/generated-nes-dmc-sample.dmc --clock 1789773 --rate 48000 --seconds 0.01 --out nes-dmc-sample-smoke.wav --debug nes-dmc-sample-smoke.json
    )
    set_tests_properties(chipper_render_nes_dmc_sample_smoke PROPERTIES DEPENDS chipper_generate_nes_dmc_fixture)
    add_test(NAME chipper_render_nes_dmc_rate0_sample_smoke
        COMMAND chipper_render --chip nes --accuracy authentic --macro manual --source1 0 --source2 0 --source3 0 --source4 1 --nes-dmc-direct-level 0.5 --nes-dmc-rate 0 --nes-dmc-sample ${CMAKE_CURRENT_BINARY_DIR}/generated-nes-dmc-sample.dmc --clock 1789773 --rate 48000 --seconds 0.004 --out nes-dmc-rate0-sample-smoke.wav --debug nes-dmc-rate0-sample-smoke.json
    )
    set_tests_properties(chipper_render_nes_dmc_rate0_sample_smoke PROPERTIES DEPENDS chipper_generate_nes_dmc_fixture)
    add_test(NAME chipper_render_nes_dmc_rate15_sample_smoke
        COMMAND chipper_render --chip nes --accuracy authentic --macro manual --source1 0 --source2 0 --source3 0 --source4 1 --nes-dmc-direct-level 0.5 --nes-dmc-rate 15 --nes-dmc-sample ${CMAKE_CURRENT_BINARY_DIR}/generated-nes-dmc-sample.dmc --clock 1789773 --rate 48000 --seconds 0.004 --out nes-dmc-rate15-sample-smoke.wav --debug nes-dmc-rate15-sample-smoke.json
    )
    set_tests_properties(chipper_render_nes_dmc_rate15_sample_smoke PROPERTIES DEPENDS chipper_generate_nes_dmc_fixture)
    add_test(NAME chipper_render_nes_dmc_chip_poly_sample_smoke
        COMMAND chipper_render --chip nes --accuracy authentic --macro manual --play-mode chip-poly --source1 0 --source2 0 --source3 0 --source4 1 --nes-dmc-direct-level 0.5 --nes-dmc-sample ${CMAKE_CURRENT_BINARY_DIR}/generated-nes-dmc-sample.dmc --clock 1789773 --rate 48000 --seconds 0.01 --note 60 --out nes-dmc-chip-poly-sample-smoke.wav --debug nes-dmc-chip-poly-sample-smoke.json
    )
    set_tests_properties(chipper_render_nes_dmc_chip_poly_sample_smoke PROPERTIES DEPENDS chipper_generate_nes_dmc_fixture)
endif()
add_test(NAME chipper_render_nes_frame_counter_5step_smoke
    COMMAND chipper_render --chip nes --accuracy authentic --macro manual --clock 1789773 --rate 48000 --seconds 0.001 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/nes-frame-counter-5step-events.txt --out nes-frame-counter-5step-smoke.wav --debug nes-frame-counter-5step-smoke.json
)
add_test(NAME chipper_render_nes_frame_counter_irq_smoke
    COMMAND chipper_render --chip nes --accuracy authentic --macro manual --clock 1789773 --rate 48000 --seconds 0.017 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/nes-frame-counter-irq-events.txt --out nes-frame-counter-irq-smoke.wav --debug nes-frame-counter-irq-smoke.json
)
add_test(NAME chipper_render_nes_frame_counter_inhibit_smoke
    COMMAND chipper_render --chip nes --accuracy authentic --macro manual --clock 1789773 --rate 48000 --seconds 0.017 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/nes-frame-counter-inhibit-events.txt --out nes-frame-counter-inhibit-smoke.wav --debug nes-frame-counter-inhibit-smoke.json
)
add_test(NAME chipper_render_list_descriptors_smoke
    COMMAND chipper_render --list-descriptors --debug chipper-descriptors.json
)
add_test(NAME chipper_render_describe_nes_smoke
    COMMAND chipper_render --describe-chip nes --debug nes-descriptor.json
)

if (Python3_Interpreter_FOUND)
    add_test(NAME chipper_render_list_descriptors_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_descriptor_list_json.py ${CMAKE_CURRENT_BINARY_DIR}/chipper-descriptors.json --chip-count 17 --chip nes --chip dmg --chip ym2149 --chip sn76489 --chip sid --chip ym2612 --chip custom --implemented nes=1 --implemented dmg=1 --implemented ym2149=1 --implemented sn76489=1 --implemented sid=1 --verification-badge "nes=Verified partial" --verification-badge "dmg=Verified partial" --verification-badge "sid=Verified partial" --verification-badge "ym2149=Verified partial" --verification-badge "sn76489=Verified partial" --verification-badge "ym2612=Unverified planned" --verification-cycle nes=0 --verification-cycle sid=0 --verification-hardware nes=0 --verification-hardware sid=0 --param-kind nes:macroControl1=chipRegister --param-surface nes:macroControl1=segmentedChoice --param-choices nes:macroControl1=4 --param-midi-cc nes:macroControl1=76 --param-kind nes:pulse2Duty=chipRegister --param-surface nes:pulse2Duty=segmentedChoice --param-choices nes:pulse2Duty=5 --param-midi-cc nes:pulse2Duty=104 --param-kind nes:macroControl3=chipRegister --param-surface nes:macroControl3=slider --param-midi-cc nes:macroControl3=78 --param-surface nes:snNoiseMode=segmentedChoice --param-choices nes:snNoiseMode=3 --param-midi-cc nes:source1Level=84 --param-kind nes:nesDmcDirectLevel=chipRegister --param-surface nes:nesDmcDirectLevel=slider --param-midi-cc nes:nesDmcDirectLevel=116 --param-kind nes:nesDmcSampleSlot=steppedNumeric --param-surface nes:nesDmcSampleSlot=steppedSlider --param-midi-cc nes:nesDmcSampleSlot=117 --param-kind nes:nesDmcRateIndex=chipRegister --param-surface nes:nesDmcRateIndex=menu --param-choices nes:nesDmcRateIndex=16 --param-midi-cc nes:nesDmcRateIndex=118 --param-kind dmg:macroControl2=chipRegister --param-surface dmg:macroControl2=slider --param-midi-cc dmg:macroControl2=77 --param-kind dmg:macroControl3=chipRegister --param-surface dmg:macroControl3=slider --param-midi-cc dmg:macroControl3=78 --param-kind dmg:macroControl4=chipRegister --param-surface dmg:macroControl4=slider --param-midi-cc dmg:macroControl4=79 --param-surface dmg:waveShape=segmentedChoice --param-choices dmg:waveShape=5 --param-midi-cc dmg:waveShape=89 --param-surface dmg:dmgWaveLevel=segmentedChoice --param-choices dmg:dmgWaveLevel=5 --param-midi-cc dmg:dmgWaveLevel=92 --param-kind dmg:dmgStereoRoute=chipRegister --param-surface dmg:dmgStereoRoute=segmentedChoice --param-choices dmg:dmgStereoRoute=5 --param-midi-cc dmg:dmgStereoRoute=94 --param-surface dmg:snNoiseMode=segmentedChoice --param-choices dmg:snNoiseMode=3 --param-kind sid:macroControl1=chipRegister --param-surface sid:macroControl1=slider --param-midi-cc sid:macroControl1=76 --param-kind sid:macroControl3=chipRegister --param-surface sid:macroControl3=slider --param-midi-cc sid:macroControl3=78 --param-kind sid:macroControl4=chipRegister --param-surface sid:macroControl4=slider --param-midi-cc sid:macroControl4=79 --param-kind sid:envelopeDecay=chipRegister --param-surface sid:envelopeDecay=slider --param-midi-cc sid:envelopeDecay=88 --param-surface sid:waveShape=segmentedChoice --param-choices sid:waveShape=9 --param-midi-cc sid:waveShape=89 --param-kind sid:ymEnvelopeShape=chipRegister --param-surface sid:ymEnvelopeShape=menu --param-choices sid:ymEnvelopeShape=9 --param-midi-cc sid:ymEnvelopeShape=90 --param-kind sid:snNoiseMode=chipRegister --param-surface sid:snNoiseMode=segmentedChoice --param-choices sid:snNoiseMode=5 --param-midi-cc sid:snNoiseMode=91 --param-kind sid:dmgStereoRoute=chipRegister --param-surface sid:dmgStereoRoute=segmentedChoice --param-choices sid:dmgStereoRoute=3 --param-midi-cc sid:dmgStereoRoute=94 --param-kind sid:stereoSpread=chipRegister --param-surface sid:stereoSpread=slider --param-midi-cc sid:stereoSpread=93 --param-kind sid:sidFilterRouting=chipRegister --param-surface sid:sidFilterRouting=menu --param-choices sid:sidFilterRouting=9 --param-midi-cc sid:sidFilterRouting=113 --param-kind sid:sidVoice2PulseWidth=chipRegister --param-surface sid:sidVoice2PulseWidth=slider --param-midi-cc sid:sidVoice2PulseWidth=114 --param-kind sid:sidVoice3PulseWidth=chipRegister --param-surface sid:sidVoice3PulseWidth=slider --param-midi-cc sid:sidVoice3PulseWidth=115 --param-midi-cc sid:source3Level=86 --param-kind ym2149:macroControl3=chipRegister --param-surface ym2149:macroControl3=slider --param-midi-cc ym2149:macroControl3=78 --param-kind ym2149:macroControl4=chipRegister --param-surface ym2149:macroControl4=segmentedChoice --param-choices ym2149:macroControl4=3 --param-kind ym2149:envelopeDecay=chipRegister --param-surface ym2149:envelopeDecay=slider --param-midi-cc ym2149:envelopeDecay=88 --param-surface ym2149:ymEnvelopeShape=menu --param-choices ym2149:ymEnvelopeShape=21 --param-midi-cc ym2149:source4Level=87 --param-kind ym2149:stereoSpread=continuous --param-surface ym2149:stereoSpread=slider --param-midi-cc ym2149:stereoSpread=93 --param-kind sn76489:macroControl3=chipRegister --param-surface sn76489:macroControl3=slider --param-midi-cc sn76489:macroControl3=78 --param-kind sn76489:macroControl4=chipRegister --param-surface sn76489:macroControl4=slider --param-midi-cc sn76489:macroControl4=79 --param-surface sn76489:snNoiseMode=segmentedChoice --param-choices sn76489:snNoiseMode=5 --param-kind sn76489:stereoSpread=continuous --param-surface sn76489:stereoSpread=slider --param-midi-cc sn76489:stereoSpread=93 --macro-label "nes:Coin=NES Coin Blip" --macro-label "dmg:Bass=DMG Wave Bass" --macro-label "sid:Bass=SID Dirty Bass" --macro-label "ym2149:Drum=YM Noise Perc" --macro-label "sn76489:Drum=PSG Drum" --macro-source nes:Drum=0011 --macro-source dmg:Bass=0010 --macro-source sid:Drum=0010 --macro-source ym2149:Drum=1001 --macro-source sn76489:Drum=0001 --preset nes-hero-pulse --preset nes-boss-damage --preset nes-power-up-rise --preset dmg-wave-bass --preset dmg-power-up-rise --preset sid-dirty-bass --preset sid-pwm-lead --preset sid-filter-pluck --preset sid-8580-round-bass --preset sid-notch-pwm-keys --preset sid-lpbp-sweep-stack --preset sid-bphp-metal-bell --preset sid-allmode-growl --preset ym-noise-hat --preset ym-fake-chord-stack --preset ym-envelope-bell --preset sn-noise-hit --preset sn-arcade-laser --preset sn-tone-stack --preset sn-arcade-bass --preset sn-periodic-zap --preset sn-warning-alarm
    )
    set_tests_properties(chipper_render_list_descriptors_assert PROPERTIES DEPENDS chipper_render_list_descriptors_smoke)

    add_test(NAME chipper_render_list_ym_preset_catalog_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_descriptor_list_json.py ${CMAKE_CURRENT_BINARY_DIR}/chipper-descriptors.json --chip-count 17 --preset ym-three-voice-arp --preset ym-menu-beep --preset ym-bright-beep --preset ym-arcade-clang --preset ym-fast-minor-arp --preset ym-triangle-env-keys
    )
    set_tests_properties(chipper_render_list_ym_preset_catalog_assert PROPERTIES DEPENDS chipper_render_list_descriptors_smoke)

    add_test(NAME chipper_render_preset_catalog_metadata_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_preset_catalog_json.py ${CMAKE_CURRENT_BINARY_DIR}/chipper-descriptors.json --min-presets 35 --min-per-chip 4
    )
    set_tests_properties(chipper_render_preset_catalog_metadata_assert PROPERTIES DEPENDS chipper_render_list_descriptors_smoke)

    add_test(NAME chipper_render_list_macro_catalog_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_descriptor_list_json.py ${CMAKE_CURRENT_BINARY_DIR}/chipper-descriptors.json --chip-count 17 --macro-label "nes:Coin=NES Coin Blip" --macro-label "dmg:Power-Up=DMG Power-Up" --macro-label "sid:Laser=SID Sync Sweep" --macro-label "ym2149:Arp=YM Three-Voice Arp" --macro-label "sn76489:Manual=PSG Manual" --macro-label "sn76489:Drum=PSG Drum" --preset nes-triangle-bass --preset nes-noise-snare --preset nes-coin-blip --preset dmg-pocket-arp --preset dmg-noise-hat --preset dmg-pulse-lead --preset dmg-wave-glass --preset dmg-noise-kick --preset dmg-split-chord --preset sid-wide-pwm-pad --preset sid-hard-sync-saw --preset sid-gated-noise-tom --preset sn-psg-lead --preset sn-psg-coin
    )
    set_tests_properties(chipper_render_list_macro_catalog_assert PROPERTIES DEPENDS chipper_render_list_descriptors_smoke)

    add_test(NAME chipper_render_factory_presets_audible_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_factory_presets_audible.py --renderer $<TARGET_FILE:chipper_render> --work-dir ${CMAKE_CURRENT_BINARY_DIR}/factory-preset-audibility --rate 48000 --seconds 0.05 --note 69 --min-peak 0.005
    )

    add_test(NAME chipper_render_list_sid_voice_waves_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_descriptor_list_json.py ${CMAKE_CURRENT_BINARY_DIR}/chipper-descriptors.json --chip-count 17 --chip sid --param-kind sid:sidVoice2WaveShape=chipRegister --param-surface sid:sidVoice2WaveShape=menu --param-choices sid:sidVoice2WaveShape=9 --param-midi-cc sid:sidVoice2WaveShape=95 --param-kind sid:sidVoice3WaveShape=chipRegister --param-surface sid:sidVoice3WaveShape=menu --param-choices sid:sidVoice3WaveShape=9 --param-midi-cc sid:sidVoice3WaveShape=96 --param-kind sid:sidVoice2PulseWidth=chipRegister --param-surface sid:sidVoice2PulseWidth=slider --param-midi-cc sid:sidVoice2PulseWidth=114 --param-kind sid:sidVoice3PulseWidth=chipRegister --param-surface sid:sidVoice3PulseWidth=slider --param-midi-cc sid:sidVoice3PulseWidth=115 --param-kind sid:sidAttack=chipRegister --param-surface sid:sidAttack=steppedSlider --param-choices sid:sidAttack=17 --param-midi-cc sid:sidAttack=97 --param-kind sid:sidDecay=chipRegister --param-surface sid:sidDecay=steppedSlider --param-choices sid:sidDecay=17 --param-midi-cc sid:sidDecay=99 --param-kind sid:sidSustain=chipRegister --param-surface sid:sidSustain=steppedSlider --param-choices sid:sidSustain=17 --param-midi-cc sid:sidSustain=100 --param-kind sid:sidRelease=chipRegister --param-surface sid:sidRelease=steppedSlider --param-choices sid:sidRelease=17 --param-midi-cc sid:sidRelease=98 --param-kind sid:sidVoice2Attack=chipRegister --param-surface sid:sidVoice2Attack=steppedSlider --param-choices sid:sidVoice2Attack=17 --param-midi-cc sid:sidVoice2Attack=105 --param-kind sid:sidVoice2Decay=chipRegister --param-surface sid:sidVoice2Decay=steppedSlider --param-choices sid:sidVoice2Decay=17 --param-midi-cc sid:sidVoice2Decay=106 --param-kind sid:sidVoice2Sustain=chipRegister --param-surface sid:sidVoice2Sustain=steppedSlider --param-choices sid:sidVoice2Sustain=17 --param-midi-cc sid:sidVoice2Sustain=107 --param-kind sid:sidVoice2Release=chipRegister --param-surface sid:sidVoice2Release=steppedSlider --param-choices sid:sidVoice2Release=17 --param-midi-cc sid:sidVoice2Release=108 --param-kind sid:sidVoice3Attack=chipRegister --param-surface sid:sidVoice3Attack=steppedSlider --param-choices sid:sidVoice3Attack=17 --param-midi-cc sid:sidVoice3Attack=109 --param-kind sid:sidVoice3Decay=chipRegister --param-surface sid:sidVoice3Decay=steppedSlider --param-choices sid:sidVoice3Decay=17 --param-midi-cc sid:sidVoice3Decay=110 --param-kind sid:sidVoice3Sustain=chipRegister --param-surface sid:sidVoice3Sustain=steppedSlider --param-choices sid:sidVoice3Sustain=17 --param-midi-cc sid:sidVoice3Sustain=111 --param-kind sid:sidVoice3Release=chipRegister --param-surface sid:sidVoice3Release=steppedSlider --param-choices sid:sidVoice3Release=17 --param-midi-cc sid:sidVoice3Release=112
    )
    set_tests_properties(chipper_render_list_sid_voice_waves_assert PROPERTIES DEPENDS chipper_render_list_descriptors_smoke)

    add_test(NAME chipper_render_list_ym_channel_mix_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_descriptor_list_json.py ${CMAKE_CURRENT_BINARY_DIR}/chipper-descriptors.json --chip-count 17 --chip ym2149 --param-kind ym2149:ymChannelAMix=chipRegister --param-surface ym2149:ymChannelAMix=menu --param-choices ym2149:ymChannelAMix=5 --param-midi-cc ym2149:ymChannelAMix=101 --param-kind ym2149:ymChannelBMix=chipRegister --param-surface ym2149:ymChannelBMix=menu --param-choices ym2149:ymChannelBMix=5 --param-midi-cc ym2149:ymChannelBMix=102 --param-kind ym2149:ymChannelCMix=chipRegister --param-surface ym2149:ymChannelCMix=menu --param-choices ym2149:ymChannelCMix=5 --param-midi-cc ym2149:ymChannelCMix=103
    )
    set_tests_properties(chipper_render_list_ym_channel_mix_assert PROPERTIES DEPENDS chipper_render_list_descriptors_smoke)

    add_test(NAME chipper_render_list_cc_map_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_descriptor_list_json.py ${CMAKE_CURRENT_BINARY_DIR}/chipper-descriptors.json --chip-count 17 --midi-cc-count 49 --midi-cc 70=chipMode --midi-cc 71=accuracy --midi-cc 72=clockHz --midi-cc 73=outputDb --midi-cc 74=macro --midi-cc 75=playMode --midi-cc 76=macroControl1 --midi-cc 77=macroControl2 --midi-cc 78=macroControl3 --midi-cc 79=macroControl4 --midi-cc 80=source1Enabled --midi-cc 81=source2Enabled --midi-cc 82=source3Enabled --midi-cc 83=source4Enabled --midi-cc 84=source1Level --midi-cc 85=source2Level --midi-cc 86=source3Level --midi-cc 87=source4Level --midi-cc 88=envelopeDecay --midi-cc 89=waveShape --midi-cc 90=ymEnvelopeShape --midi-cc 91=snNoiseMode --midi-cc 92=dmgWaveLevel --midi-cc 93=stereoSpread --midi-cc 94=dmgStereoRoute --midi-cc 95=sidVoice2WaveShape --midi-cc 96=sidVoice3WaveShape --midi-cc 97=sidAttack --midi-cc 98=sidRelease --midi-cc 99=sidDecay --midi-cc 100=sidSustain --midi-cc 101=ymChannelAMix --midi-cc 102=ymChannelBMix --midi-cc 103=ymChannelCMix --midi-cc 104=pulse2Duty --midi-cc 105=sidVoice2Attack --midi-cc 106=sidVoice2Decay --midi-cc 107=sidVoice2Sustain --midi-cc 108=sidVoice2Release --midi-cc 109=sidVoice3Attack --midi-cc 110=sidVoice3Decay --midi-cc 111=sidVoice3Sustain --midi-cc 112=sidVoice3Release --midi-cc 113=sidFilterRouting --midi-cc 114=sidVoice2PulseWidth --midi-cc 115=sidVoice3PulseWidth --midi-cc 116=nesDmcDirectLevel --midi-cc 117=nesDmcSampleSlot --midi-cc 118=nesDmcRateIndex
    )
    set_tests_properties(chipper_render_list_cc_map_assert PROPERTIES DEPENDS chipper_render_list_descriptors_smoke)

    add_test(NAME chipper_render_describe_nes_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_descriptor_list_json.py ${CMAKE_CURRENT_BINARY_DIR}/nes-descriptor.json --chip-count 1 --chip nes --implemented nes=1 --param-kind nes:macroControl1=chipRegister --param-surface nes:macroControl1=segmentedChoice --param-choices nes:macroControl1=4 --param-midi-cc nes:macroControl1=76 --param-kind nes:pulse2Duty=chipRegister --param-surface nes:pulse2Duty=segmentedChoice --param-choices nes:pulse2Duty=5 --param-midi-cc nes:pulse2Duty=104 --param-surface nes:snNoiseMode=segmentedChoice --param-choices nes:snNoiseMode=3 --param-kind nes:nesDmcDirectLevel=chipRegister --param-surface nes:nesDmcDirectLevel=slider --param-midi-cc nes:nesDmcDirectLevel=116 --param-kind nes:nesDmcSampleSlot=steppedNumeric --param-surface nes:nesDmcSampleSlot=steppedSlider --param-midi-cc nes:nesDmcSampleSlot=117 --param-kind nes:nesDmcRateIndex=chipRegister --param-surface nes:nesDmcRateIndex=menu --param-choices nes:nesDmcRateIndex=16 --param-midi-cc nes:nesDmcRateIndex=118 --macro-label "nes:Coin=NES Coin Blip" --preset nes-hero-pulse
    )
    set_tests_properties(chipper_render_describe_nes_assert PROPERTIES DEPENDS chipper_render_describe_nes_smoke)

    add_test(NAME chipper_render_nes_json_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/nes-smoke.json --implemented-contains partial --verification-badge "Verified partial" --cycle-accurate 0 --macro Manual --min-peak 0.1 --min-rms 0.05 --register-writes 0 --note-events 0 --min-zero-crossings 30 --max-zero-crossings 120 --descriptor-param-kind macroControl1=chipRegister --descriptor-param-surface macroControl1=segmentedChoice --descriptor-param-label "macroControl1=Pulse 1 Duty" --descriptor-param-choices macroControl1=4 --descriptor-param-kind pulse2Duty=chipRegister --descriptor-param-surface pulse2Duty=segmentedChoice --descriptor-param-label "pulse2Duty=Pulse 2 Duty" --descriptor-param-choices pulse2Duty=5 --descriptor-param-surface snNoiseMode=segmentedChoice --descriptor-param-label "snNoiseMode=Noise Mode" --descriptor-param-choices snNoiseMode=3
    )
    set_tests_properties(chipper_render_nes_json_assert PROPERTIES DEPENDS chipper_render_nes_smoke)

    add_test(NAME chipper_render_nes_output_trim_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/nes-output-trim-smoke.json --implemented-contains partial --macro Manual --output-db -24 --output-gain 0.0630957 --min-peak 0.01 --max-peak 0.08 --min-rms 0.003 --max-rms 0.02 --register-writes 0 --note-events 0
    )
    set_tests_properties(chipper_render_nes_output_trim_assert PROPERTIES DEPENDS chipper_render_nes_output_trim_smoke)

    add_test(NAME chipper_render_sid_json_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sid-smoke.json --implemented-contains partial --macro Lead --play-mode "Big Mono" --min-peak 0.1 --min-rms 0.05 --register-writes 0 --note-events 0 --core-min waveformBits=64 --core-max waveformBits=64 --core-min pulseWidthRegister=2375 --core-max pulseWidthRegister=2375 --core-min filterCutoffRegister=1474 --core-max filterCutoffRegister=1474 --core-min filterModeChoice=1 --core-max filterModeChoice=1 --core-min filterModeBits=16 --core-max filterModeBits=16 --core-min filterResonanceNibble=5 --core-max filterResonanceNibble=5 --core-min filterRoutingRegister=87 --core-max filterRoutingRegister=87 --core-min modModeChoice=1 --core-max modModeChoice=1 --core-min modBits1=0 --core-max modBits1=0 --core-min modBits2=0 --core-max modBits2=0 --core-min sustain0=11 --core-max sustain0=11 --core-min frequency0=7493 --core-max frequency0=7493 --descriptor-param-kind macroControl1=chipRegister --descriptor-param-surface macroControl1=slider --descriptor-param-label "macroControl1=Pulse Width" --descriptor-param-kind waveShape=chipRegister --descriptor-param-surface waveShape=segmentedChoice --descriptor-param-choices waveShape=9 --descriptor-param-kind ymEnvelopeShape=chipRegister --descriptor-param-surface ymEnvelopeShape=menu --descriptor-param-choices ymEnvelopeShape=9 --descriptor-param-label "ymEnvelopeShape=Filter Mode" --descriptor-param-kind snNoiseMode=chipRegister --descriptor-param-surface snNoiseMode=segmentedChoice --descriptor-param-choices snNoiseMode=5 --descriptor-param-label "snNoiseMode=Osc Interaction" --descriptor-param-kind stereoSpread=chipRegister --descriptor-param-surface stereoSpread=slider --descriptor-param-label "stereoSpread=Resonance"
    )
    set_tests_properties(chipper_render_sid_json_assert PROPERTIES DEPENDS chipper_render_sid_smoke)

    add_test(NAME chipper_render_sid_chip_poly_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sid-chip-poly-smoke.json --implemented-contains partial --macro Manual --play-mode "Chip Poly" --register-writes 0 --note-events 3 --min-peak 0.1 --core-min activeChannels=3 --core-max activeChannels=3 --core-min assignedNote0=60 --core-max assignedNote0=60 --core-min assignedNote1=64 --core-max assignedNote1=64 --core-min assignedNote2=67 --core-max assignedNote2=67 --core-min frequency0=4455 --core-max frequency0=4455 --core-min frequency1=5613 --core-max frequency1=5613 --core-min frequency2=6675 --core-max frequency2=6675
    )
    set_tests_properties(chipper_render_sid_chip_poly_assert PROPERTIES DEPENDS chipper_render_sid_chip_poly_smoke)

    add_test(NAME chipper_render_sid_wave_noise_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sid-wave-noise-smoke.json --implemented-contains partial --macro Manual --min-peak 0.05 --register-writes 0 --note-events 0 --core-min waveformChoice=4 --core-max waveformChoice=4 --core-min waveformBits=128 --core-max waveformBits=128 --core-min sourceEnabled1=0 --core-max sourceEnabled1=0 --core-min sourceEnabled2=0 --core-max sourceEnabled2=0 --core-min sourceEnabled3=1 --core-max sourceEnabled3=1 --core-min control2=128 --core-max control2=128
    )
    set_tests_properties(chipper_render_sid_wave_noise_assert PROPERTIES DEPENDS chipper_render_sid_wave_noise_smoke)

    add_test(NAME chipper_render_sid_voice_waves_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sid-voice-waves-smoke.json --implemented-contains partial --macro Manual --min-peak 0.05 --register-writes 0 --note-events 0 --core-min waveformChoice0=3 --core-max waveformChoice0=3 --core-min waveformChoice1=2 --core-max waveformChoice1=2 --core-min waveformChoice2=4 --core-max waveformChoice2=4 --core-min waveformBits0=64 --core-max waveformBits0=64 --core-min waveformBits1=32 --core-max waveformBits1=32 --core-min waveformBits2=128 --core-max waveformBits2=128 --core-min control0=64 --core-max control0=64 --core-min control1=32 --core-max control1=32 --core-min control2=128 --core-max control2=128 --descriptor-param-kind sidVoice2WaveShape=chipRegister --descriptor-param-surface sidVoice2WaveShape=menu --descriptor-param-choices sidVoice2WaveShape=9 --descriptor-param-kind sidVoice3WaveShape=chipRegister --descriptor-param-surface sidVoice3WaveShape=menu --descriptor-param-choices sidVoice3WaveShape=9
    )
    set_tests_properties(chipper_render_sid_voice_waves_assert PROPERTIES DEPENDS chipper_render_sid_voice_waves_smoke)

    add_test(NAME chipper_render_sid_combined_waves_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sid-combined-waves-smoke.json --implemented-contains partial --macro Manual --min-peak 0.02 --register-writes 0 --note-events 0 --core-min waveformChoice0=5 --core-max waveformChoice0=5 --core-min waveformChoice1=7 --core-max waveformChoice1=7 --core-min waveformChoice2=8 --core-max waveformChoice2=8 --core-min waveformBits0=48 --core-max waveformBits0=48 --core-min waveformBits1=96 --core-max waveformBits1=96 --core-min waveformBits2=112 --core-max waveformBits2=112 --core-min control0=48 --core-max control0=48 --core-min control1=96 --core-max control1=96 --core-min control2=112 --core-max control2=112
    )
    set_tests_properties(chipper_render_sid_combined_waves_assert PROPERTIES DEPENDS chipper_render_sid_combined_waves_smoke)

    add_test(NAME chipper_render_sid_voice3_big_mono_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sid-voice3-big-mono-smoke.json --implemented-contains partial --macro Bass --play-mode "Big Mono" --min-peak 0.01 --register-writes 0 --note-events 0 --core-min sourceEnabled1=0 --core-max sourceEnabled1=0 --core-min sourceEnabled2=0 --core-max sourceEnabled2=0 --core-min sourceEnabled3=1 --core-max sourceEnabled3=1 --core-min waveformChoice2=2 --core-max waveformChoice2=2 --core-min waveformBits2=32 --core-max waveformBits2=32 --core-min control2=32 --core-max control2=32 --core-min frequency2=5613 --core-max frequency2=5613
    )
    set_tests_properties(chipper_render_sid_voice3_big_mono_assert PROPERTIES DEPENDS chipper_render_sid_voice3_big_mono_smoke)

    add_test(NAME chipper_render_sid_per_voice_pulse_width_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sid-per-voice-pw-smoke.json --implemented-contains partial --macro Manual --min-peak 0.05 --register-writes 0 --note-events 0 --core-min pulseWidthRegister0=2048 --core-max pulseWidthRegister0=2048 --core-min pulseWidthRegister1=1024 --core-max pulseWidthRegister1=1024 --core-min pulseWidthRegister2=3071 --core-max pulseWidthRegister2=3071 --core-min pulseWidth0=2048 --core-max pulseWidth0=2048 --core-min pulseWidth1=1024 --core-max pulseWidth1=1024 --core-min pulseWidth2=3071 --core-max pulseWidth2=3071 --descriptor-param-kind sidVoice2PulseWidth=chipRegister --descriptor-param-surface sidVoice2PulseWidth=slider --descriptor-param-label "sidVoice2PulseWidth=Voice 2 PW" --descriptor-param-kind sidVoice3PulseWidth=chipRegister --descriptor-param-surface sidVoice3PulseWidth=slider --descriptor-param-label "sidVoice3PulseWidth=Voice 3 PW"
    )
    set_tests_properties(chipper_render_sid_per_voice_pulse_width_assert PROPERTIES DEPENDS chipper_render_sid_per_voice_pulse_width_smoke)

    add_test(NAME chipper_render_sid_mod_sync_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sid-mod-sync-smoke.json --implemented-contains partial --macro Manual --min-peak 0.05 --register-writes 0 --note-events 0 --core-min waveformChoice=2 --core-max waveformChoice=2 --core-min waveformBits=32 --core-max waveformBits=32 --core-min modModeChoice=2 --core-max modModeChoice=2 --core-min modBits0=0 --core-max modBits0=0 --core-min modBits1=2 --core-max modBits1=2 --core-min modBits2=2 --core-max modBits2=2 --core-min control0=32 --core-max control0=32 --core-min control1=34 --core-max control1=34 --core-min control2=34 --core-max control2=34 --descriptor-param-kind snNoiseMode=chipRegister --descriptor-param-surface snNoiseMode=segmentedChoice --descriptor-param-choices snNoiseMode=5
    )
    set_tests_properties(chipper_render_sid_mod_sync_assert PROPERTIES DEPENDS chipper_render_sid_mod_sync_smoke)

    add_test(NAME chipper_render_sid_mod_ring_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sid-mod-ring-smoke.json --implemented-contains partial --macro Manual --min-peak 0.05 --register-writes 0 --note-events 0 --core-min waveformChoice=1 --core-max waveformChoice=1 --core-min waveformBits=16 --core-max waveformBits=16 --core-min modModeChoice=3 --core-max modModeChoice=3 --core-min modBits0=0 --core-max modBits0=0 --core-min modBits1=4 --core-max modBits1=4 --core-min modBits2=4 --core-max modBits2=4 --core-min control0=16 --core-max control0=16 --core-min control1=20 --core-max control1=20 --core-min control2=20 --core-max control2=20
    )
    set_tests_properties(chipper_render_sid_mod_ring_assert PROPERTIES DEPENDS chipper_render_sid_mod_ring_smoke)

    add_test(NAME chipper_render_sid_model_6581_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sid-model-6581-smoke.json --implemented-contains partial --macro Manual --min-peak 0.001 --register-writes 0 --note-events 0 --core-min sidModelChoice=1 --core-max sidModelChoice=1 --core-min sidModelNumber=6581 --core-max sidModelNumber=6581 --core-min filterCutoffRegister=1330 --core-max filterCutoffRegister=1332 --core-min modelCutoffHz=3380 --core-max modelCutoffHz=3420 --core-min modelFilterDamping=0.84 --core-max modelFilterDamping=0.86 --core-min modelOutputDrive=1.32 --core-max modelOutputDrive=1.32 --descriptor-param-kind dmgStereoRoute=chipRegister --descriptor-param-surface dmgStereoRoute=segmentedChoice --descriptor-param-choices dmgStereoRoute=3 --descriptor-param-label "dmgStereoRoute=SID Model"
    )
    set_tests_properties(chipper_render_sid_model_6581_assert PROPERTIES DEPENDS chipper_render_sid_model_6581_smoke)

    add_test(NAME chipper_render_sid_model_8580_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sid-model-8580-smoke.json --implemented-contains partial --macro Manual --min-peak 0.001 --register-writes 0 --note-events 0 --core-min sidModelChoice=2 --core-max sidModelChoice=2 --core-min sidModelNumber=8580 --core-max sidModelNumber=8580 --core-min filterCutoffRegister=1330 --core-max filterCutoffRegister=1332 --core-min modelCutoffHz=7220 --core-max modelCutoffHz=7270 --core-min modelFilterDamping=0.49 --core-max modelFilterDamping=0.51 --core-min modelOutputDrive=0.96 --core-max modelOutputDrive=0.96
    )
    set_tests_properties(chipper_render_sid_model_8580_assert PROPERTIES DEPENDS chipper_render_sid_model_8580_smoke)

    add_test(NAME chipper_render_sid_adsr_fast_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sid-adsr-fast-smoke.json --implemented-contains partial --macro Manual --min-peak 0.05 --register-writes 0 --note-events 0 --core-min attackDecayRegister=0 --core-max attackDecayRegister=0 --core-min sustainReleaseRegister=192 --core-max sustainReleaseRegister=192 --core-min attack0=0 --core-max attack0=0 --core-min decay0=0 --core-max decay0=0 --core-min sustain0=12 --core-max sustain0=12 --core-min release0=0 --core-max release0=0 --descriptor-param-kind envelopeDecay=chipRegister --descriptor-param-surface envelopeDecay=slider --descriptor-param-label "envelopeDecay=ADSR Speed"
    )
    set_tests_properties(chipper_render_sid_adsr_fast_assert PROPERTIES DEPENDS chipper_render_sid_adsr_fast_smoke)

    add_test(NAME chipper_render_sid_adsr_nibbles_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sid-adsr-nibbles-smoke.json --implemented-contains partial --macro Manual --min-peak 0.005 --register-writes 0 --note-events 0 --core-min attackChoice=16 --core-max attackChoice=16 --core-min decayChoice=4 --core-max decayChoice=4 --core-min sustainChoice=13 --core-max sustainChoice=13 --core-min releaseChoice=1 --core-max releaseChoice=1 --core-min attackChoice0=16 --core-max attackChoice0=16 --core-min decayChoice0=4 --core-max decayChoice0=4 --core-min sustainChoice0=13 --core-max sustainChoice0=13 --core-min releaseChoice0=1 --core-max releaseChoice0=1 --core-min attackChoice1=7 --core-max attackChoice1=7 --core-min decayChoice1=3 --core-max decayChoice1=3 --core-min sustainChoice1=10 --core-max sustainChoice1=10 --core-min releaseChoice1=5 --core-max releaseChoice1=5 --core-min attackChoice2=2 --core-max attackChoice2=2 --core-min decayChoice2=6 --core-max decayChoice2=6 --core-min sustainChoice2=4 --core-max sustainChoice2=4 --core-min releaseChoice2=9 --core-max releaseChoice2=9 --core-min attackNibbleResolved=15 --core-max attackNibbleResolved=15 --core-min decayNibbleResolved=3 --core-max decayNibbleResolved=3 --core-min sustainNibbleResolved=12 --core-max sustainNibbleResolved=12 --core-min releaseNibbleResolved=0 --core-max releaseNibbleResolved=0 --core-min attackDecayRegister=243 --core-max attackDecayRegister=243 --core-min sustainReleaseRegister=192 --core-max sustainReleaseRegister=192 --core-min attackDecayRegister0=243 --core-max attackDecayRegister0=243 --core-min sustainReleaseRegister0=192 --core-max sustainReleaseRegister0=192 --core-min attackDecayRegister1=98 --core-max attackDecayRegister1=98 --core-min sustainReleaseRegister1=148 --core-max sustainReleaseRegister1=148 --core-min attackDecayRegister2=21 --core-max attackDecayRegister2=21 --core-min sustainReleaseRegister2=56 --core-max sustainReleaseRegister2=56 --core-min attack0=15 --core-max attack0=15 --core-min decay0=3 --core-max decay0=3 --core-min sustain0=12 --core-max sustain0=12 --core-min release0=0 --core-max release0=0 --descriptor-param-kind sidAttack=chipRegister --descriptor-param-surface sidAttack=steppedSlider --descriptor-param-choices sidAttack=17 --descriptor-param-kind sidDecay=chipRegister --descriptor-param-surface sidDecay=steppedSlider --descriptor-param-choices sidDecay=17 --descriptor-param-kind sidSustain=chipRegister --descriptor-param-surface sidSustain=steppedSlider --descriptor-param-choices sidSustain=17 --descriptor-param-kind sidRelease=chipRegister --descriptor-param-surface sidRelease=steppedSlider --descriptor-param-choices sidRelease=17 --descriptor-param-kind sidVoice2Attack=chipRegister --descriptor-param-surface sidVoice2Attack=steppedSlider --descriptor-param-choices sidVoice2Attack=17 --descriptor-param-kind sidVoice2Decay=chipRegister --descriptor-param-surface sidVoice2Decay=steppedSlider --descriptor-param-choices sidVoice2Decay=17 --descriptor-param-kind sidVoice2Sustain=chipRegister --descriptor-param-surface sidVoice2Sustain=steppedSlider --descriptor-param-choices sidVoice2Sustain=17 --descriptor-param-kind sidVoice2Release=chipRegister --descriptor-param-surface sidVoice2Release=steppedSlider --descriptor-param-choices sidVoice2Release=17 --descriptor-param-kind sidVoice3Attack=chipRegister --descriptor-param-surface sidVoice3Attack=steppedSlider --descriptor-param-choices sidVoice3Attack=17 --descriptor-param-kind sidVoice3Decay=chipRegister --descriptor-param-surface sidVoice3Decay=steppedSlider --descriptor-param-choices sidVoice3Decay=17 --descriptor-param-kind sidVoice3Sustain=chipRegister --descriptor-param-surface sidVoice3Sustain=steppedSlider --descriptor-param-choices sidVoice3Sustain=17 --descriptor-param-kind sidVoice3Release=chipRegister --descriptor-param-surface sidVoice3Release=steppedSlider --descriptor-param-choices sidVoice3Release=17
    )
    set_tests_properties(chipper_render_sid_adsr_nibbles_assert PROPERTIES DEPENDS chipper_render_sid_adsr_nibbles_smoke)

    add_test(NAME chipper_render_sid_pulse_width_min_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sid-pulse-width-min-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --core-min pulseWidthRegister=0 --core-max pulseWidthRegister=0 --core-min pulseWidth0=0 --core-max pulseWidth0=0
    )
    set_tests_properties(chipper_render_sid_pulse_width_min_assert PROPERTIES DEPENDS chipper_render_sid_pulse_width_min_smoke)

    add_test(NAME chipper_render_sid_pulse_width_max_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sid-pulse-width-max-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --core-min pulseWidthRegister=4095 --core-max pulseWidthRegister=4095 --core-min pulseWidth0=4095 --core-max pulseWidth0=4095
    )
    set_tests_properties(chipper_render_sid_pulse_width_max_assert PROPERTIES DEPENDS chipper_render_sid_pulse_width_max_smoke)

    add_test(NAME chipper_render_sid_sources_off_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sid-sources-off-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --max-peak 0.001 --max-rms 0.001 --core-min sourceEnabled1=0 --core-max sourceEnabled1=0 --core-min sourceEnabled2=0 --core-max sourceEnabled2=0 --core-min sourceEnabled3=0 --core-max sourceEnabled3=0
    )
    set_tests_properties(chipper_render_sid_sources_off_assert PROPERTIES DEPENDS chipper_render_sid_sources_off_smoke)

    add_test(NAME chipper_render_sid_filter_bp_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sid-filter-bp-smoke.json --implemented-contains partial --macro Manual --min-peak 0.001 --register-writes 0 --note-events 0 --core-min filterModeChoice=2 --core-max filterModeChoice=2 --core-min filterModeBits=32 --core-max filterModeBits=32 --core-min filterModeVolume=47 --core-max filterModeVolume=47 --core-min filterResonanceNibble=15 --core-max filterResonanceNibble=15 --core-min filterRoutingRegister=247 --core-max filterRoutingRegister=247
    )
    set_tests_properties(chipper_render_sid_filter_bp_assert PROPERTIES DEPENDS chipper_render_sid_filter_bp_smoke)

    add_test(NAME chipper_render_sid_filter_bypass_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sid-filter-bypass-smoke.json --implemented-contains partial --macro Manual --min-peak 0.1 --register-writes 0 --note-events 0 --core-min filterModeChoice=4 --core-max filterModeChoice=4 --core-min filterModeBits=0 --core-max filterModeBits=0 --core-min filterModeVolume=15 --core-max filterModeVolume=15 --core-min filterResonanceNibble=0 --core-max filterResonanceNibble=0 --core-min filterRoutingRegister=7 --core-max filterRoutingRegister=7
    )
    set_tests_properties(chipper_render_sid_filter_bypass_assert PROPERTIES DEPENDS chipper_render_sid_filter_bypass_smoke)

    add_test(NAME chipper_render_sid_filter_notch_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sid-filter-notch-smoke.json --implemented-contains partial --macro Manual --min-peak 0.001 --register-writes 0 --note-events 0 --core-min filterModeChoice=5 --core-max filterModeChoice=5 --core-min filterModeBits=80 --core-max filterModeBits=80 --core-min filterModeVolume=95 --core-max filterModeVolume=95 --core-min filterResonanceNibble=15 --core-max filterResonanceNibble=15 --core-min filterRoutingRegister=247 --core-max filterRoutingRegister=247
    )
    set_tests_properties(chipper_render_sid_filter_notch_assert PROPERTIES DEPENDS chipper_render_sid_filter_notch_smoke)

    add_test(NAME chipper_render_sid_filter_lpbp_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sid-filter-lpbp-smoke.json --implemented-contains partial --macro Manual --min-peak 0.001 --register-writes 0 --note-events 0 --core-min filterModeChoice=6 --core-max filterModeChoice=6 --core-min filterModeBits=48 --core-max filterModeBits=48 --core-min filterModeVolume=63 --core-max filterModeVolume=63 --core-min filterResonanceNibble=15 --core-max filterResonanceNibble=15 --core-min filterRoutingRegister=247 --core-max filterRoutingRegister=247
    )
    set_tests_properties(chipper_render_sid_filter_lpbp_assert PROPERTIES DEPENDS chipper_render_sid_filter_lpbp_smoke)

    add_test(NAME chipper_render_sid_filter_bphp_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sid-filter-bphp-smoke.json --implemented-contains partial --macro Manual --min-peak 0.001 --register-writes 0 --note-events 0 --core-min filterModeChoice=7 --core-max filterModeChoice=7 --core-min filterModeBits=96 --core-max filterModeBits=96 --core-min filterModeVolume=111 --core-max filterModeVolume=111 --core-min filterResonanceNibble=15 --core-max filterResonanceNibble=15 --core-min filterRoutingRegister=247 --core-max filterRoutingRegister=247
    )
    set_tests_properties(chipper_render_sid_filter_bphp_assert PROPERTIES DEPENDS chipper_render_sid_filter_bphp_smoke)

    add_test(NAME chipper_render_sid_filter_all_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sid-filter-all-smoke.json --implemented-contains partial --macro Manual --min-peak 0.001 --register-writes 0 --note-events 0 --core-min filterModeChoice=8 --core-max filterModeChoice=8 --core-min filterModeBits=112 --core-max filterModeBits=112 --core-min filterModeVolume=127 --core-max filterModeVolume=127 --core-min filterResonanceNibble=15 --core-max filterResonanceNibble=15 --core-min filterRoutingRegister=247 --core-max filterRoutingRegister=247
    )
    set_tests_properties(chipper_render_sid_filter_all_assert PROPERTIES DEPENDS chipper_render_sid_filter_all_smoke)

    add_test(NAME chipper_render_sid_filter_route_v3_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sid-filter-route-v3-smoke.json --implemented-contains partial --macro Manual --min-peak 0.01 --register-writes 0 --note-events 0 --core-min filterRoutingChoice=4 --core-max filterRoutingChoice=4 --core-min filterRoutingBits=4 --core-max filterRoutingBits=4 --core-min filterRoutingRegister=4 --core-max filterRoutingRegister=4 --descriptor-param-kind sidFilterRouting=chipRegister --descriptor-param-surface sidFilterRouting=menu --descriptor-param-label "sidFilterRouting=Filter Routing" --descriptor-param-choices sidFilterRouting=9
    )
    set_tests_properties(chipper_render_sid_filter_route_v3_assert PROPERTIES DEPENDS chipper_render_sid_filter_route_v3_smoke)

    add_test(NAME chipper_render_sid_filter_exact_bits_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sid-filter-exact-bits-smoke.json --implemented-contains partial --macro Manual --min-peak 0.001 --register-writes 0 --note-events 0 --core-min filterModeChoice=5 --core-max filterModeChoice=5 --core-min filterModeBits=80 --core-max filterModeBits=80 --core-min filterModeVolume=95 --core-max filterModeVolume=95 --core-min filterRoutingChoice=6 --core-max filterRoutingChoice=6 --core-min filterRoutingBits=5 --core-max filterRoutingBits=5 --core-min filterRoutingRegister=5 --core-max filterRoutingRegister=5
    )
    set_tests_properties(chipper_render_sid_filter_exact_bits_assert PROPERTIES DEPENDS chipper_render_sid_filter_exact_bits_smoke)

    add_test(NAME chipper_render_sid_follow_aliases_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sid-follow-aliases-smoke.json --implemented-contains partial --macro Lead --register-writes 0 --note-events 0 --min-peak 0.05 --core-min waveformChoice=0 --core-max waveformChoice=0 --core-min waveformChoice1=0 --core-max waveformChoice1=0 --core-min waveformChoice2=0 --core-max waveformChoice2=0 --core-min waveformBits=64 --core-max waveformBits=64 --core-min filterModeChoice=0 --core-max filterModeChoice=0 --core-min filterModeBits=16 --core-max filterModeBits=16 --core-min filterRoutingChoice=0 --core-max filterRoutingChoice=0 --core-min filterRoutingBits=7 --core-max filterRoutingBits=7 --core-min modModeChoice=0 --core-max modModeChoice=0 --core-min modBits1=0 --core-max modBits1=0 --core-min modBits2=0 --core-max modBits2=0 --core-min sidModelChoice=2 --core-max sidModelChoice=2 --core-min attackChoice=0 --core-max attackChoice=0 --core-min decayChoice=0 --core-max decayChoice=0 --core-min sustainChoice=0 --core-max sustainChoice=0 --core-min releaseChoice=0 --core-max releaseChoice=0
    )
    set_tests_properties(chipper_render_sid_follow_aliases_assert PROPERTIES DEPENDS chipper_render_sid_follow_aliases_smoke)

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

    add_test(NAME chipper_render_nes_pulse2_duty_override_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/nes-pulse2-duty-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --min-peak 0.1 --core-min pulseDuty1=0 --core-max pulseDuty1=0 --core-min pulseDuty2=3 --core-max pulseDuty2=3 --core-min pulse2DutyChoice=4 --core-max pulse2DutyChoice=4 --descriptor-param-kind pulse2Duty=chipRegister --descriptor-param-surface pulse2Duty=segmentedChoice --descriptor-param-label "pulse2Duty=Pulse 2 Duty" --descriptor-param-choices pulse2Duty=5
    )
    set_tests_properties(chipper_render_nes_pulse2_duty_override_assert PROPERTIES DEPENDS chipper_render_nes_pulse2_duty_override_smoke)

    add_test(NAME chipper_render_nes_pulse2_duty_follow_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/nes-pulse2-duty-follow-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --min-peak 0.1 --core-min pulseDuty1=0 --core-max pulseDuty1=0 --core-min pulseDuty2=1 --core-max pulseDuty2=1 --core-min pulse2DutyChoice=0 --core-max pulse2DutyChoice=0
    )
    set_tests_properties(chipper_render_nes_pulse2_duty_follow_assert PROPERTIES DEPENDS chipper_render_nes_pulse2_duty_follow_smoke)

    add_test(NAME chipper_render_nes_sources_off_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/nes-sources-off-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --max-peak 0.001 --max-rms 0.001 --core-min sourceEnabled1=0 --core-max sourceEnabled1=0 --core-min sourceEnabled2=0 --core-max sourceEnabled2=0 --core-min sourceEnabled3=0 --core-max sourceEnabled3=0 --core-min sourceEnabled4=0 --core-max sourceEnabled4=0 --core-min enabled0=0 --core-max enabled0=0 --core-min enabled1=0 --core-max enabled1=0 --core-min enabled2=0 --core-max enabled2=0 --core-min enabled3=0 --core-max enabled3=0
    )
    set_tests_properties(chipper_render_nes_sources_off_assert PROPERTIES DEPENDS chipper_render_nes_sources_off_smoke)

    add_test(NAME chipper_render_nes_envelope_decay_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/nes-envelope-decay-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --min-peak 0.1 --core-min envelopeDecayControl=1 --core-max envelopeDecayControl=1 --core-min envelopePeriod0=1 --core-max envelopePeriod0=1 --core-min envelopeConstant0=0 --core-max envelopeConstant0=0
    )
    set_tests_properties(chipper_render_nes_envelope_decay_assert PROPERTIES DEPENDS chipper_render_nes_envelope_decay_smoke)

    add_test(NAME chipper_render_nes_noise_long_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/nes-noise-long-smoke.json --implemented-contains partial --macro Drum --register-writes 0 --note-events 0 --core-min noiseModeChoice=1 --core-max noiseModeChoice=1 --core-min noiseShortMode=0 --core-max noiseShortMode=0 --core-min noiseRegister=0 --core-max noiseRegister=0 --core-min noisePeriod=0 --core-max noisePeriod=0 --descriptor-param-kind macroControl3=chipRegister --descriptor-param-surface macroControl3=slider --descriptor-param-label "macroControl3=Noise Period"
    )
    set_tests_properties(chipper_render_nes_noise_long_assert PROPERTIES DEPENDS chipper_render_nes_noise_long_smoke)

    add_test(NAME chipper_render_nes_noise_short_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/nes-noise-short-smoke.json --implemented-contains partial --macro Drum --register-writes 0 --note-events 0 --core-min noiseModeChoice=2 --core-max noiseModeChoice=2 --core-min dmcDirectControl=0.32 --core-max dmcDirectControl=0.32 --core-min dmcLevel=41 --core-max dmcLevel=41 --core-min noiseShortMode=1 --core-max noiseShortMode=1 --core-min noiseRegister=143 --core-max noiseRegister=143 --core-min noisePeriod=15 --core-max noisePeriod=15
    )
    set_tests_properties(chipper_render_nes_noise_short_assert PROPERTIES DEPENDS chipper_render_nes_noise_short_smoke)

    add_test(NAME chipper_render_ym2149_stereo_spread_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/ym-stereo-spread-smoke.json --implemented-contains partial --macro Manual --stereo-spread 1 --register-writes 0 --note-events 0 --min-stereo-rms-delta 0.001 --core-min stereoSpread=1 --core-max stereoSpread=1 --core-min stereoPanA=-1 --core-max stereoPanA=-1 --core-min stereoPanC=1 --core-max stereoPanC=1 --descriptor-param-kind stereoSpread=continuous --descriptor-param-surface stereoSpread=slider --descriptor-param-label "stereoSpread=Stereo Spread"
    )
    set_tests_properties(chipper_render_ym2149_stereo_spread_assert PROPERTIES DEPENDS chipper_render_ym2149_stereo_spread_smoke)

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

    add_test(NAME chipper_render_ym2149_source_level_mute_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/ym-source-level-mute-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --max-peak 0.001 --max-rms 0.001 --core-min sourceEnabledA=0 --core-max sourceEnabledA=0 --core-min sourceEnabledB=1 --core-max sourceEnabledB=1 --core-min sourceEnabledC=0 --core-max sourceEnabledC=0 --core-min sourceEnabledNoise=0 --core-max sourceEnabledNoise=0 --core-min sourceLevelB=0 --core-max sourceLevelB=0 --core-min volumeB=10 --core-max volumeB=10
    )
    set_tests_properties(chipper_render_ym2149_source_level_mute_assert PROPERTIES DEPENDS chipper_render_ym2149_source_level_mute_smoke)

    add_test(NAME chipper_render_ym2149_noise_source_off_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/ym-noise-source-off-smoke.json --implemented-contains partial --macro Drum --register-writes 0 --note-events 0 --max-peak 0.001 --max-rms 0.001 --core-min sourceEnabledNoise=0 --core-max sourceEnabledNoise=0 --core-min mixer=63 --core-max mixer=63 --core-min volumeA=0 --core-max volumeA=0 --core-min volumeB=0 --core-max volumeB=0 --core-min volumeC=0 --core-max volumeC=0
    )
    set_tests_properties(chipper_render_ym2149_noise_source_off_assert PROPERTIES DEPENDS chipper_render_ym2149_noise_source_off_smoke)

    add_test(NAME chipper_render_ym2149_noise_period_slow_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/ym-noise-period-slow-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --core-min noisePeriod=31 --core-max noisePeriod=31 --descriptor-param-kind macroControl3=chipRegister --descriptor-param-surface macroControl3=slider --descriptor-param-label "macroControl3=Noise Pitch"
    )
    set_tests_properties(chipper_render_ym2149_noise_period_slow_assert PROPERTIES DEPENDS chipper_render_ym2149_noise_period_slow_smoke)

    add_test(NAME chipper_render_ym2149_noise_period_fast_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/ym-noise-period-fast-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --core-min noisePeriod=1 --core-max noisePeriod=1
    )
    set_tests_properties(chipper_render_ym2149_noise_period_fast_assert PROPERTIES DEPENDS chipper_render_ym2149_noise_period_fast_smoke)

    add_test(NAME chipper_render_ym2149_mixer_noise_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/ym-mixer-noise-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --min-peak 0.02 --core-min mixer=7 --core-max mixer=7 --descriptor-param-kind macroControl4=chipRegister --descriptor-param-surface macroControl4=segmentedChoice --descriptor-param-label "macroControl4=Tone/Noise Mix" --descriptor-param-choices macroControl4=3
    )
    set_tests_properties(chipper_render_ym2149_mixer_noise_assert PROPERTIES DEPENDS chipper_render_ym2149_mixer_noise_smoke)

    add_test(NAME chipper_render_ym2149_mixer_tone_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/ym-mixer-tone-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --min-peak 0.02 --core-min mixer=56 --core-max mixer=56
    )
    set_tests_properties(chipper_render_ym2149_mixer_tone_assert PROPERTIES DEPENDS chipper_render_ym2149_mixer_tone_smoke)

    add_test(NAME chipper_render_ym2149_mixer_both_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/ym-mixer-both-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --min-peak 0.02 --core-min mixer=0 --core-max mixer=0
    )
    set_tests_properties(chipper_render_ym2149_mixer_both_assert PROPERTIES DEPENDS chipper_render_ym2149_mixer_both_smoke)

    add_test(NAME chipper_render_ym2149_channel_mix_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/ym-channel-mix-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --min-peak 0.02 --core-min mixer=46 --core-max mixer=46 --core-min channelMixChoiceA=1 --core-max channelMixChoiceA=1 --core-min channelMixChoiceB=2 --core-max channelMixChoiceB=2 --core-min channelMixChoiceC=4 --core-max channelMixChoiceC=4 --core-min toneEnabledA=1 --core-max toneEnabledA=1 --core-min noiseEnabledA=0 --core-max noiseEnabledA=0 --core-min toneEnabledB=0 --core-max toneEnabledB=0 --core-min noiseEnabledB=1 --core-max noiseEnabledB=1 --core-min toneEnabledC=0 --core-max toneEnabledC=0 --core-min noiseEnabledC=0 --core-max noiseEnabledC=0 --descriptor-param-kind ymChannelAMix=chipRegister --descriptor-param-surface ymChannelAMix=menu --descriptor-param-choices ymChannelAMix=5
    )
    set_tests_properties(chipper_render_ym2149_channel_mix_assert PROPERTIES DEPENDS chipper_render_ym2149_channel_mix_smoke)

    add_test(NAME chipper_render_ym2149_channel_mix_follow_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/ym-channel-mix-follow-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --min-peak 0.02 --core-min mixer=56 --core-max mixer=56 --core-min channelMixChoiceA=0 --core-max channelMixChoiceA=0 --core-min channelMixChoiceB=0 --core-max channelMixChoiceB=0 --core-min channelMixChoiceC=0 --core-max channelMixChoiceC=0 --core-min toneEnabledA=1 --core-max toneEnabledA=1 --core-min noiseEnabledA=0 --core-max noiseEnabledA=0 --core-min toneEnabledB=1 --core-max toneEnabledB=1 --core-min noiseEnabledB=0 --core-max noiseEnabledB=0 --core-min toneEnabledC=1 --core-max toneEnabledC=1 --core-min noiseEnabledC=0 --core-max noiseEnabledC=0
    )
    set_tests_properties(chipper_render_ym2149_channel_mix_follow_assert PROPERTIES DEPENDS chipper_render_ym2149_channel_mix_follow_smoke)

    add_test(NAME chipper_render_ym2149_envelope_fall_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/ym-envelope-fall-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --min-peak 0.02 --core-min envelopeShapeChoice=1 --core-max envelopeShapeChoice=1 --core-min envelopeShape=9 --core-max envelopeShape=9 --core-min envelopeEnabledA=1 --core-max envelopeEnabledA=1 --core-min envelopeEnabledB=1 --core-max envelopeEnabledB=1 --core-min envelopeEnabledC=1 --core-max envelopeEnabledC=1 --core-min volumeA=16 --core-max volumeA=16 --descriptor-param-kind ymEnvelopeShape=chipRegister --descriptor-param-surface ymEnvelopeShape=menu --descriptor-param-choices ymEnvelopeShape=21
    )
    set_tests_properties(chipper_render_ym2149_envelope_fall_assert PROPERTIES DEPENDS chipper_render_ym2149_envelope_fall_smoke)

    add_test(NAME chipper_render_ym2149_envelope_triangle_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/ym-envelope-triangle-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --min-peak 0.02 --core-min envelopeShapeChoice=4 --core-max envelopeShapeChoice=4 --core-min envelopeShape=14 --core-max envelopeShape=14 --core-min envelopeEnabledA=1 --core-max envelopeEnabledA=1 --core-min envelopeEnabledB=1 --core-max envelopeEnabledB=1 --core-min envelopeEnabledC=1 --core-max envelopeEnabledC=1 --core-min envelopePeriod=640 --core-max envelopePeriod=640
    )
    set_tests_properties(chipper_render_ym2149_envelope_triangle_assert PROPERTIES DEPENDS chipper_render_ym2149_envelope_triangle_smoke)

    add_test(NAME chipper_render_ym2149_envelope_code0f_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/ym-envelope-code0f-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --min-peak 0.02 --core-min envelopeShapeChoice=20 --core-max envelopeShapeChoice=20 --core-min envelopeShape=15 --core-max envelopeShape=15 --core-min envelopeEnabledA=1 --core-max envelopeEnabledA=1 --core-min envelopeEnabledB=1 --core-max envelopeEnabledB=1 --core-min envelopeEnabledC=1 --core-max envelopeEnabledC=1
    )
    set_tests_properties(chipper_render_ym2149_envelope_code0f_assert PROPERTIES DEPENDS chipper_render_ym2149_envelope_code0f_smoke)

    add_test(NAME chipper_render_ym2149_envelope_code10_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/ym-envelope-code10-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --min-peak 0.02 --core-min envelopeShapeChoice=15 --core-max envelopeShapeChoice=15 --core-min envelopeShape=10 --core-max envelopeShape=10 --core-min envelopeEnabledA=1 --core-max envelopeEnabledA=1 --core-min envelopeEnabledB=1 --core-max envelopeEnabledB=1 --core-min envelopeEnabledC=1 --core-max envelopeEnabledC=1
    )
    set_tests_properties(chipper_render_ym2149_envelope_code10_assert PROPERTIES DEPENDS chipper_render_ym2149_envelope_code10_smoke)

    add_test(NAME chipper_render_ym2149_envelope_fast_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/ym-envelope-fast-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --min-peak 0.02 --core-min envelopeDecayControl=1 --core-max envelopeDecayControl=1 --core-min envelopeShapeChoice=4 --core-max envelopeShapeChoice=4 --core-min envelopeShape=14 --core-max envelopeShape=14 --core-min envelopePeriod=16 --core-max envelopePeriod=16 --descriptor-param-kind envelopeDecay=chipRegister --descriptor-param-surface envelopeDecay=slider --descriptor-param-label "envelopeDecay=Envelope Speed"
    )
    set_tests_properties(chipper_render_ym2149_envelope_fast_assert PROPERTIES DEPENDS chipper_render_ym2149_envelope_fast_smoke)

    add_test(NAME chipper_render_ym2149_envelope_reset_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/ym-envelope-reset-smoke.json --implemented-contains partial --macro Manual --register-writes 1 --note-events 1 --min-peak 0.02 --core-min envelopeDecayControl=1 --core-max envelopeDecayControl=1 --core-min envelopePeriod=16 --core-max envelopePeriod=16 --core-min envelopeShape=13 --core-max envelopeShape=13 --core-min envelopeStepResolution=32 --core-max envelopeStepResolution=32 --core-min envelopeStepRateHz=15625 --core-max envelopeStepRateHz=15625 --core-min envelopeResetCount=2 --core-max envelopeResetCount=2 --core-min envelopeCounter=7 --core-max envelopeCounter=7 --core-min envelopeDirection=1 --core-max envelopeDirection=1 --core-min envelopeHolding=0 --core-max envelopeHolding=0
    )
    set_tests_properties(chipper_render_ym2149_envelope_reset_assert PROPERTIES DEPENDS chipper_render_ym2149_envelope_reset_smoke)

    add_test(NAME chipper_render_ym2149_volume_curve_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/ym-volume-curve-smoke.json --implemented-contains partial --macro Manual --register-writes 4 --note-events 1 --min-peak 0.1 --core-min volumeA=15 --core-max volumeA=15 --core-min volumeB=10 --core-max volumeB=10 --core-min volumeC=0 --core-max volumeC=0 --core-min linearVolumeA=1 --core-max linearVolumeA=1 --core-min linearVolumeB=0.421 --core-max linearVolumeB=0.423 --core-min linearVolumeC=0 --core-max linearVolumeC=0
    )
    set_tests_properties(chipper_render_ym2149_volume_curve_assert PROPERTIES DEPENDS chipper_render_ym2149_volume_curve_smoke)

    add_test(NAME chipper_render_sn76489_stereo_spread_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sn-stereo-spread-smoke.json --implemented-contains partial --macro Manual --stereo-spread 1 --register-writes 0 --note-events 0 --min-stereo-rms-delta 0.001 --core-min stereoSpread=1 --core-max stereoSpread=1 --core-min stereoPan0=-1 --core-max stereoPan0=-1 --core-min stereoPan2=1 --core-max stereoPan2=1 --descriptor-param-kind stereoSpread=continuous --descriptor-param-surface stereoSpread=slider --descriptor-param-label "stereoSpread=Stereo Spread"
    )
    set_tests_properties(chipper_render_sn76489_stereo_spread_assert PROPERTIES DEPENDS chipper_render_sn76489_stereo_spread_smoke)

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
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sn-noise-macro-smoke.json --implemented-contains partial --macro Drum --register-writes 0 --note-events 0 --min-peak 0.05 --core-min noiseModeChoice=0 --core-max noiseModeChoice=0 --core-min noiseControl=4 --core-max noiseControl=4 --core-min noiseWhite=1 --core-max noiseWhite=1 --core-min noiseRate=0 --core-max noiseRate=0 --core-min attenuationNoise=2 --core-max attenuationNoise=2 --descriptor-param-kind macroControl3=chipRegister --descriptor-param-surface macroControl3=slider --descriptor-param-label "macroControl3=Noise Bias"
    )
    set_tests_properties(chipper_render_sn76489_noise_macro_assert PROPERTIES DEPENDS chipper_render_sn76489_noise_macro_smoke)

    add_test(NAME chipper_render_sn76489_noise_bias_low_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sn-noise-bias-low-smoke.json --implemented-contains partial --macro Drum --register-writes 0 --note-events 0 --min-peak 0.05 --core-min noiseModeChoice=0 --core-max noiseModeChoice=0 --core-min noiseControl=0 --core-max noiseControl=0 --core-min noiseWhite=0 --core-max noiseWhite=0 --core-min noiseRate=0 --core-max noiseRate=0
    )
    set_tests_properties(chipper_render_sn76489_noise_bias_low_assert PROPERTIES DEPENDS chipper_render_sn76489_noise_bias_low_smoke)

    add_test(NAME chipper_render_sn76489_noise_bias_high_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sn-noise-bias-high-smoke.json --implemented-contains partial --macro Drum --register-writes 0 --note-events 0 --min-peak 0.05 --core-min noiseModeChoice=0 --core-max noiseModeChoice=0 --core-min noiseControl=4 --core-max noiseControl=4 --core-min noiseWhite=1 --core-max noiseWhite=1 --core-min noiseRate=0 --core-max noiseRate=0
    )
    set_tests_properties(chipper_render_sn76489_noise_bias_high_assert PROPERTIES DEPENDS chipper_render_sn76489_noise_bias_high_smoke)

    add_test(NAME chipper_render_sn76489_noise_periodic_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sn-noise-periodic-smoke.json --implemented-contains partial --macro Drum --register-writes 0 --note-events 0 --min-peak 0.05 --core-min noiseModeChoice=2 --core-max noiseModeChoice=2 --core-min noiseControl=2 --core-max noiseControl=2 --core-min noiseWhite=0 --core-max noiseWhite=0 --core-min noiseRate=2 --core-max noiseRate=2 --core-min attenuationNoise=2 --core-max attenuationNoise=2 --descriptor-param-kind snNoiseMode=chipRegister --descriptor-param-surface snNoiseMode=segmentedChoice --descriptor-param-choices snNoiseMode=5
    )
    set_tests_properties(chipper_render_sn76489_noise_periodic_assert PROPERTIES DEPENDS chipper_render_sn76489_noise_periodic_smoke)

    add_test(NAME chipper_render_sn76489_noise_white_t3_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sn-noise-white-t3-smoke.json --implemented-contains partial --macro Drum --register-writes 0 --note-events 0 --min-peak 0.05 --core-min noiseModeChoice=4 --core-max noiseModeChoice=4 --core-min noiseControl=7 --core-max noiseControl=7 --core-min noiseWhite=1 --core-max noiseWhite=1 --core-min noiseRate=3 --core-max noiseRate=3 --core-min noiseTone3Clocked=1 --core-max noiseTone3Clocked=1 --core-min attenuationNoise=2 --core-max attenuationNoise=2
    )
    set_tests_properties(chipper_render_sn76489_noise_white_t3_assert PROPERTIES DEPENDS chipper_render_sn76489_noise_white_t3_smoke)

    add_test(NAME chipper_render_sn76489_register_trace_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sn-register-trace-smoke.json --implemented-contains partial --macro Manual --register-writes 8 --note-events 0 --min-peak 0.15 --core-min period2=32 --core-max period2=32 --core-min noiseControl=7 --core-max noiseControl=7 --core-min noiseWhite=1 --core-max noiseWhite=1 --core-min noiseRate=3 --core-max noiseRate=3 --core-min noiseTone3Clocked=1 --core-max noiseTone3Clocked=1 --core-min noiseDivider=1024 --core-max noiseDivider=1024 --core-min noiseRegisterResets=2 --core-max noiseRegisterResets=2 --core-min attenuation0=15 --core-max attenuation0=15 --core-min attenuation1=15 --core-max attenuation1=15 --core-min attenuation2=15 --core-max attenuation2=15 --core-min attenuationNoise=0 --core-max attenuationNoise=0
    )
    set_tests_properties(chipper_render_sn76489_register_trace_assert PROPERTIES DEPENDS chipper_render_sn76489_register_trace_smoke)

    add_test(NAME chipper_render_sn76489_tone_zero_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sn-tone-zero-smoke.json --implemented-contains partial --macro Manual --register-writes 3 --note-events 0 --min-peak 0.19 --max-peak 0.21 --min-rms 0.19 --max-rms 0.21 --max-zero-crossings 0 --core-min period0=0 --core-max period0=0 --core-min toneConstant0=1 --core-max toneConstant0=1 --core-min toneConstant1=0 --core-max toneConstant1=0 --core-min toneConstant2=0 --core-max toneConstant2=0 --core-min sourceEnabled0=1 --core-max sourceEnabled0=1 --core-min sourceEnabled1=0 --core-max sourceEnabled1=0 --core-min sourceEnabled2=0 --core-max sourceEnabled2=0 --core-min sourceEnabledNoise=0 --core-max sourceEnabledNoise=0 --core-min attenuation0=0 --core-max attenuation0=0 --core-min attenuation1=15 --core-max attenuation1=15 --core-min attenuation2=15 --core-max attenuation2=15 --core-min attenuationNoise=15 --core-max attenuationNoise=15
    )
    set_tests_properties(chipper_render_sn76489_tone_zero_assert PROPERTIES DEPENDS chipper_render_sn76489_tone_zero_smoke)

    add_test(NAME chipper_render_sn76489_tone_one_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sn-tone-one-smoke.json --implemented-contains partial --macro Manual --register-writes 3 --note-events 0 --min-peak 0.19 --max-peak 0.21 --min-rms 0.19 --max-rms 0.21 --max-zero-crossings 1 --core-min period0=1 --core-max period0=1 --core-min toneConstant0=1 --core-max toneConstant0=1 --core-min toneConstant1=0 --core-max toneConstant1=0 --core-min toneConstant2=0 --core-max toneConstant2=0 --core-min sourceEnabled0=1 --core-max sourceEnabled0=1 --core-min sourceEnabled1=0 --core-max sourceEnabled1=0 --core-min sourceEnabled2=0 --core-max sourceEnabled2=0 --core-min sourceEnabledNoise=0 --core-max sourceEnabledNoise=0 --core-min attenuation0=0 --core-max attenuation0=0 --core-min attenuation1=15 --core-max attenuation1=15 --core-min attenuation2=15 --core-max attenuation2=15 --core-min attenuationNoise=15 --core-max attenuationNoise=15
    )
    set_tests_properties(chipper_render_sn76489_tone_one_assert PROPERTIES DEPENDS chipper_render_sn76489_tone_one_smoke)

    add_test(NAME chipper_render_sn76489_volume_data_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sn-volume-data-smoke.json --implemented-contains partial --macro Manual --register-writes 4 --note-events 0 --min-peak 0.19 --max-peak 0.21 --min-rms 0.19 --max-rms 0.21 --max-zero-crossings 1 --core-min period0=0 --core-max period0=0 --core-min toneConstant0=1 --core-max toneConstant0=1 --core-min volumeDataByteWrites=1 --core-max volumeDataByteWrites=1 --core-min attenuation0=0 --core-max attenuation0=0 --core-min attenuation1=15 --core-max attenuation1=15 --core-min attenuation2=15 --core-max attenuation2=15 --core-min attenuationNoise=15 --core-max attenuationNoise=15
    )
    set_tests_properties(chipper_render_sn76489_volume_data_assert PROPERTIES DEPENDS chipper_render_sn76489_volume_data_smoke)

    add_test(NAME chipper_render_sn76489_noise_level_min_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sn-noise-level-min-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --core-min attenuationNoise=15 --core-max attenuationNoise=15
    )
    set_tests_properties(chipper_render_sn76489_noise_level_min_assert PROPERTIES DEPENDS chipper_render_sn76489_noise_level_min_smoke)

    add_test(NAME chipper_render_sn76489_noise_level_max_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/sn-noise-level-max-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --core-min attenuationNoise=0 --core-max attenuationNoise=0 --descriptor-param-kind macroControl4=chipRegister --descriptor-param-surface macroControl4=slider --descriptor-param-label "macroControl4=Noise Level"
    )
    set_tests_properties(chipper_render_sn76489_noise_level_max_assert PROPERTIES DEPENDS chipper_render_sn76489_noise_level_max_smoke)

    add_test(NAME chipper_render_preset_nes_hero_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-nes-hero-smoke.json --implemented-contains partial --preset nes-hero-pulse --macro Lead --play-mode "Big Mono" --output-db -9 --register-writes 0 --note-events 0 --min-peak 0.1 --core-min pulseDuty1=2 --core-max pulseDuty1=2 --core-min sourceEnabled1=1 --core-max sourceEnabled1=1 --core-min sourceEnabled2=1 --core-max sourceEnabled2=1 --core-min sourceEnabled3=1 --core-max sourceEnabled3=1 --core-min sourceEnabled4=0 --core-max sourceEnabled4=0
    )
    set_tests_properties(chipper_render_preset_nes_hero_assert PROPERTIES DEPENDS chipper_render_preset_nes_hero_smoke)

    add_test(NAME chipper_render_preset_nes_triangle_bass_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-nes-triangle-bass-smoke.json --implemented-contains partial --preset nes-triangle-bass --macro Bass --play-mode "Big Mono" --output-db -8 --register-writes 0 --note-events 0 --min-peak 0.04 --core-min sourceEnabled1=1 --core-max sourceEnabled1=1 --core-min sourceEnabled2=0 --core-max sourceEnabled2=0 --core-min sourceEnabled3=1 --core-max sourceEnabled3=1 --core-min sourceEnabled4=0 --core-max sourceEnabled4=0 --core-min pulseDuty1=2 --core-max pulseDuty1=2
    )
    set_tests_properties(chipper_render_preset_nes_triangle_bass_assert PROPERTIES DEPENDS chipper_render_preset_nes_triangle_bass_smoke)

    add_test(NAME chipper_render_preset_nes_noise_snare_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-nes-noise-snare-smoke.json --implemented-contains partial --preset nes-noise-snare --macro Drum --play-mode "Big Mono" --output-db -10 --register-writes 0 --note-events 0 --min-peak 0.04 --min-zero-crossings 100 --core-min sourceEnabled1=0 --core-max sourceEnabled1=0 --core-min sourceEnabled2=0 --core-max sourceEnabled2=0 --core-min sourceEnabled3=1 --core-max sourceEnabled3=1 --core-min sourceEnabled4=1 --core-max sourceEnabled4=1 --core-min dmcDirectControl=0.34 --core-max dmcDirectControl=0.34 --core-min dmcLevel=43 --core-max dmcLevel=43
    )
    set_tests_properties(chipper_render_preset_nes_noise_snare_assert PROPERTIES DEPENDS chipper_render_preset_nes_noise_snare_smoke)

    add_test(NAME chipper_render_preset_nes_coin_blip_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-nes-coin-blip-smoke.json --implemented-contains partial --preset nes-coin-blip --macro Coin --play-mode "Big Mono" --output-db -11 --register-writes 0 --note-events 0 --min-peak 0.03 --min-zero-crossings 40 --core-min sourceEnabled1=1 --core-max sourceEnabled1=1 --core-min sourceEnabled2=1 --core-max sourceEnabled2=1 --core-min sourceEnabled3=0 --core-max sourceEnabled3=0 --core-min sourceEnabled4=0 --core-max sourceEnabled4=0 --core-min pulseDuty1=0 --core-max pulseDuty1=0
    )
    set_tests_properties(chipper_render_preset_nes_coin_blip_assert PROPERTIES DEPENDS chipper_render_preset_nes_coin_blip_smoke)

    add_test(NAME chipper_render_preset_nes_boss_damage_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-nes-boss-damage-smoke.json --implemented-contains partial --preset nes-boss-damage --macro Hit --play-mode "Big Mono" --output-db -12 --register-writes 0 --note-events 0 --min-peak 0.08 --min-zero-crossings 100 --core-min sourceEnabled1=1 --core-max sourceEnabled1=1 --core-min sourceEnabled2=0 --core-max sourceEnabled2=0 --core-min sourceEnabled3=1 --core-max sourceEnabled3=1 --core-min sourceEnabled4=1 --core-max sourceEnabled4=1 --core-min noiseModeChoice=2 --core-max noiseModeChoice=2 --core-min dmcDirectControl=0.66 --core-max dmcDirectControl=0.66 --core-min dmcLevel=84 --core-max dmcLevel=84
    )
    set_tests_properties(chipper_render_preset_nes_boss_damage_assert PROPERTIES DEPENDS chipper_render_preset_nes_boss_damage_smoke)

    add_test(NAME chipper_render_preset_nes_power_up_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-nes-power-up-smoke.json --implemented-contains partial --preset nes-power-up-rise --macro Power-Up --play-mode "Big Mono" --output-db -10 --register-writes 0 --note-events 0 --min-peak 0.1 --core-min sourceEnabled1=1 --core-max sourceEnabled1=1 --core-min sourceEnabled2=1 --core-max sourceEnabled2=1 --core-min sourceEnabled3=1 --core-max sourceEnabled3=1 --core-min sourceEnabled4=0 --core-max sourceEnabled4=0
    )
    set_tests_properties(chipper_render_preset_nes_power_up_assert PROPERTIES DEPENDS chipper_render_preset_nes_power_up_smoke)

    add_test(NAME chipper_render_preset_dmg_wave_bass_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-dmg-wave-bass-smoke.json --implemented-contains partial --preset dmg-wave-bass --macro Bass --play-mode "Big Mono" --output-db -8 --register-writes 0 --note-events 0 --min-peak 0.08 --core-min sourceEnabled1=0 --core-max sourceEnabled1=0 --core-min sourceEnabled2=0 --core-max sourceEnabled2=0 --core-min sourceEnabled3=1 --core-max sourceEnabled3=1 --core-min sourceEnabled4=0 --core-max sourceEnabled4=0 --core-min waveShape=1 --core-max waveShape=1 --core-min enabledWave=1 --core-max enabledWave=1
    )
    set_tests_properties(chipper_render_preset_dmg_wave_bass_assert PROPERTIES DEPENDS chipper_render_preset_dmg_wave_bass_smoke)

    add_test(NAME chipper_render_preset_dmg_pocket_arp_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-dmg-pocket-arp-smoke.json --implemented-contains partial --preset dmg-pocket-arp --macro Arp --play-mode "Chip Poly" --output-db -10 --register-writes 0 --note-events 0 --min-peak 0.01 --core-min sourceEnabled1=1 --core-max sourceEnabled1=1 --core-min sourceEnabled2=1 --core-max sourceEnabled2=1 --core-min sourceEnabled3=1 --core-max sourceEnabled3=1 --core-min sourceEnabled4=0 --core-max sourceEnabled4=0 --core-min waveShape=4 --core-max waveShape=4 --core-min nr51=90 --core-max nr51=90
    )
    set_tests_properties(chipper_render_preset_dmg_pocket_arp_assert PROPERTIES DEPENDS chipper_render_preset_dmg_pocket_arp_smoke)

    add_test(NAME chipper_render_preset_dmg_noise_hat_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-dmg-noise-hat-smoke.json --implemented-contains partial --preset dmg-noise-hat --macro Drum --play-mode "Big Mono" --output-db -12 --register-writes 0 --note-events 0 --min-peak 0.02 --min-zero-crossings 100 --core-min sourceEnabled1=0 --core-max sourceEnabled1=0 --core-min sourceEnabled2=0 --core-max sourceEnabled2=0 --core-min sourceEnabled3=0 --core-max sourceEnabled3=0 --core-min sourceEnabled4=1 --core-max sourceEnabled4=1 --core-min enabledNoise=1 --core-max enabledNoise=1
    )
    set_tests_properties(chipper_render_preset_dmg_noise_hat_assert PROPERTIES DEPENDS chipper_render_preset_dmg_noise_hat_smoke)

    add_test(NAME chipper_render_preset_dmg_power_up_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-dmg-power-up-smoke.json --implemented-contains partial --preset dmg-power-up-rise --macro Power-Up --play-mode "Big Mono" --output-db -10 --register-writes 0 --note-events 0 --min-peak 0.06 --min-left-peak 0.08 --min-right-peak 0.03 --core-min sourceEnabled1=1 --core-max sourceEnabled1=1 --core-min sourceEnabled2=1 --core-max sourceEnabled2=1 --core-min sourceEnabled3=1 --core-max sourceEnabled3=1 --core-min sourceEnabled4=0 --core-max sourceEnabled4=0 --core-min waveShape=2 --core-max waveShape=2 --core-min enabledWave=1 --core-max enabledWave=1 --core-min dmgStereoRoute=4 --core-max dmgStereoRoute=4 --core-min dmgStereoRouteRegister=90 --core-max dmgStereoRouteRegister=90 --core-min nr51=90 --core-max nr51=90
    )
    set_tests_properties(chipper_render_preset_dmg_power_up_assert PROPERTIES DEPENDS chipper_render_preset_dmg_power_up_smoke)

    add_test(NAME chipper_render_preset_sid_dirty_bass_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-sid-dirty-bass-smoke.json --implemented-contains partial --preset sid-dirty-bass --macro Bass --play-mode "Big Mono" --output-db -9 --register-writes 0 --note-events 0 --min-peak 0.03 --core-min sourceEnabled1=1 --core-max sourceEnabled1=1 --core-min sourceEnabled2=1 --core-max sourceEnabled2=1 --core-min sourceEnabled3=0 --core-max sourceEnabled3=0 --core-min waveformChoice=3 --core-max waveformChoice=3 --core-min waveformChoice1=3 --core-max waveformChoice1=3 --core-min waveformBits=64 --core-max waveformBits=64 --core-min waveformBits1=64 --core-max waveformBits1=64 --core-min pulseWidthRegister=1966 --core-max pulseWidthRegister=1966 --core-min sustain0=12 --core-max sustain0=12
    )
    set_tests_properties(chipper_render_preset_sid_dirty_bass_assert PROPERTIES DEPENDS chipper_render_preset_sid_dirty_bass_smoke)

    add_test(NAME chipper_render_preset_sid_pwm_lead_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-sid-pwm-lead-smoke.json --implemented-contains partial --preset sid-pwm-lead --macro Lead --play-mode "Big Mono" --output-db -10 --register-writes 0 --note-events 0 --min-peak 0.03 --core-min sourceEnabled1=1 --core-max sourceEnabled1=1 --core-min sourceEnabled2=1 --core-max sourceEnabled2=1 --core-min sourceEnabled3=1 --core-max sourceEnabled3=1 --core-min waveformChoice=3 --core-max waveformChoice=3 --core-min waveformChoice1=3 --core-max waveformChoice1=3 --core-min waveformChoice2=3 --core-max waveformChoice2=3 --core-min waveformBits=64 --core-max waveformBits=64 --core-min waveformBits1=64 --core-max waveformBits1=64 --core-min waveformBits2=64 --core-max waveformBits2=64 --core-min pulseWidthRegister=2375 --core-max pulseWidthRegister=2375 --core-min sustain0=11 --core-max sustain0=11
    )
    set_tests_properties(chipper_render_preset_sid_pwm_lead_assert PROPERTIES DEPENDS chipper_render_preset_sid_pwm_lead_smoke)

    add_test(NAME chipper_render_preset_sid_robot_arp_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-sid-robot-arp-smoke.json --implemented-contains partial --preset sid-robot-arp --macro Arp --play-mode "Chip Poly" --output-db -10 --register-writes 0 --note-events 0 --min-peak 0.03 --core-min sourceEnabled1=1 --core-max sourceEnabled1=1 --core-min sourceEnabled2=1 --core-max sourceEnabled2=1 --core-min sourceEnabled3=1 --core-max sourceEnabled3=1 --core-min waveformChoice0=2 --core-max waveformChoice0=2 --core-min waveformChoice1=3 --core-max waveformChoice1=3 --core-min waveformChoice2=1 --core-max waveformChoice2=1 --core-min waveformBits0=32 --core-max waveformBits0=32 --core-min waveformBits1=64 --core-max waveformBits1=64 --core-min waveformBits2=16 --core-max waveformBits2=16
    )
    set_tests_properties(chipper_render_preset_sid_robot_arp_assert PROPERTIES DEPENDS chipper_render_preset_sid_robot_arp_smoke)

    add_test(NAME chipper_render_preset_sid_filter_pluck_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-sid-filter-pluck-smoke.json --implemented-contains partial --preset sid-filter-pluck --macro Lead --play-mode "Big Mono" --output-db -10 --register-writes 0 --note-events 0 --min-peak 0.02 --core-min sourceEnabled1=1 --core-max sourceEnabled1=1 --core-min sourceEnabled2=1 --core-max sourceEnabled2=1 --core-min sourceEnabled3=1 --core-max sourceEnabled3=1 --core-min filterModeChoice=1 --core-max filterModeChoice=1 --core-min filterRoutingChoice=1 --core-max filterRoutingChoice=1 --core-min filterRoutingBits=7 --core-max filterRoutingBits=7
    )
    set_tests_properties(chipper_render_preset_sid_filter_pluck_assert PROPERTIES DEPENDS chipper_render_preset_sid_filter_pluck_smoke)

    add_test(NAME chipper_render_preset_sid_notch_pwm_keys_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-sid-notch-pwm-keys-smoke.json --implemented-contains partial --preset sid-notch-pwm-keys --macro Lead --play-mode "Big Mono" --output-db -10 --register-writes 0 --note-events 0 --min-peak 0.02 --core-min filterModeChoice=5 --core-max filterModeChoice=5 --core-min filterModeBits=80 --core-max filterModeBits=80 --core-min filterModeVolume=95 --core-max filterModeVolume=95 --core-min waveformChoice0=3 --core-max waveformChoice0=3 --core-min waveformChoice1=3 --core-max waveformChoice1=3 --core-min waveformChoice2=2 --core-max waveformChoice2=2 --core-min sourceLevel1=0.92 --core-max sourceLevel1=0.92 --core-min sourceLevel2=0.78 --core-max sourceLevel2=0.78 --core-min sourceLevel3=0.55 --core-max sourceLevel3=0.55 --core-min attackChoice0=1 --core-max attackChoice0=1 --core-min decayChoice0=8 --core-max decayChoice0=8 --core-min sustainChoice0=7 --core-max sustainChoice0=7 --core-min releaseChoice0=4 --core-max releaseChoice0=4 --core-min attackDecayRegister0=7 --core-max attackDecayRegister0=7 --core-min sustainReleaseRegister0=99 --core-max sustainReleaseRegister0=99
    )
    set_tests_properties(chipper_render_preset_sid_notch_pwm_keys_assert PROPERTIES DEPENDS chipper_render_preset_sid_notch_pwm_keys_smoke)

    add_test(NAME chipper_render_preset_sid_lpbp_sweep_stack_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-sid-lpbp-sweep-stack-smoke.json --implemented-contains partial --preset sid-lpbp-sweep-stack --macro Power-Up --play-mode "Big Mono" --output-db -11 --register-writes 0 --note-events 0 --min-peak 0.02 --core-min filterModeChoice=6 --core-max filterModeChoice=6 --core-min filterModeBits=48 --core-max filterModeBits=48 --core-min filterModeVolume=63 --core-max filterModeVolume=63 --core-min sidModelChoice=2 --core-max sidModelChoice=2 --core-min waveformChoice0=2 --core-max waveformChoice0=2 --core-min waveformChoice1=3 --core-max waveformChoice1=3 --core-min waveformChoice2=1 --core-max waveformChoice2=1 --core-min sourceLevel1=0.88 --core-max sourceLevel1=0.88 --core-min sourceLevel2=0.62 --core-max sourceLevel2=0.62 --core-min sourceLevel3=0.45 --core-max sourceLevel3=0.45 --core-min attackChoice1=2 --core-max attackChoice1=2 --core-min decayChoice1=8 --core-max decayChoice1=8 --core-min sustainChoice1=9 --core-max sustainChoice1=9 --core-min releaseChoice1=5 --core-max releaseChoice1=5
    )
    set_tests_properties(chipper_render_preset_sid_lpbp_sweep_stack_assert PROPERTIES DEPENDS chipper_render_preset_sid_lpbp_sweep_stack_smoke)

    add_test(NAME chipper_render_preset_sid_bphp_metal_bell_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-sid-bphp-metal-bell-smoke.json --implemented-contains partial --preset sid-bphp-metal-bell --macro Lead --play-mode "Big Mono" --output-db -12 --register-writes 0 --note-events 0 --min-peak 0.01 --core-min filterModeChoice=7 --core-max filterModeChoice=7 --core-min filterModeBits=96 --core-max filterModeBits=96 --core-min filterModeVolume=111 --core-max filterModeVolume=111 --core-min modModeChoice=3 --core-max modModeChoice=3 --core-min waveformChoice0=1 --core-max waveformChoice0=1 --core-min waveformChoice1=3 --core-max waveformChoice1=3 --core-min waveformChoice2=1 --core-max waveformChoice2=1 --core-min sourceLevel1=0.65 --core-max sourceLevel1=0.65 --core-min sourceLevel2=0.9 --core-max sourceLevel2=0.9 --core-min sourceLevel3=0.52 --core-max sourceLevel3=0.52 --core-min attackChoice2=1 --core-max attackChoice2=1 --core-min decayChoice2=8 --core-max decayChoice2=8 --core-min sustainChoice2=5 --core-max sustainChoice2=5 --core-min releaseChoice2=10 --core-max releaseChoice2=10
    )
    set_tests_properties(chipper_render_preset_sid_bphp_metal_bell_assert PROPERTIES DEPENDS chipper_render_preset_sid_bphp_metal_bell_smoke)

    add_test(NAME chipper_render_preset_sid_allmode_growl_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-sid-allmode-growl-smoke.json --implemented-contains partial --preset sid-allmode-growl --macro Bass --play-mode "Big Mono" --output-db -12 --register-writes 0 --note-events 0 --min-peak 0.01 --core-min filterModeChoice=8 --core-max filterModeChoice=8 --core-min filterModeBits=112 --core-max filterModeBits=112 --core-min filterModeVolume=127 --core-max filterModeVolume=127 --core-min modModeChoice=4 --core-max modModeChoice=4 --core-min waveformChoice0=3 --core-max waveformChoice0=3 --core-min waveformChoice1=2 --core-max waveformChoice1=2 --core-min waveformChoice2=4 --core-max waveformChoice2=4 --core-min sourceLevel1=0.82 --core-max sourceLevel1=0.82 --core-min sourceLevel2=0.58 --core-max sourceLevel2=0.58 --core-min sourceLevel3=0.36 --core-max sourceLevel3=0.36 --core-min attackChoice0=1 --core-max attackChoice0=1 --core-min decayChoice0=5 --core-max decayChoice0=5 --core-min sustainChoice0=13 --core-max sustainChoice0=13 --core-min releaseChoice0=4 --core-max releaseChoice0=4
    )
    set_tests_properties(chipper_render_preset_sid_allmode_growl_assert PROPERTIES DEPENDS chipper_render_preset_sid_allmode_growl_smoke)

    add_test(NAME chipper_render_preset_sid_pwm_bass_split_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-sid-pwm-bass-split-smoke.json --implemented-contains partial --preset sid-pwm-bass-split --macro Bass --play-mode "Big Mono" --output-db -9 --register-writes 0 --note-events 0 --min-peak 0.02 --core-min filterModeChoice=1 --core-max filterModeChoice=1 --core-min filterModeBits=16 --core-max filterModeBits=16 --core-min filterRoutingChoice=5 --core-max filterRoutingChoice=5 --core-min filterRoutingBits=3 --core-max filterRoutingBits=3 --core-min sidModelChoice=2 --core-max sidModelChoice=2 --core-min waveformChoice0=3 --core-max waveformChoice0=3 --core-min waveformChoice1=3 --core-max waveformChoice1=3 --core-min waveformChoice2=2 --core-max waveformChoice2=2 --core-min sourceLevel1=0.9 --core-max sourceLevel1=0.9 --core-min sourceLevel2=0.72 --core-max sourceLevel2=0.72 --core-min sourceLevel3=0.28 --core-max sourceLevel3=0.28 --core-min attackChoice0=1 --core-max attackChoice0=1 --core-min decayChoice0=5 --core-max decayChoice0=5 --core-min sustainChoice0=14 --core-max sustainChoice0=14 --core-min releaseChoice0=4 --core-max releaseChoice0=4 --core-min attackChoice1=1 --core-max attackChoice1=1 --core-min decayChoice1=6 --core-max decayChoice1=6 --core-min sustainChoice1=12 --core-max sustainChoice1=12 --core-min releaseChoice1=4 --core-max releaseChoice1=4
    )
    set_tests_properties(chipper_render_preset_sid_pwm_bass_split_assert PROPERTIES DEPENDS chipper_render_preset_sid_pwm_bass_split_smoke)

    add_test(NAME chipper_render_preset_sid_wide_pwm_pad_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-sid-wide-pwm-pad-smoke.json --implemented-contains partial --preset sid-wide-pwm-pad --macro Lead --play-mode "Big Mono" --output-db -12 --register-writes 0 --note-events 0 --min-peak 0.005 --core-min filterModeChoice=1 --core-max filterModeChoice=1 --core-min filterModeBits=16 --core-max filterModeBits=16 --core-min filterRoutingChoice=1 --core-max filterRoutingChoice=1 --core-min filterRoutingBits=7 --core-max filterRoutingBits=7 --core-min sidModelChoice=1 --core-max sidModelChoice=1 --core-min waveformChoice0=3 --core-max waveformChoice0=3 --core-min waveformChoice1=3 --core-max waveformChoice1=3 --core-min waveformChoice2=3 --core-max waveformChoice2=3 --core-min sourceLevel1=0.72 --core-max sourceLevel1=0.72 --core-min sourceLevel2=0.68 --core-max sourceLevel2=0.68 --core-min sourceLevel3=0.6 --core-max sourceLevel3=0.6 --core-min attackChoice0=3 --core-max attackChoice0=3 --core-min decayChoice0=9 --core-max decayChoice0=9 --core-min sustainChoice0=12 --core-max sustainChoice0=12 --core-min releaseChoice0=8 --core-max releaseChoice0=8
    )
    set_tests_properties(chipper_render_preset_sid_wide_pwm_pad_assert PROPERTIES DEPENDS chipper_render_preset_sid_wide_pwm_pad_smoke)

    add_test(NAME chipper_render_preset_sid_hard_sync_saw_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-sid-hard-sync-saw-smoke.json --implemented-contains partial --preset sid-hard-sync-saw --macro Lead --play-mode "Big Mono" --output-db -10 --register-writes 0 --note-events 0 --min-peak 0.01 --core-min filterModeChoice=3 --core-max filterModeChoice=3 --core-min filterModeBits=64 --core-max filterModeBits=64 --core-min filterRoutingChoice=3 --core-max filterRoutingChoice=3 --core-min filterRoutingBits=2 --core-max filterRoutingBits=2 --core-min modModeChoice=2 --core-max modModeChoice=2 --core-min sidModelChoice=2 --core-max sidModelChoice=2 --core-min waveformChoice0=2 --core-max waveformChoice0=2 --core-min waveformChoice1=2 --core-max waveformChoice1=2 --core-min sourceEnabled3=0 --core-max sourceEnabled3=0 --core-min sourceLevel1=0.86 --core-max sourceLevel1=0.86 --core-min sourceLevel2=0.74 --core-max sourceLevel2=0.74
    )
    set_tests_properties(chipper_render_preset_sid_hard_sync_saw_assert PROPERTIES DEPENDS chipper_render_preset_sid_hard_sync_saw_smoke)

    add_test(NAME chipper_render_preset_sid_gated_noise_tom_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-sid-gated-noise-tom-smoke.json --implemented-contains partial --preset sid-gated-noise-tom --macro Drum --play-mode "Big Mono" --output-db -11 --register-writes 0 --note-events 0 --min-peak 0.005 --core-min filterModeChoice=2 --core-max filterModeChoice=2 --core-min filterModeBits=32 --core-max filterModeBits=32 --core-min filterRoutingChoice=4 --core-max filterRoutingChoice=4 --core-min filterRoutingBits=4 --core-max filterRoutingBits=4 --core-min sidModelChoice=2 --core-max sidModelChoice=2 --core-min waveformChoice0=3 --core-max waveformChoice0=3 --core-min waveformChoice2=4 --core-max waveformChoice2=4 --core-min sourceEnabled1=1 --core-max sourceEnabled1=1 --core-min sourceEnabled2=0 --core-max sourceEnabled2=0 --core-min sourceEnabled3=1 --core-max sourceEnabled3=1 --core-min sourceLevel1=0.5 --core-max sourceLevel1=0.5 --core-min sourceLevel3=0.92 --core-max sourceLevel3=0.92
    )
    set_tests_properties(chipper_render_preset_sid_gated_noise_tom_assert PROPERTIES DEPENDS chipper_render_preset_sid_gated_noise_tom_smoke)

    add_test(NAME chipper_render_preset_ym_noise_hat_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-ym-noise-hat-smoke.json --implemented-contains partial --preset ym-noise-hat --macro Drum --play-mode "Big Mono" --output-db -12 --register-writes 0 --note-events 0 --min-peak 0.05 --min-zero-crossings 100 --core-min sourceEnabledA=1 --core-max sourceEnabledA=1 --core-min sourceEnabledB=0 --core-max sourceEnabledB=0 --core-min sourceEnabledC=0 --core-max sourceEnabledC=0 --core-min sourceEnabledNoise=1 --core-max sourceEnabledNoise=1 --core-min mixer=55 --core-max mixer=55 --core-min envelopeShapeChoice=1 --core-max envelopeShapeChoice=1
    )
    set_tests_properties(chipper_render_preset_ym_noise_hat_assert PROPERTIES DEPENDS chipper_render_preset_ym_noise_hat_smoke)

    add_test(NAME chipper_render_preset_ym_three_voice_arp_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-ym-three-voice-arp-smoke.json --implemented-contains partial --preset ym-three-voice-arp --macro Arp --play-mode "Chip Poly" --output-db -10 --register-writes 0 --note-events 0 --min-peak 0.05 --core-min sourceEnabledA=1 --core-max sourceEnabledA=1 --core-min sourceEnabledB=1 --core-max sourceEnabledB=1 --core-min sourceEnabledC=1 --core-max sourceEnabledC=1 --core-min sourceEnabledNoise=0 --core-max sourceEnabledNoise=0
    )
    set_tests_properties(chipper_render_preset_ym_three_voice_arp_assert PROPERTIES DEPENDS chipper_render_preset_ym_three_voice_arp_smoke)

    add_test(NAME chipper_render_preset_ym_fake_chord_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-ym-fake-chord-smoke.json --implemented-contains partial --preset ym-fake-chord-stack --macro Arp --play-mode "Big Mono" --output-db -10 --register-writes 0 --note-events 0 --min-peak 0.08 --min-zero-crossings 40 --core-min sourceEnabledA=1 --core-max sourceEnabledA=1 --core-min sourceEnabledB=1 --core-max sourceEnabledB=1 --core-min sourceEnabledC=1 --core-max sourceEnabledC=1 --core-min sourceEnabledNoise=0 --core-max sourceEnabledNoise=0
    )
    set_tests_properties(chipper_render_preset_ym_fake_chord_assert PROPERTIES DEPENDS chipper_render_preset_ym_fake_chord_smoke)

    add_test(NAME chipper_render_preset_ym_menu_beep_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-ym-menu-beep-smoke.json --implemented-contains partial --preset ym-menu-beep --macro Coin --play-mode "Big Mono" --output-db -9 --register-writes 0 --note-events 0 --min-peak 0.04 --core-min sourceEnabledA=1 --core-max sourceEnabledA=1 --core-min sourceEnabledB=0 --core-max sourceEnabledB=0 --core-min sourceEnabledC=0 --core-max sourceEnabledC=0 --core-min sourceEnabledNoise=0 --core-max sourceEnabledNoise=0
    )
    set_tests_properties(chipper_render_preset_ym_menu_beep_assert PROPERTIES DEPENDS chipper_render_preset_ym_menu_beep_smoke)

    add_test(NAME chipper_render_preset_ym_bright_beep_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-ym-bright-beep-smoke.json --implemented-contains partial --preset ym-bright-beep --macro Coin --play-mode "Big Mono" --output-db -11 --register-writes 0 --note-events 0 --min-peak 0.04 --core-min sourceEnabledA=1 --core-max sourceEnabledA=1 --core-min sourceEnabledB=0 --core-max sourceEnabledB=0 --core-min sourceEnabledC=0 --core-max sourceEnabledC=0 --core-min sourceEnabledNoise=0 --core-max sourceEnabledNoise=0
    )
    set_tests_properties(chipper_render_preset_ym_bright_beep_assert PROPERTIES DEPENDS chipper_render_preset_ym_bright_beep_smoke)

    add_test(NAME chipper_render_preset_ym_envelope_bell_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-ym-envelope-bell-smoke.json --implemented-contains partial --preset ym-envelope-bell --macro Lead --play-mode "Big Mono" --output-db -9 --register-writes 0 --note-events 0 --min-peak 0.02 --core-min envelopeShapeChoice=4 --core-max envelopeShapeChoice=4 --core-min sourceEnabledA=1 --core-max sourceEnabledA=1 --core-min sourceEnabledB=1 --core-max sourceEnabledB=1 --core-min sourceEnabledC=1 --core-max sourceEnabledC=1 --core-min sourceEnabledNoise=0 --core-max sourceEnabledNoise=0
    )
    set_tests_properties(chipper_render_preset_ym_envelope_bell_assert PROPERTIES DEPENDS chipper_render_preset_ym_envelope_bell_smoke)

    add_test(NAME chipper_render_preset_ym_arcade_clang_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-ym-arcade-clang-smoke.json --implemented-contains partial --preset ym-arcade-clang --macro Hit --play-mode "Big Mono" --output-db -12 --register-writes 0 --note-events 0 --min-peak 0.02 --min-zero-crossings 40 --core-min sourceEnabledA=1 --core-max sourceEnabledA=1 --core-min sourceEnabledB=1 --core-max sourceEnabledB=1 --core-min sourceEnabledC=0 --core-max sourceEnabledC=0 --core-min sourceEnabledNoise=1 --core-max sourceEnabledNoise=1 --core-min envelopeShapeChoice=2 --core-max envelopeShapeChoice=2
    )
    set_tests_properties(chipper_render_preset_ym_arcade_clang_assert PROPERTIES DEPENDS chipper_render_preset_ym_arcade_clang_smoke)

    add_test(NAME chipper_render_preset_ym_fast_minor_arp_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-ym-fast-minor-arp-smoke.json --implemented-contains partial --preset ym-fast-minor-arp --macro Arp --play-mode "Chip Poly" --output-db -10 --register-writes 0 --note-events 0 --min-peak 0.05 --core-min sourceEnabledA=1 --core-max sourceEnabledA=1 --core-min sourceEnabledB=1 --core-max sourceEnabledB=1 --core-min sourceEnabledC=1 --core-max sourceEnabledC=1 --core-min sourceEnabledNoise=0 --core-max sourceEnabledNoise=0
    )
    set_tests_properties(chipper_render_preset_ym_fast_minor_arp_assert PROPERTIES DEPENDS chipper_render_preset_ym_fast_minor_arp_smoke)

    add_test(NAME chipper_render_preset_sn_noise_hit_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-sn-noise-hit-smoke.json --implemented-contains partial --preset sn-noise-hit --macro Drum --play-mode "Big Mono" --output-db -12 --register-writes 0 --note-events 0 --min-peak 0.02 --min-zero-crossings 50 --core-min sourceEnabled0=0 --core-max sourceEnabled0=0 --core-min sourceEnabled1=0 --core-max sourceEnabled1=0 --core-min sourceEnabled2=0 --core-max sourceEnabled2=0 --core-min sourceEnabledNoise=1 --core-max sourceEnabledNoise=1 --core-min noiseModeChoice=3 --core-max noiseModeChoice=3 --core-min noiseControl=4 --core-max noiseControl=4 --core-min attenuationNoise=2 --core-max attenuationNoise=2
    )
    set_tests_properties(chipper_render_preset_sn_noise_hit_assert PROPERTIES DEPENDS chipper_render_preset_sn_noise_hit_smoke)

    add_test(NAME chipper_render_preset_sn_psg_lead_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-sn-psg-lead-smoke.json --implemented-contains partial --preset sn-psg-lead --macro Lead --play-mode "Big Mono" --output-db -7 --register-writes 0 --note-events 0 --min-peak 0.04 --core-min sourceEnabled0=1 --core-max sourceEnabled0=1 --core-min sourceEnabled1=1 --core-max sourceEnabled1=1 --core-min sourceEnabled2=1 --core-max sourceEnabled2=1 --core-min sourceEnabledNoise=0 --core-max sourceEnabledNoise=0 --core-min attenuation0=2 --core-max attenuation0=2
    )
    set_tests_properties(chipper_render_preset_sn_psg_lead_assert PROPERTIES DEPENDS chipper_render_preset_sn_psg_lead_smoke)

    add_test(NAME chipper_render_preset_sn_psg_coin_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-sn-psg-coin-smoke.json --implemented-contains partial --preset sn-psg-coin --macro Coin --play-mode "Big Mono" --output-db -11 --register-writes 0 --note-events 0 --min-peak 0.02 --min-zero-crossings 40 --core-min sourceEnabled0=1 --core-max sourceEnabled0=1 --core-min sourceEnabled1=0 --core-max sourceEnabled1=0 --core-min sourceEnabled2=0 --core-max sourceEnabled2=0 --core-min sourceEnabledNoise=0 --core-max sourceEnabledNoise=0 --core-min attenuation0=1 --core-max attenuation0=1
    )
    set_tests_properties(chipper_render_preset_sn_psg_coin_assert PROPERTIES DEPENDS chipper_render_preset_sn_psg_coin_smoke)

    add_test(NAME chipper_render_preset_sn_arcade_laser_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-sn-arcade-laser-smoke.json --implemented-contains partial --preset sn-arcade-laser --macro Laser --play-mode "Big Mono" --output-db -11 --register-writes 0 --note-events 0 --min-peak 0.04 --min-zero-crossings 50 --core-min sourceEnabled0=1 --core-max sourceEnabled0=1 --core-min sourceEnabled1=1 --core-max sourceEnabled1=1 --core-min sourceEnabled2=0 --core-max sourceEnabled2=0 --core-min sourceEnabledNoise=1 --core-max sourceEnabledNoise=1 --core-min noiseModeChoice=4 --core-max noiseModeChoice=4 --core-min noiseControl=7 --core-max noiseControl=7
    )
    set_tests_properties(chipper_render_preset_sn_arcade_laser_assert PROPERTIES DEPENDS chipper_render_preset_sn_arcade_laser_smoke)

    add_test(NAME chipper_render_preset_sn_tone_stack_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-sn-tone-stack-smoke.json --implemented-contains partial --preset sn-tone-stack --macro Lead --play-mode "Big Mono" --output-db -7 --register-writes 0 --note-events 0 --min-peak 0.08 --core-min sourceEnabled0=1 --core-max sourceEnabled0=1 --core-min sourceEnabled1=1 --core-max sourceEnabled1=1 --core-min sourceEnabled2=1 --core-max sourceEnabled2=1 --core-min sourceEnabledNoise=0 --core-max sourceEnabledNoise=0
    )
    set_tests_properties(chipper_render_preset_sn_tone_stack_assert PROPERTIES DEPENDS chipper_render_preset_sn_tone_stack_smoke)

    add_test(NAME chipper_render_preset_sn_arcade_bass_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-sn-arcade-bass-smoke.json --implemented-contains partial --preset sn-arcade-bass --macro Bass --play-mode "Big Mono" --output-db -8 --register-writes 0 --note-events 0 --min-peak 0.04 --core-min sourceEnabled0=1 --core-max sourceEnabled0=1 --core-min sourceEnabled1=0 --core-max sourceEnabled1=0 --core-min sourceEnabled2=0 --core-max sourceEnabled2=0 --core-min sourceEnabledNoise=0 --core-max sourceEnabledNoise=0
    )
    set_tests_properties(chipper_render_preset_sn_arcade_bass_assert PROPERTIES DEPENDS chipper_render_preset_sn_arcade_bass_smoke)

    add_test(NAME chipper_render_preset_sn_periodic_zap_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-sn-periodic-zap-smoke.json --implemented-contains partial --preset sn-periodic-zap --macro Laser --play-mode "Big Mono" --output-db -11 --register-writes 0 --note-events 0 --min-peak 0.04 --min-zero-crossings 50 --core-min sourceEnabled0=1 --core-max sourceEnabled0=1 --core-min sourceEnabled1=0 --core-max sourceEnabled1=0 --core-min sourceEnabled2=0 --core-max sourceEnabled2=0 --core-min sourceEnabledNoise=1 --core-max sourceEnabledNoise=1 --core-min noiseModeChoice=2 --core-max noiseModeChoice=2 --core-min noiseControl=2 --core-max noiseControl=2
    )
    set_tests_properties(chipper_render_preset_sn_periodic_zap_assert PROPERTIES DEPENDS chipper_render_preset_sn_periodic_zap_smoke)

    add_test(NAME chipper_render_preset_sn_warning_alarm_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-sn-warning-alarm-smoke.json --implemented-contains partial --preset sn-warning-alarm --macro Jump --play-mode "Big Mono" --output-db -11 --register-writes 0 --note-events 0 --min-peak 0.04 --min-zero-crossings 50 --core-min sourceEnabled0=1 --core-max sourceEnabled0=1 --core-min sourceEnabled1=1 --core-max sourceEnabled1=1 --core-min sourceEnabled2=0 --core-max sourceEnabled2=0 --core-min sourceEnabledNoise=1 --core-max sourceEnabledNoise=1 --core-min noiseModeChoice=4 --core-max noiseModeChoice=4 --core-min noiseControl=7 --core-max noiseControl=7
    )
    set_tests_properties(chipper_render_preset_sn_warning_alarm_assert PROPERTIES DEPENDS chipper_render_preset_sn_warning_alarm_smoke)

    add_test(NAME chipper_render_preset_nes_duty_bass_duo_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-nes-duty-bass-duo-smoke.json --implemented-contains partial --preset nes-duty-bass-duo --macro Bass --play-mode "Big Mono" --output-db -9 --register-writes 0 --note-events 0 --min-peak 0.04 --core-min sourceEnabled1=1 --core-max sourceEnabled1=1 --core-min sourceEnabled2=1 --core-max sourceEnabled2=1 --core-min sourceEnabled3=1 --core-max sourceEnabled3=1 --core-min sourceEnabled4=0 --core-max sourceEnabled4=0 --core-min sourceLevel1=0.52 --core-max sourceLevel1=0.52 --core-min sourceLevel2=0.38 --core-max sourceLevel2=0.38 --core-min sourceLevel3=1 --core-max sourceLevel3=1
    )
    set_tests_properties(chipper_render_preset_nes_duty_bass_duo_assert PROPERTIES DEPENDS chipper_render_preset_nes_duty_bass_duo_smoke)

    add_test(NAME chipper_render_preset_dmg_step_wave_arp_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-dmg-step-wave-arp-smoke.json --implemented-contains partial --preset dmg-step-wave-arp --macro Arp --play-mode "Big Mono" --output-db -10 --register-writes 0 --note-events 0 --min-peak 0.04 --core-min sourceEnabled1=1 --core-max sourceEnabled1=1 --core-min sourceEnabled2=1 --core-max sourceEnabled2=1 --core-min sourceEnabled3=1 --core-max sourceEnabled3=1 --core-min sourceEnabled4=0 --core-max sourceEnabled4=0 --core-min sourceLevel1=0.35 --core-max sourceLevel1=0.35 --core-min sourceLevel2=0.22 --core-max sourceLevel2=0.22 --core-min sourceLevel3=0.95 --core-max sourceLevel3=0.95 --core-min waveShape=4 --core-max waveShape=4 --core-min dmgWaveLevel=2 --core-max dmgWaveLevel=2 --core-min nr32=32 --core-max nr32=32 --core-min dmgStereoRoute=4 --core-max dmgStereoRoute=4 --core-min enabledWave=1 --core-max enabledWave=1
    )
    set_tests_properties(chipper_render_preset_dmg_step_wave_arp_assert PROPERTIES DEPENDS chipper_render_preset_dmg_step_wave_arp_smoke)

    add_test(NAME chipper_render_preset_dmg_pulse_lead_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-dmg-pulse-lead-smoke.json --implemented-contains partial --preset dmg-pulse-lead --macro Lead --play-mode "Big Mono" --output-db -10 --register-writes 0 --note-events 0 --min-peak 0.02 --core-min sourceEnabled1=1 --core-max sourceEnabled1=1 --core-min sourceEnabled2=1 --core-max sourceEnabled2=1 --core-min sourceEnabled3=0 --core-max sourceEnabled3=0 --core-min sourceEnabled4=0 --core-max sourceEnabled4=0 --core-min pulseDuty1=2 --core-max pulseDuty1=2 --core-min nr51=90 --core-max nr51=90
    )
    set_tests_properties(chipper_render_preset_dmg_pulse_lead_assert PROPERTIES DEPENDS chipper_render_preset_dmg_pulse_lead_smoke)

    add_test(NAME chipper_render_preset_dmg_wave_glass_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-dmg-wave-glass-smoke.json --implemented-contains partial --preset dmg-wave-glass --macro Lead --play-mode "Big Mono" --output-db -11 --register-writes 0 --note-events 0 --min-peak 0.01 --core-min sourceEnabled1=0 --core-max sourceEnabled1=0 --core-min sourceEnabled2=0 --core-max sourceEnabled2=0 --core-min sourceEnabled3=1 --core-max sourceEnabled3=1 --core-min sourceEnabled4=0 --core-max sourceEnabled4=0 --core-min enabledWave=1 --core-max enabledWave=1 --core-min waveShape=2 --core-max waveShape=2 --core-min nr32=64 --core-max nr32=64
    )
    set_tests_properties(chipper_render_preset_dmg_wave_glass_assert PROPERTIES DEPENDS chipper_render_preset_dmg_wave_glass_smoke)

    add_test(NAME chipper_render_preset_dmg_noise_kick_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-dmg-noise-kick-smoke.json --implemented-contains partial --preset dmg-noise-kick --macro Drum --play-mode "Big Mono" --output-db -12 --register-writes 0 --note-events 0 --min-peak 0.02 --min-zero-crossings 100 --core-min sourceEnabled1=0 --core-max sourceEnabled1=0 --core-min sourceEnabled2=0 --core-max sourceEnabled2=0 --core-min sourceEnabled3=0 --core-max sourceEnabled3=0 --core-min sourceEnabled4=1 --core-max sourceEnabled4=1 --core-min enabledNoise=1 --core-max enabledNoise=1 --core-min noiseModeChoice=2 --core-max noiseModeChoice=2
    )
    set_tests_properties(chipper_render_preset_dmg_noise_kick_assert PROPERTIES DEPENDS chipper_render_preset_dmg_noise_kick_smoke)

    add_test(NAME chipper_render_preset_dmg_split_chord_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-dmg-split-chord-smoke.json --implemented-contains partial --preset dmg-split-chord --macro Arp --play-mode "Chip Poly" --output-db -10 --register-writes 0 --note-events 0 --min-peak 0.01 --core-min sourceEnabled1=1 --core-max sourceEnabled1=1 --core-min sourceEnabled2=1 --core-max sourceEnabled2=1 --core-min sourceEnabled3=1 --core-max sourceEnabled3=1 --core-min sourceEnabled4=0 --core-max sourceEnabled4=0 --core-min waveShape=4 --core-max waveShape=4 --core-min nr51=90 --core-max nr51=90
    )
    set_tests_properties(chipper_render_preset_dmg_split_chord_assert PROPERTIES DEPENDS chipper_render_preset_dmg_split_chord_smoke)

    add_test(NAME chipper_render_preset_sid_filtered_noise_sweep_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-sid-filtered-noise-sweep-smoke.json --implemented-contains partial --preset sid-filtered-noise-sweep --macro Laser --play-mode "Big Mono" --output-db -12 --register-writes 0 --note-events 0 --min-peak 0.01 --core-min sourceEnabled1=0 --core-max sourceEnabled1=0 --core-min sourceEnabled2=1 --core-max sourceEnabled2=1 --core-min sourceEnabled3=1 --core-max sourceEnabled3=1 --core-min sourceLevel1=0.2 --core-max sourceLevel1=0.2 --core-min sourceLevel2=0.55 --core-max sourceLevel2=0.55 --core-min sourceLevel3=0.82 --core-max sourceLevel3=0.82 --core-min waveformChoice0=4 --core-max waveformChoice0=4 --core-min waveformChoice1=2 --core-max waveformChoice1=2 --core-min waveformChoice2=4 --core-max waveformChoice2=4 --core-min filterModeChoice=2 --core-max filterModeChoice=2 --core-min modModeChoice=2 --core-max modModeChoice=2 --core-min filterRoutingChoice=4 --core-max filterRoutingChoice=4 --core-min filterRoutingBits=4 --core-max filterRoutingBits=4
    )
    set_tests_properties(chipper_render_preset_sid_filtered_noise_sweep_assert PROPERTIES DEPENDS chipper_render_preset_sid_filtered_noise_sweep_smoke)

    add_test(NAME chipper_render_preset_ym_triangle_env_keys_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-ym-triangle-env-keys-smoke.json --implemented-contains partial --preset ym-triangle-env-keys --macro Lead --play-mode "Big Mono" --output-db -10 --register-writes 0 --note-events 0 --min-peak 0.006 --core-min sourceEnabledA=1 --core-max sourceEnabledA=1 --core-min sourceEnabledB=1 --core-max sourceEnabledB=1 --core-min sourceEnabledC=1 --core-max sourceEnabledC=1 --core-min sourceEnabledNoise=0 --core-max sourceEnabledNoise=0 --core-min sourceLevelA=0.95 --core-max sourceLevelA=0.95 --core-min sourceLevelB=0.68 --core-max sourceLevelB=0.68 --core-min sourceLevelC=0.48 --core-max sourceLevelC=0.48 --core-min envelopeShapeChoice=4 --core-max envelopeShapeChoice=4
    )
    set_tests_properties(chipper_render_preset_ym_triangle_env_keys_assert PROPERTIES DEPENDS chipper_render_preset_ym_triangle_env_keys_smoke)

    add_test(NAME chipper_render_preset_sn_periodic_bass_click_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/preset-sn-periodic-bass-click-smoke.json --implemented-contains partial --preset sn-periodic-bass-click --macro Bass --play-mode "Big Mono" --output-db -10 --register-writes 0 --note-events 0 --min-peak 0.04 --core-min sourceEnabled0=1 --core-max sourceEnabled0=1 --core-min sourceEnabled1=0 --core-max sourceEnabled1=0 --core-min sourceEnabled2=0 --core-max sourceEnabled2=0 --core-min sourceEnabledNoise=1 --core-max sourceEnabledNoise=1 --core-min sourceLevel0=0.92 --core-max sourceLevel0=0.92 --core-min sourceLevelNoise=0.38 --core-max sourceLevelNoise=0.38 --core-min noiseModeChoice=2 --core-max noiseModeChoice=2 --core-min noiseControl=2 --core-max noiseControl=2
    )
    set_tests_properties(chipper_render_preset_sn_periodic_bass_click_assert PROPERTIES DEPENDS chipper_render_preset_sn_periodic_bass_click_smoke)

    add_test(NAME chipper_render_dmg_json_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/dmg-smoke.json --implemented-contains partial --macro Lead --min-peak 0.08 --min-left-peak 0.1 --min-rms 0.03 --register-writes 0 --note-events 0 --min-zero-crossings 30 --max-zero-crossings 80 --core-min pulse1FreqReg=800 --core-max pulse1FreqReg=860 --core-min sweepRegister=28 --core-max sweepRegister=28 --core-min sweepPace=1 --core-max sweepPace=1 --core-min sweepShift=4 --core-max sweepShift=4 --core-min sweepSubtract=1 --core-max sweepSubtract=1 --core-min sweepEnabled=1 --core-max sweepEnabled=1 --core-min enabled0=1 --core-max enabled0=1 --core-min enabledNoise=0 --core-max enabledNoise=0 --core-min dmgStereoRoute=4 --core-max dmgStereoRoute=4 --core-min dmgStereoRouteRegister=90 --core-max dmgStereoRouteRegister=90 --core-min nr51=90 --core-max nr51=90 --descriptor-param-kind macroControl2=chipRegister --descriptor-param-surface macroControl2=slider --descriptor-param-label "macroControl2=Sweep Shift" --descriptor-param-kind snNoiseMode=chipRegister --descriptor-param-surface snNoiseMode=segmentedChoice --descriptor-param-label "snNoiseMode=Noise Mode" --descriptor-param-choices snNoiseMode=3
    )
    set_tests_properties(chipper_render_dmg_json_assert PROPERTIES DEPENDS chipper_render_dmg_smoke)

    add_test(NAME chipper_render_dmg_sweep_shift_min_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/dmg-sweep-shift-min-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --min-peak 0.1 --core-min sweepRegister=24 --core-max sweepRegister=24 --core-min sweepPace=1 --core-max sweepPace=1 --core-min sweepShift=0 --core-max sweepShift=0 --core-min sweepSubtract=1 --core-max sweepSubtract=1
    )
    set_tests_properties(chipper_render_dmg_sweep_shift_min_assert PROPERTIES DEPENDS chipper_render_dmg_sweep_shift_min_smoke)

    add_test(NAME chipper_render_dmg_sweep_shift_max_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/dmg-sweep-shift-max-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --min-peak 0.1 --core-min sweepRegister=31 --core-max sweepRegister=31 --core-min sweepPace=1 --core-max sweepPace=1 --core-min sweepShift=7 --core-max sweepShift=7 --core-min sweepSubtract=1 --core-max sweepSubtract=1
    )
    set_tests_properties(chipper_render_dmg_sweep_shift_max_assert PROPERTIES DEPENDS chipper_render_dmg_sweep_shift_max_smoke)

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

    add_test(NAME chipper_render_dmg_envelope_level_low_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/dmg-envelope-level-low-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --min-peak 0.005 --core-min initialEnvelope0=1 --core-max initialEnvelope0=1 --core-min envelope0=1 --core-max envelope0=1 --descriptor-param-kind macroControl4=chipRegister --descriptor-param-surface macroControl4=slider --descriptor-param-label "macroControl4=Envelope Level"
    )
    set_tests_properties(chipper_render_dmg_envelope_level_low_assert PROPERTIES DEPENDS chipper_render_dmg_envelope_level_low_smoke)

    add_test(NAME chipper_render_dmg_envelope_level_high_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/dmg-envelope-level-high-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --min-peak 0.1 --core-min initialEnvelope0=15 --core-max initialEnvelope0=15 --core-min envelope0=15 --core-max envelope0=15 --descriptor-param-kind macroControl4=chipRegister --descriptor-param-surface macroControl4=slider --descriptor-param-label "macroControl4=Envelope Level"
    )
    set_tests_properties(chipper_render_dmg_envelope_level_high_assert PROPERTIES DEPENDS chipper_render_dmg_envelope_level_high_smoke)

    add_test(NAME chipper_render_dmg_wave_triangle_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/dmg-wave-triangle-smoke.json --implemented-contains partial --macro Bass --register-writes 0 --note-events 0 --min-peak 0.03 --core-min waveShape=1 --core-max waveShape=1 --core-min waveRam0=1 --core-max waveRam0=1 --core-min waveRam15=16 --core-max waveRam15=16 --core-min enabledWave=1 --core-max enabledWave=1 --descriptor-param-kind waveShape=chipRegister --descriptor-param-surface waveShape=segmentedChoice --descriptor-param-choices waveShape=5
    )
    set_tests_properties(chipper_render_dmg_wave_triangle_assert PROPERTIES DEPENDS chipper_render_dmg_wave_triangle_smoke)

    add_test(NAME chipper_render_dmg_wave_pulse_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/dmg-wave-pulse-smoke.json --implemented-contains partial --macro Bass --register-writes 0 --note-events 0 --min-peak 0.03 --core-min waveShape=3 --core-max waveShape=3 --core-min waveRam0=255 --core-max waveRam0=255 --core-min waveRam15=0 --core-max waveRam15=0 --core-min enabledWave=1 --core-max enabledWave=1
    )
    set_tests_properties(chipper_render_dmg_wave_pulse_assert PROPERTIES DEPENDS chipper_render_dmg_wave_pulse_smoke)

    add_test(NAME chipper_render_dmg_wave_level_mute_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/dmg-wave-level-mute-smoke.json --implemented-contains partial --macro Bass --register-writes 0 --note-events 0 --max-peak 0.001 --max-rms 0.001 --core-min sourceEnabled3=1 --core-max sourceEnabled3=1 --core-min enabledWave=1 --core-max enabledWave=1 --core-min dmgWaveLevel=1 --core-max dmgWaveLevel=1 --core-min nr32=0 --core-max nr32=0 --core-min waveOutputLevelCode=0 --core-max waveOutputLevelCode=0 --descriptor-param-kind dmgWaveLevel=chipRegister --descriptor-param-surface dmgWaveLevel=segmentedChoice --descriptor-param-choices dmgWaveLevel=5
    )
    set_tests_properties(chipper_render_dmg_wave_level_mute_assert PROPERTIES DEPENDS chipper_render_dmg_wave_level_mute_smoke)

    add_test(NAME chipper_render_dmg_wave_level_full_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/dmg-wave-level-full-smoke.json --implemented-contains partial --macro Bass --register-writes 0 --note-events 0 --min-peak 0.03 --core-min dmgWaveLevel=2 --core-max dmgWaveLevel=2 --core-min nr32=32 --core-max nr32=32 --core-min waveOutputLevelCode=1 --core-max waveOutputLevelCode=1
    )
    set_tests_properties(chipper_render_dmg_wave_level_full_assert PROPERTIES DEPENDS chipper_render_dmg_wave_level_full_smoke)

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

    add_test(NAME chipper_render_dmg_noise_wide_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/dmg-noise-wide-smoke.json --implemented-contains partial --macro Drum --register-writes 0 --note-events 0 --core-min noiseModeChoice=1 --core-max noiseModeChoice=1 --core-min noiseRegister=2 --core-max noiseRegister=2 --core-min noiseWidth7=0 --core-max noiseWidth7=0 --core-min noiseShift=0 --core-max noiseShift=0 --core-min noiseDivisorCode=2 --core-max noiseDivisorCode=2 --descriptor-param-kind macroControl3=chipRegister --descriptor-param-surface macroControl3=slider --descriptor-param-label "macroControl3=Noise Clock"
    )
    set_tests_properties(chipper_render_dmg_noise_wide_assert PROPERTIES DEPENDS chipper_render_dmg_noise_wide_smoke)

    add_test(NAME chipper_render_dmg_noise_narrow_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/dmg-noise-narrow-smoke.json --implemented-contains partial --macro Drum --register-writes 0 --note-events 0 --core-min noiseModeChoice=2 --core-max noiseModeChoice=2 --core-min noiseRegister=122 --core-max noiseRegister=122 --core-min noiseWidth7=1 --core-max noiseWidth7=1 --core-min noiseShift=7 --core-max noiseShift=7 --core-min noiseDivisorCode=2 --core-max noiseDivisorCode=2 --descriptor-param-kind macroControl3=chipRegister --descriptor-param-surface macroControl3=slider --descriptor-param-label "macroControl3=Noise Clock"
    )
    set_tests_properties(chipper_render_dmg_noise_narrow_assert PROPERTIES DEPENDS chipper_render_dmg_noise_narrow_smoke)

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

    add_test(NAME chipper_render_dmg_route_left_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/dmg-route-left-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --min-left-rms 0.02 --max-right-rms 0.001 --core-min dmgStereoRoute=2 --core-max dmgStereoRoute=2 --core-min dmgStereoRouteRegister=240 --core-max dmgStereoRouteRegister=240 --core-min nr51=240 --core-max nr51=240 --descriptor-param-kind dmgStereoRoute=chipRegister --descriptor-param-surface dmgStereoRoute=segmentedChoice --descriptor-param-choices dmgStereoRoute=5 --descriptor-param-label "dmgStereoRoute=Stereo Route"
    )
    set_tests_properties(chipper_render_dmg_route_left_assert PROPERTIES DEPENDS chipper_render_dmg_route_left_smoke)

    add_test(NAME chipper_render_dmg_route_right_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/dmg-route-right-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --max-left-rms 0.001 --min-right-rms 0.02 --core-min dmgStereoRoute=3 --core-max dmgStereoRoute=3 --core-min dmgStereoRouteRegister=15 --core-max dmgStereoRouteRegister=15 --core-min nr51=15 --core-max nr51=15
    )
    set_tests_properties(chipper_render_dmg_route_right_assert PROPERTIES DEPENDS chipper_render_dmg_route_right_smoke)

    add_test(NAME chipper_render_dmg_route_split_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/dmg-route-split-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --min-left-rms 0.02 --max-right-rms 0.001 --core-min dmgStereoRoute=4 --core-max dmgStereoRoute=4 --core-min dmgStereoRouteRegister=90 --core-max dmgStereoRouteRegister=90 --core-min nr51=90 --core-max nr51=90
    )
    set_tests_properties(chipper_render_dmg_route_split_assert PROPERTIES DEPENDS chipper_render_dmg_route_split_smoke)

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

    add_test(NAME chipper_render_nes_dmc_direct_parameter_low_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/nes-dmc-direct-parameter-low-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --max-peak 0.001 --max-rms 0.001 --core-min dmcDirectControl=0 --core-max dmcDirectControl=0 --core-min dmcLevel=0 --core-max dmcLevel=0
    )
    set_tests_properties(chipper_render_nes_dmc_direct_parameter_low_assert PROPERTIES DEPENDS chipper_render_nes_dmc_direct_parameter_low_smoke)

    add_test(NAME chipper_render_nes_dmc_direct_parameter_high_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/nes-dmc-direct-parameter-high-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --min-peak 0.65 --max-peak 0.95 --core-min dmcDirectControl=1 --core-max dmcDirectControl=1 --core-min dmcLevel=127 --core-max dmcLevel=127
    )
    set_tests_properties(chipper_render_nes_dmc_direct_parameter_high_assert PROPERTIES DEPENDS chipper_render_nes_dmc_direct_parameter_high_smoke)

    add_test(NAME chipper_render_nes_dmc_sample_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/nes-dmc-sample-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --min-peak 0.2 --core-min dmcSampleLoaded=1 --core-max dmcSampleLoaded=1 --core-min dmcSampleBytes=4 --core-max dmcSampleBytes=4 --core-min dmcSampleBitsPlayed=30 --core-min dmcLevel=40 --core-max dmcLevel=90
    )
    set_tests_properties(chipper_render_nes_dmc_sample_assert PROPERTIES DEPENDS chipper_render_nes_dmc_sample_smoke)

    add_test(NAME chipper_render_nes_dmc_rate0_sample_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/nes-dmc-rate0-sample-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --core-min dmcSampleLoaded=1 --core-max dmcSampleLoaded=1 --core-min dmcRateIndex=0 --core-max dmcRateIndex=0 --core-min dmcRatePeriodCycles=428 --core-max dmcRatePeriodCycles=428
    )
    set_tests_properties(chipper_render_nes_dmc_rate0_sample_assert PROPERTIES DEPENDS chipper_render_nes_dmc_rate0_sample_smoke)

    add_test(NAME chipper_render_nes_dmc_rate15_sample_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/nes-dmc-rate15-sample-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --core-min dmcSampleLoaded=1 --core-max dmcSampleLoaded=1 --core-min dmcRateIndex=15 --core-max dmcRateIndex=15 --core-min dmcRatePeriodCycles=54 --core-max dmcRatePeriodCycles=54 --core-min dmcSampleBitsPlayed=30
    )
    set_tests_properties(chipper_render_nes_dmc_rate15_sample_assert PROPERTIES DEPENDS chipper_render_nes_dmc_rate15_sample_smoke)

    add_test(NAME chipper_render_nes_dmc_chip_poly_sample_assert
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/nes-dmc-chip-poly-sample-smoke.json --implemented-contains partial --macro Manual --play-mode "Chip Poly" --register-writes 0 --note-events 0 --min-peak 0.2 --core-min sourceEnabled4=1 --core-max sourceEnabled4=1 --core-min dmcSampleLoaded=1 --core-max dmcSampleLoaded=1 --core-min dmcSampleBytes=4 --core-max dmcSampleBytes=4 --core-min dmcSampleBitsPlayed=30 --core-min dmcLevel=40 --core-max dmcLevel=90
    )
    set_tests_properties(chipper_render_nes_dmc_chip_poly_sample_assert PROPERTIES DEPENDS chipper_render_nes_dmc_chip_poly_sample_smoke)

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

add_executable(chipper_parameter_midi_cc_smoke
    tests/parameter_midi_cc_smoke.cpp
    Source/Parameters.cpp
    Source/Parameters.h
)

target_include_directories(chipper_parameter_midi_cc_smoke PRIVATE Source)

target_compile_definitions(chipper_parameter_midi_cc_smoke
    PUBLIC
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
)

target_link_libraries(chipper_parameter_midi_cc_smoke
    PRIVATE
        chipper_engine
        juce::juce_audio_processors
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

add_test(NAME chipper_parameter_midi_cc_smoke
    COMMAND chipper_parameter_midi_cc_smoke
)

add_executable(chipper_processor_midi_cc_smoke
    tests/processor_midi_cc_smoke.cpp
    Source/Parameters.cpp
    Source/Parameters.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
)

add_dependencies(chipper_processor_midi_cc_smoke chipper_build_info)

target_include_directories(chipper_processor_midi_cc_smoke
    PRIVATE
        Source
        "${CHIPPER_GENERATED_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}/Chipper_artefacts/JuceLibraryCode"
)

target_compile_definitions(chipper_processor_midi_cc_smoke
    PUBLIC
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JucePlugin_Name="Chipper"
)

target_link_libraries(chipper_processor_midi_cc_smoke
    PRIVATE
        chipper_engine
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

add_test(NAME chipper_processor_midi_cc_smoke
    COMMAND chipper_processor_midi_cc_smoke
)
