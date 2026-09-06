add_executable(chipper_descriptor_smoke tests/descriptor_smoke.cpp)
target_link_libraries(chipper_descriptor_smoke PRIVATE chipper_engine)

add_executable(chipper_yamaha_adpcm_codec_smoke tests/yamaha_adpcm_codec_smoke.cpp)
target_link_libraries(chipper_yamaha_adpcm_codec_smoke PRIVATE chipper_engine)

if (Python3_Interpreter_FOUND)
    add_test(NAME chipper_reference_comparator_selftest
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_compare_reference_wav.py
    )
endif()
add_test(NAME chipper_descriptor_smoke
    COMMAND chipper_descriptor_smoke
)
add_test(NAME chipper_yamaha_adpcm_codec_smoke
    COMMAND chipper_yamaha_adpcm_codec_smoke
)
set_tests_properties(chipper_yamaha_adpcm_codec_smoke PROPERTIES
    LABELS "codec;release-gate"
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
add_test(NAME chipper_render_nes_vrc6_smoke
    COMMAND chipper_render --chip nesVrc6 --accuracy authentic --macro lead --source1 0 --source2 0 --source3 0 --source4 0 --source5 1 --source6 1 --source7 1 --clock 1789773 --rate 48000 --seconds 0.06 --note 69 --out nes-vrc6-smoke.wav --debug nes-vrc6-smoke.json
)
add_test(NAME chipper_render_nes_vrc6_sources_off_smoke
    COMMAND chipper_render --chip nesVrc6 --accuracy authentic --macro manual --source1 0 --source2 0 --source3 0 --source4 0 --source5 0 --source6 0 --source7 0 --clock 1789773 --rate 48000 --seconds 0.02 --note 69 --out nes-vrc6-sources-off-smoke.wav --debug nes-vrc6-sources-off-smoke.json
)
add_test(NAME chipper_render_nes_vrc6_chip_poly_smoke
    COMMAND chipper_render --chip nesVrc6 --accuracy authentic --macro manual --play-mode chip-poly --source1 1 --source2 1 --source3 1 --source4 0 --source5 1 --source6 1 --source7 1 --clock 1789773 --rate 48000 --seconds 0.04 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/nes-vrc6-six-voice-events.txt --out nes-vrc6-chip-poly-smoke.wav --debug nes-vrc6-chip-poly-smoke.json
)
add_test(NAME chipper_render_nes_fds_smoke
    COMMAND chipper_render --chip nesFds --accuracy authentic --macro lead --source1 0 --source2 0 --source3 0 --source4 0 --source5 1 --clock 1789773 --rate 48000 --seconds 0.06 --note 69 --out nes-fds-smoke.wav --debug nes-fds-smoke.json
)
add_test(NAME chipper_render_nes_fds_sources_off_smoke
    COMMAND chipper_render --chip nesFds --accuracy authentic --macro manual --source1 0 --source2 0 --source3 0 --source4 0 --source5 0 --clock 1789773 --rate 48000 --seconds 0.02 --note 69 --out nes-fds-sources-off-smoke.wav --debug nes-fds-sources-off-smoke.json
)
add_test(NAME chipper_render_nes_fds_chip_poly_smoke
    COMMAND chipper_render --chip nesFds --accuracy authentic --macro manual --play-mode chip-poly --source1 1 --source2 1 --source3 1 --source4 0 --source5 1 --clock 1789773 --rate 48000 --seconds 0.04 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/nes-fds-four-voice-events.txt --out nes-fds-chip-poly-smoke.wav --debug nes-fds-chip-poly-smoke.json
)
add_test(NAME chipper_render_nes_sunsoft5b_smoke
    COMMAND chipper_render --chip nesSunsoft5b --accuracy authentic --macro lead --source1 0 --source2 0 --source3 0 --source4 0 --source5 1 --source6 1 --source7 1 --clock 1789773 --rate 48000 --seconds 0.06 --note 69 --out nes-sunsoft5b-smoke.wav --debug nes-sunsoft5b-smoke.json
)
add_test(NAME chipper_render_nes_sunsoft5b_sources_off_smoke
    COMMAND chipper_render --chip nesSunsoft5b --accuracy authentic --macro manual --source1 0 --source2 0 --source3 0 --source4 0 --source5 0 --source6 0 --source7 0 --clock 1789773 --rate 48000 --seconds 0.02 --note 69 --out nes-sunsoft5b-sources-off-smoke.wav --debug nes-sunsoft5b-sources-off-smoke.json
)
add_test(NAME chipper_render_nes_sunsoft5b_chip_poly_smoke
    COMMAND chipper_render --chip nesSunsoft5b --accuracy authentic --macro manual --play-mode chip-poly --source1 1 --source2 1 --source3 1 --source4 0 --source5 1 --source6 1 --source7 1 --clock 1789773 --rate 48000 --seconds 0.04 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/nes-sunsoft5b-six-voice-events.txt --out nes-sunsoft5b-chip-poly-smoke.wav --debug nes-sunsoft5b-chip-poly-smoke.json
)
add_test(NAME chipper_render_nes_mmc5_smoke
    COMMAND chipper_render --chip nesMmc5 --accuracy authentic --macro lead --source1 0 --source2 0 --source3 0 --source4 0 --source5 1 --source6 1 --source7 1 --clock 1789773 --rate 48000 --seconds 0.06 --note 69 --out nes-mmc5-smoke.wav --debug nes-mmc5-smoke.json
)
add_test(NAME chipper_render_nes_mmc5_pcm_smoke
    COMMAND chipper_render --chip nesMmc5 --accuracy authentic --macro drum --source1 0 --source2 0 --source3 0 --source4 1 --source5 0 --source6 0 --source7 1 --clock 1789773 --rate 48000 --seconds 0.06 --note 48 --out nes-mmc5-pcm-smoke.wav --debug nes-mmc5-pcm-smoke.json
)
add_test(NAME chipper_render_nes_mmc5_sources_off_smoke
    COMMAND chipper_render --chip nesMmc5 --accuracy authentic --macro manual --source1 0 --source2 0 --source3 0 --source4 0 --source5 0 --source6 0 --source7 0 --clock 1789773 --rate 48000 --seconds 0.02 --note 69 --out nes-mmc5-sources-off-smoke.wav --debug nes-mmc5-sources-off-smoke.json
)
add_test(NAME chipper_render_nes_mmc5_chip_poly_smoke
    COMMAND chipper_render --chip nesMmc5 --accuracy authentic --macro manual --play-mode chip-poly --source1 1 --source2 1 --source3 1 --source4 0 --source5 1 --source6 1 --source7 1 --clock 1789773 --rate 48000 --seconds 0.04 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/nes-mmc5-five-voice-events.txt --out nes-mmc5-chip-poly-smoke.wav --debug nes-mmc5-chip-poly-smoke.json
)
add_test(NAME chipper_render_nes_vrc7_smoke
    COMMAND chipper_render --chip nesVrc7 --accuracy authentic --macro lead --source1 0 --source2 0 --source3 0 --source4 1 --source5 1 --source6 1 --source7 1 --source8 1 --source9 1 --clock 1789773 --rate 48000 --seconds 0.06 --note 69 --out nes-vrc7-smoke.wav --debug nes-vrc7-smoke.json
)
add_test(NAME chipper_render_nes_vrc7_custom_patch_smoke
    COMMAND chipper_render --chip nesVrc7 --accuracy authentic --macro lead --wave-shape custom --control3 0.42 --control4 0.72 --source1 0 --source2 0 --source3 0 --source4 1 --source5 1 --source6 1 --source7 1 --source8 1 --source9 1 --fm-op1-level 0.72 --fm-op2-level 0.68 --fm-op1-multiplier 3 --fm-op2-multiplier 1 --fm-op1-attack-rate 15 --fm-op2-attack-rate 14 --fm-op1-decay-rate 7 --fm-op2-decay-rate 5 --fm-op1-sustain-rate 4 --fm-op2-sustain-rate 5 --fm-op1-release-rate 6 --fm-op2-release-rate 4 --clock 1789773 --rate 48000 --seconds 0.06 --note 69 --out nes-vrc7-custom-patch-smoke.wav --debug nes-vrc7-custom-patch-smoke.json
)
add_test(NAME chipper_render_nes_vrc7_sources_off_smoke
    COMMAND chipper_render --chip nesVrc7 --accuracy authentic --macro manual --source1 0 --source2 0 --source3 0 --source4 0 --source5 0 --source6 0 --source7 0 --source8 0 --source9 0 --clock 1789773 --rate 48000 --seconds 0.02 --note 69 --out nes-vrc7-sources-off-smoke.wav --debug nes-vrc7-sources-off-smoke.json
)
add_test(NAME chipper_render_nes_vrc7_chip_poly_smoke
    COMMAND chipper_render --chip nesVrc7 --accuracy authentic --macro manual --play-mode chip-poly --source1 1 --source2 1 --source3 1 --source4 1 --source5 1 --source6 1 --source7 1 --source8 1 --source9 1 --clock 1789773 --rate 48000 --seconds 0.06 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/nes-vrc7-nine-voice-events.txt --out nes-vrc7-chip-poly-smoke.wav --debug nes-vrc7-chip-poly-smoke.json
)
add_test(NAME chipper_render_saa1099_smoke
    COMMAND chipper_render --chip saa1099 --accuracy authentic --macro lead --source1 1 --source2 1 --source3 1 --source4 1 --source5 1 --source6 1 --clock 8000000 --rate 48000 --seconds 0.06 --note 69 --out saa1099-smoke.wav --debug saa1099-smoke.json
)
add_test(NAME chipper_render_saa1099_sources_off_smoke
    COMMAND chipper_render --chip saa1099 --accuracy authentic --macro manual --source1 0 --source2 0 --source3 0 --source4 0 --source5 0 --source6 0 --clock 8000000 --rate 48000 --seconds 0.02 --note 69 --out saa1099-sources-off-smoke.wav --debug saa1099-sources-off-smoke.json
)
add_test(NAME chipper_render_saa1099_chip_poly_smoke
    COMMAND chipper_render --chip saa1099 --accuracy authentic --macro manual --play-mode chip-poly --source1 1 --source2 1 --source3 1 --source4 1 --source5 1 --source6 1 --clock 8000000 --rate 48000 --seconds 0.06 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/saa1099-six-voice-events.txt --out saa1099-chip-poly-smoke.wav --debug saa1099-chip-poly-smoke.json
)
add_test(NAME chipper_render_pc_speaker_smoke
    COMMAND chipper_render --chip pcSpeaker --accuracy authentic --macro lead --source1 1 --clock 1193182 --rate 48000 --seconds 0.06 --note 69 --out pc-speaker-smoke.wav --debug pc-speaker-smoke.json
)
add_test(NAME chipper_render_pc_speaker_sources_off_smoke
    COMMAND chipper_render --chip pcSpeaker --accuracy authentic --macro manual --source1 0 --clock 1193182 --rate 48000 --seconds 0.02 --note 69 --out pc-speaker-sources-off-smoke.wav --debug pc-speaker-sources-off-smoke.json
)
add_test(NAME chipper_render_pc_speaker_click_smoke
    COMMAND chipper_render --chip pcSpeaker --accuracy authentic --macro drum --wave-shape 2 --control3 0.94 --clock 1193182 --rate 48000 --seconds 0.04 --note 48 --out pc-speaker-click-smoke.wav --debug pc-speaker-click-smoke.json
)
add_test(NAME chipper_render_zx_spectrum_beeper_smoke
    COMMAND chipper_render --chip zxSpectrum --accuracy authentic --macro lead --source1 1 --clock 3500000 --rate 48000 --seconds 0.06 --note 69 --out zx-spectrum-beeper-smoke.wav --debug zx-spectrum-beeper-smoke.json
)
add_test(NAME chipper_render_zx_spectrum_beeper_sources_off_smoke
    COMMAND chipper_render --chip zxSpectrum --accuracy authentic --macro manual --source1 0 --clock 3500000 --rate 48000 --seconds 0.02 --note 69 --out zx-spectrum-beeper-sources-off-smoke.wav --debug zx-spectrum-beeper-sources-off-smoke.json
)
add_test(NAME chipper_render_zx_spectrum_beeper_mic_smoke
    COMMAND chipper_render --chip zxSpectrum --accuracy authentic --macro drum --wave-shape 2 --control3 0.94 --clock 3500000 --rate 48000 --seconds 0.04 --note 48 --out zx-spectrum-beeper-mic-smoke.wav --debug zx-spectrum-beeper-mic-smoke.json
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

add_test(NAME chipper_render_yamaha_adpcm_a_opna_reference
    COMMAND chipper_render --chip ym2608 --accuracy authentic --macro manual --clock 7987200 --rate 48000 --seconds 0.2 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/ym2608-adpcm-a-reference-events.txt --source1 1 --source2 0 --source3 0 --source4 0 --source5 0 --source6 0 --source7 0 --source8 0 --source9 0 --stereo-spread 0 --output-db 0 --opna-rhythm-rom ${CMAKE_CURRENT_SOURCE_DIR}/tests/references/yamaha-adpcm-a-synthetic-opna.bin --out yamaha-adpcm-a-opna-candidate.wav --debug yamaha-adpcm-a-opna-candidate.json
)
set_tests_properties(chipper_render_yamaha_adpcm_a_opna_reference PROPERTIES
    LABELS "audio;reference;release-gate"
)
add_test(NAME chipper_render_yamaha_adpcm_a_opnb_reference
    COMMAND chipper_render --chip ym2610 --accuracy authentic --macro manual --clock 8000000 --rate 48000 --seconds 0.2 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/ym2610-adpcm-a-reference-events.txt --source1 1 --source2 0 --source3 0 --source4 0 --source5 0 --source6 0 --source7 0 --source8 0 --source9 0 --stereo-spread 0 --output-db 0 --opnb-adpcm-a-sample ${CMAKE_CURRENT_SOURCE_DIR}/tests/references/yamaha-adpcm-a-synthetic-opnb.bin --out yamaha-adpcm-a-opnb-candidate.wav --debug yamaha-adpcm-a-opnb-candidate.json
)
set_tests_properties(chipper_render_yamaha_adpcm_a_opnb_reference PROPERTIES
    LABELS "audio;reference;release-gate"
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
add_test(NAME chipper_render_spc700_smoke
    COMMAND chipper_render --chip spc700 --accuracy authentic --macro lead --wave-shape 1 --clock 32000 --rate 48000 --seconds 0.05 --note 64 --out spc700-smoke.wav --debug spc700-smoke.json
)
add_test(NAME chipper_render_spc700_noise_smoke
    COMMAND chipper_render --chip spc700 --accuracy authentic --macro drum --wave-shape noise --source1 0 --source2 0 --source3 1 --source4 1 --source5 0 --source6 0 --source7 0 --source8 0 --clock 32000 --rate 48000 --seconds 0.05 --note 48 --out spc700-noise-smoke.wav --debug spc700-noise-smoke.json
)
add_test(NAME chipper_render_spc700_noise_clock_smoke
    COMMAND chipper_render --chip spc700 --accuracy authentic --macro drum --spc700-noise mid --source1 0 --source2 1 --source3 1 --source4 1 --source5 0 --source6 0 --source7 0 --source8 0 --clock 32000 --rate 48000 --seconds 0.05 --note 48 --out spc700-noise-clock-smoke.wav --debug spc700-noise-clock-smoke.json
)
add_test(NAME chipper_render_spc700_chip_poly_smoke
    COMMAND chipper_render --chip spc700 --accuracy authentic --macro manual --play-mode chip-poly --wave-shape 2 --clock 32000 --rate 48000 --seconds 0.05 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/spc700-eight-chip-poly-events.txt --out spc700-chip-poly-smoke.wav --debug spc700-chip-poly-smoke.json
)
add_test(NAME chipper_render_spc700_loop_smoke
    COMMAND chipper_render --chip spc700 --accuracy authentic --macro lead --wave-shape pulse --spc700-playback loop --clock 32000 --rate 48000 --seconds 0.08 --note 64 --out spc700-loop-smoke.wav --debug spc700-loop-smoke.json
)
add_test(NAME chipper_render_spc700_envelope_pad_smoke
    COMMAND chipper_render --chip spc700 --accuracy authentic --macro lead --wave-shape pulse --spc700-envelope pad --envelope-decay 0.0 --spc700-playback loop --clock 32000 --rate 48000 --seconds 0.05 --note 64 --out spc700-envelope-pad-smoke.wav --debug spc700-envelope-pad-smoke.json
)
add_test(NAME chipper_render_spc700_one_shot_smoke
    COMMAND chipper_render --chip spc700 --accuracy authentic --macro lead --wave-shape pulse --spc700-playback one-shot --clock 32000 --rate 48000 --seconds 0.08 --note 64 --out spc700-one-shot-smoke.wav --debug spc700-one-shot-smoke.json
)
add_test(NAME chipper_render_spc700_brr_smoke
    COMMAND chipper_render --chip spc700 --accuracy authentic --macro lead --spc700-brr-hex 817f7f7f7f80808080 --spc700-playback loop --clock 32000 --rate 48000 --seconds 0.05 --note 64 --out spc700-brr-smoke.wav --debug spc700-brr-smoke.json
)
add_test(NAME chipper_render_spc700_brr_loop_point_smoke
    COMMAND chipper_render --chip spc700 --accuracy authentic --macro lead --spc700-brr-hex 000000000000000000037f7f7f7f80808080 --spc700-playback loop --clock 32000 --rate 48000 --seconds 0.08 --note 64 --out spc700-brr-loop-point-smoke.wav --debug spc700-brr-loop-point-smoke.json
)
add_test(NAME chipper_render_spc700_explicit_loop_point_smoke
    COMMAND chipper_render --chip spc700 --accuracy authentic --macro lead --spc700-brr-hex 000000000000000000037f7f7f7f80808080 --spc700-playback loop --spc700-loop-start 0.25 --spc700-loop-end 0.75 --clock 32000 --rate 48000 --seconds 0.08 --note 64 --out spc700-explicit-loop-point-smoke.wav --debug spc700-explicit-loop-point-smoke.json
)
add_test(NAME chipper_render_spc700_brr_bank_slot_smoke
    COMMAND chipper_render --chip spc700 --accuracy authentic --macro lead --spc700-brr-bank-hex 817f7f7f7f80808080 --spc700-brr-bank-hex 837f7f7f7f80808080 --spc700-sample-slot 1 --spc700-playback loop --clock 32000 --rate 48000 --seconds 0.08 --note 64 --out spc700-brr-bank-slot-smoke.wav --debug spc700-brr-bank-slot-smoke.json
)
add_test(NAME chipper_render_spc700_voice_sample_slots_smoke
    COMMAND chipper_render --chip spc700 --accuracy authentic --macro lead --spc700-brr-bank-hex 817f7f7f7f80808080 --spc700-brr-bank-hex 807f7f7f7f80808080837f7f7f7f80808080 --spc700-sample-slot 0 --spc700-sample-slot1 2 --spc700-sample-slot2 1 --spc700-sample-slot3 2 --spc700-sample-slot4 1 --spc700-sample-slot5 2 --spc700-sample-slot6 1 --spc700-sample-slot7 2 --spc700-sample-slot8 1 --spc700-playback loop --clock 32000 --rate 48000 --seconds 0.08 --note 64 --out spc700-voice-sample-slots-smoke.wav --debug spc700-voice-sample-slots-smoke.json
)
add_test(NAME chipper_render_spc700_brr_note_map_smoke
    COMMAND chipper_render --chip spc700 --accuracy authentic --macro lead --spc700-brr-bank-hex 817f7f7f7f80808080 --spc700-brr-bank-hex 837f7f7f7f80808080 --spc700-map-root 60 --spc700-playback loop --clock 32000 --rate 48000 --seconds 0.08 --note 61 --out spc700-brr-note-map-smoke.wav --debug spc700-brr-note-map-smoke.json
)
add_test(NAME chipper_render_spc700_pitch_motion_smoke
    COMMAND chipper_render --chip spc700 --accuracy authentic --macro laser --clock 32000 --rate 48000 --seconds 0.08 --note 64 --out spc700-pitch-motion-smoke.wav --debug spc700-pitch-motion-smoke.json
)
add_test(NAME chipper_render_spc700_pitch_mod_smoke
    COMMAND chipper_render --chip spc700 --accuracy authentic --macro arp --play-mode chip-poly --control2 1.0 --wave-shape pulse --clock 32000 --rate 48000 --seconds 0.08 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/spc700-chip-poly-events.txt --out spc700-pitch-mod-smoke.wav --debug spc700-pitch-mod-smoke.json
)
add_test(NAME chipper_render_spc700_echo_smoke
    COMMAND chipper_render --chip spc700 --accuracy authentic --macro lead --control3 1.0 --clock 32000 --rate 48000 --seconds 0.30 --note 64 --out spc700-echo-smoke.wav --debug spc700-echo-smoke.json
)
add_test(NAME chipper_render_spc700_pan_echo_smoke
    COMMAND chipper_render --chip spc700 --accuracy authentic --macro lead --control3 1.0 --stereo-spread 1.0 --clock 32000 --rate 48000 --seconds 0.10 --note 64 --out spc700-pan-echo-smoke.wav --debug spc700-pan-echo-smoke.json
)
add_test(NAME chipper_render_spc700_release_smoke
    COMMAND chipper_render --chip spc700 --accuracy authentic --macro lead --clock 32000 --rate 48000 --seconds 0.08 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/spc700-release-events.txt --out spc700-release-smoke.wav --debug spc700-release-smoke.json
)
add_test(NAME chipper_render_preset_spc700_noise_snare_smoke
    COMMAND chipper_render --preset spc700-noise-snare --rate 48000 --seconds 0.05 --note 48 --out preset-spc700-noise-snare.wav --debug preset-spc700-noise-snare.json
)
add_test(NAME chipper_render_preset_spc700_echo_pad_smoke
    COMMAND chipper_render --preset spc700-echo-pad --rate 48000 --seconds 0.12 --note 60 --out preset-spc700-echo-pad.wav --debug preset-spc700-echo-pad.json
)
add_test(NAME chipper_render_preset_spc700_muted_pluck_smoke
    COMMAND chipper_render --preset spc700-muted-pluck --rate 48000 --seconds 0.06 --note 72 --out preset-spc700-muted-pluck.wav --debug preset-spc700-muted-pluck.json
)
add_test(NAME chipper_render_preset_spc700_pmon_shimmer_smoke
    COMMAND chipper_render --preset spc700-pmon-shimmer --rate 48000 --seconds 0.08 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/spc700-chip-poly-events.txt --out preset-spc700-pmon-shimmer.wav --debug preset-spc700-pmon-shimmer.json
)
add_test(NAME chipper_render_preset_spc700_tracker_lead_smoke
    COMMAND chipper_render --preset spc700-tracker-lead --rate 48000 --seconds 0.08 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/spc700-chip-poly-events.txt --out preset-spc700-tracker-lead.wav --debug preset-spc700-tracker-lead.json
)
add_test(NAME chipper_render_preset_spc700_tracker_bass_smoke
    COMMAND chipper_render --preset spc700-tracker-bass --rate 48000 --seconds 0.08 --note 48 --out preset-spc700-tracker-bass.wav --debug preset-spc700-tracker-bass.json
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
add_test(NAME chipper_render_preset_nes_tracker_noise_tick_smoke
    COMMAND chipper_render --preset nes-tracker-noise-tick --rate 48000 --seconds 0.04 --note 60 --out preset-nes-tracker-noise-tick.wav --debug preset-nes-tracker-noise-tick.json
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
add_test(NAME chipper_render_preset_ym_tracker_lead_smoke
    COMMAND chipper_render --preset ym-tracker-lead --rate 48000 --seconds 0.04 --note 72 --out preset-ym-tracker-lead-smoke.wav --debug preset-ym-tracker-lead-smoke.json
)
add_test(NAME chipper_render_preset_ym_tracker_square_arp_smoke
    COMMAND chipper_render --preset ym-tracker-square-arp --rate 48000 --seconds 0.04 --note 72 --out preset-ym-tracker-square-arp-smoke.wav --debug preset-ym-tracker-square-arp-smoke.json
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
add_test(NAME chipper_render_preset_sn_periodic_alarm_smoke
    COMMAND chipper_render --preset sn-periodic-alarm --rate 48000 --seconds 0.04 --note 72 --out preset-sn-periodic-alarm-smoke.wav --debug preset-sn-periodic-alarm-smoke.json
)
add_test(NAME chipper_render_preset_sn_pulse_alarm_smoke
    COMMAND chipper_render --preset sn-pulse-alarm --rate 48000 --seconds 0.04 --note 72 --out preset-sn-pulse-alarm-smoke.wav --debug preset-sn-pulse-alarm-smoke.json
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
add_test(NAME chipper_render_preset_dmg_stereo_sweep_lead_smoke
    COMMAND chipper_render --preset dmg-stereo-sweep-lead --rate 48000 --seconds 0.05 --note 72 --out preset-dmg-stereo-sweep-lead.wav --debug preset-dmg-stereo-sweep-lead.json
)
add_test(NAME chipper_render_preset_dmg_stereo_sweep_bass_smoke
    COMMAND chipper_render --preset dmg-stereo-sweep-bass --rate 48000 --seconds 0.05 --note 48 --out preset-dmg-stereo-sweep-bass.wav --debug preset-dmg-stereo-sweep-bass.json
)
add_test(NAME chipper_render_preset_dmg_duty_noise_pop_smoke
    COMMAND chipper_render --preset dmg-duty-noise-pop --rate 48000 --seconds 0.04 --note 60 --out preset-dmg-duty-noise-pop.wav --debug preset-dmg-duty-noise-pop.json
)
add_test(NAME chipper_render_preset_huc_lfo_chord_arp_smoke
    COMMAND chipper_render --preset huc-lfo-chord-arp --rate 48000 --seconds 0.08 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/huc6280-chip-poly-events.txt --out preset-huc-lfo-chord-arp.wav --debug preset-huc-lfo-chord-arp.json
)
add_test(NAME chipper_render_preset_huc_lfo_noise_hit_smoke
    COMMAND chipper_render --preset huc-lfo-noise-hit --rate 48000 --seconds 0.05 --note 48 --out preset-huc-lfo-noise-hit.wav --debug preset-huc-lfo-noise-hit.json
)
add_test(NAME chipper_render_preset_paula_dac_noise_kit_smoke
    COMMAND chipper_render --preset paula-dac-noise-kit --rate 48000 --seconds 0.05 --note 48 --out preset-paula-dac-noise-kit.wav --debug preset-paula-dac-noise-kit.json
)
add_test(NAME chipper_render_preset_paula_dac_noise_burst_smoke
    COMMAND chipper_render --preset paula-dac-noise-burst --rate 48000 --seconds 0.05 --note 48 --out preset-paula-dac-noise-burst.wav --debug preset-paula-dac-noise-burst.json
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
add_test(NAME chipper_render_preset_pokey_distortion_lead_smoke
    COMMAND chipper_render --preset pokey-distortion-lead --rate 48000 --seconds 0.04 --note 69 --out preset-pokey-distortion-lead-smoke.wav --debug preset-pokey-distortion-lead-smoke.json
)
add_test(NAME chipper_render_preset_pokey_distortion_filter_perc_smoke
    COMMAND chipper_render --preset pokey-distortion-filter-perc --rate 48000 --seconds 0.04 --note 60 --out preset-pokey-distortion-filter-perc-smoke.wav --debug preset-pokey-distortion-filter-perc-smoke.json
)
add_test(NAME chipper_render_preset_scc_noise_click_drum_smoke
    COMMAND chipper_render --preset scc-noise-click-drum --rate 48000 --seconds 0.05 --note 48 --out preset-scc-noise-click-drum.wav --debug preset-scc-noise-click-drum.json
)
add_test(NAME chipper_render_preset_scc_noise_burst_smoke
    COMMAND chipper_render --preset scc-noise-burst --rate 48000 --seconds 0.05 --note 48 --out preset-scc-noise-burst.wav --debug preset-scc-noise-burst.json
)
add_test(NAME chipper_render_preset_namco_tracker_lead_smoke
    COMMAND chipper_render --preset namco-tracker-lead --rate 48000 --seconds 0.05 --note 69 --out preset-namco-tracker-lead.wav --debug preset-namco-tracker-lead.json
)
add_test(NAME chipper_render_preset_namco_tracker_bass_smoke
    COMMAND chipper_render --preset namco-tracker-bass --rate 48000 --seconds 0.05 --note 48 --out preset-namco-tracker-bass.wav --debug preset-namco-tracker-bass.json
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
add_test(NAME chipper_render_dmg_pulse2_duty_override_smoke
    COMMAND chipper_render --chip dmg --accuracy authentic --macro manual --control1 0.0 --pulse2-duty 75 --clock 4194304 --rate 48000 --seconds 0.01 --note 69 --out dmg-pulse2-duty-smoke.wav --debug dmg-pulse2-duty-smoke.json
)
add_test(NAME chipper_render_dmg_pulse2_duty_follow_smoke
    COMMAND chipper_render --chip dmg --accuracy authentic --macro manual --control1 0.0 --pulse2-duty follow --clock 4194304 --rate 48000 --seconds 0.01 --note 69 --out dmg-pulse2-duty-follow-smoke.wav --debug dmg-pulse2-duty-follow-smoke.json
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
    set_tests_properties(chipper_generate_nes_dmc_fixture PROPERTIES FIXTURES_SETUP nes_dmc_fixture)
    add_test(NAME chipper_render_nes_dmc_sample_smoke
        COMMAND chipper_render --chip nes --accuracy authentic --macro manual --source1 0 --source2 0 --source3 0 --source4 1 --nes-dmc-direct-level 0.5 --nes-dmc-sample ${CMAKE_CURRENT_BINARY_DIR}/generated-nes-dmc-sample.dmc --clock 1789773 --rate 48000 --seconds 0.01 --out nes-dmc-sample-smoke.wav --debug nes-dmc-sample-smoke.json
    )
    set_tests_properties(chipper_render_nes_dmc_sample_smoke PROPERTIES DEPENDS chipper_generate_nes_dmc_fixture FIXTURES_REQUIRED nes_dmc_fixture)
    add_test(NAME chipper_render_nes_dmc_rate0_sample_smoke
        COMMAND chipper_render --chip nes --accuracy authentic --macro manual --source1 0 --source2 0 --source3 0 --source4 1 --nes-dmc-direct-level 0.5 --nes-dmc-rate 0 --nes-dmc-sample ${CMAKE_CURRENT_BINARY_DIR}/generated-nes-dmc-sample.dmc --clock 1789773 --rate 48000 --seconds 0.004 --out nes-dmc-rate0-sample-smoke.wav --debug nes-dmc-rate0-sample-smoke.json
    )
    set_tests_properties(chipper_render_nes_dmc_rate0_sample_smoke PROPERTIES DEPENDS chipper_generate_nes_dmc_fixture FIXTURES_REQUIRED nes_dmc_fixture)
    add_test(NAME chipper_render_nes_dmc_rate15_sample_smoke
        COMMAND chipper_render --chip nes --accuracy authentic --macro manual --source1 0 --source2 0 --source3 0 --source4 1 --nes-dmc-direct-level 0.5 --nes-dmc-rate 15 --nes-dmc-sample ${CMAKE_CURRENT_BINARY_DIR}/generated-nes-dmc-sample.dmc --clock 1789773 --rate 48000 --seconds 0.004 --out nes-dmc-rate15-sample-smoke.wav --debug nes-dmc-rate15-sample-smoke.json
    )
    set_tests_properties(chipper_render_nes_dmc_rate15_sample_smoke PROPERTIES DEPENDS chipper_generate_nes_dmc_fixture FIXTURES_REQUIRED nes_dmc_fixture)
    add_test(NAME chipper_render_nes_dmc_loop_sample_smoke
        COMMAND chipper_render --chip nes --accuracy authentic --macro manual --source1 0 --source2 0 --source3 0 --source4 1 --nes-dmc-direct-level 0.5 --nes-dmc-rate 15 --nes-dmc-loop 1 --nes-dmc-sample ${CMAKE_CURRENT_BINARY_DIR}/generated-nes-dmc-sample.dmc --clock 1789773 --rate 48000 --seconds 0.004 --out nes-dmc-loop-sample-smoke.wav --debug nes-dmc-loop-sample-smoke.json
    )
    set_tests_properties(chipper_render_nes_dmc_loop_sample_smoke PROPERTIES DEPENDS chipper_generate_nes_dmc_fixture FIXTURES_REQUIRED nes_dmc_fixture)
    add_test(NAME chipper_render_nes_dmc_only_sample_smoke
        COMMAND chipper_render --chip nes --accuracy authentic --macro drum --source1 1 --source2 1 --source3 1 --source4 1 --nes-dmc-direct-level 0.5 --nes-dmc-rate 15 --nes-dmc-only 1 --nes-dmc-sample ${CMAKE_CURRENT_BINARY_DIR}/generated-nes-dmc-sample.dmc --clock 1789773 --rate 48000 --seconds 0.004 --out nes-dmc-only-sample-smoke.wav --debug nes-dmc-only-sample-smoke.json
    )
    set_tests_properties(chipper_render_nes_dmc_only_sample_smoke PROPERTIES DEPENDS chipper_generate_nes_dmc_fixture FIXTURES_REQUIRED nes_dmc_fixture)
    add_test(NAME chipper_render_nes_dmc_chip_poly_sample_smoke
        COMMAND chipper_render --chip nes --accuracy authentic --macro manual --play-mode chip-poly --source1 0 --source2 0 --source3 0 --source4 1 --nes-dmc-direct-level 0.5 --nes-dmc-sample ${CMAKE_CURRENT_BINARY_DIR}/generated-nes-dmc-sample.dmc --clock 1789773 --rate 48000 --seconds 0.01 --note 60 --out nes-dmc-chip-poly-sample-smoke.wav --debug nes-dmc-chip-poly-sample-smoke.json
    )
    set_tests_properties(chipper_render_nes_dmc_chip_poly_sample_smoke PROPERTIES DEPENDS chipper_generate_nes_dmc_fixture FIXTURES_REQUIRED nes_dmc_fixture)
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
add_test(NAME chipper_render_pokey_smoke
    COMMAND chipper_render --chip pokey --accuracy authentic --macro lead --clock 1789790 --rate 48000 --seconds 0.05 --note 60 --out pokey-smoke.wav --debug pokey-smoke.json
)
add_test(NAME chipper_render_pokey_poly17_smoke
    COMMAND chipper_render --chip pokey --accuracy authentic --macro drum --wave-shape poly17 --source1 0 --source2 0 --source3 1 --source4 1 --clock 1789790 --rate 48000 --seconds 0.05 --note 48 --out pokey-poly17-smoke.wav --debug pokey-poly17-smoke.json
)
add_test(NAME chipper_render_pokey_chip_poly_smoke
    COMMAND chipper_render --chip pokey --accuracy authentic --macro manual --play-mode chip-poly --wave-shape pure --clock 1789790 --rate 48000 --seconds 0.05 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/pokey-chip-poly-events.txt --out pokey-chip-poly-smoke.wav --debug pokey-chip-poly-smoke.json
)
add_test(NAME chipper_render_pokey_audctl_pair_smoke
    COMMAND chipper_render --chip pokey --accuracy authentic --macro bass --pokey-audctl 1+2 --pokey-filter off --wave-shape pure --clock 1789790 --rate 48000 --seconds 0.05 --note 60 --out pokey-audctl-pair-smoke.wav --debug pokey-audctl-pair-smoke.json
)
add_test(NAME chipper_render_pokey_filter_smoke
    COMMAND chipper_render --chip pokey --accuracy authentic --macro bass --pokey-filter 1by3 --wave-shape pure --clock 1789790 --rate 48000 --seconds 0.08 --note 48 --out pokey-filter-smoke.wav --debug pokey-filter-smoke.json
)
add_test(NAME chipper_render_pokey_filter_both_smoke
    COMMAND chipper_render --chip pokey --accuracy authentic --macro laser --pokey-filter both --wave-shape poly5 --clock 1789790 --rate 48000 --seconds 0.08 --note 60 --out pokey-filter-both-smoke.wav --debug pokey-filter-both-smoke.json
)
add_test(NAME chipper_render_pokey_paired_chip_poly_smoke
    COMMAND chipper_render --chip pokey --accuracy authentic --macro manual --play-mode chip-poly --pokey-audctl both --wave-shape pure --clock 1789790 --rate 48000 --seconds 0.05 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/pokey-paired-chip-poly-events.txt --out pokey-paired-chip-poly-smoke.wav --debug pokey-paired-chip-poly-smoke.json
)
add_test(NAME chipper_render_paula_smoke
    COMMAND chipper_render --chip paula --accuracy authentic --macro lead --wave-shape 1 --clock 3546895 --rate 48000 --seconds 0.05 --note 64 --out paula-smoke.wav --debug paula-smoke.json
)
add_test(NAME chipper_render_paula_noise_smoke
    COMMAND chipper_render --chip paula --accuracy authentic --macro drum --wave-shape noise --source1 0 --source2 0 --source3 1 --source4 1 --clock 3546895 --rate 48000 --seconds 0.05 --note 48 --out paula-noise-smoke.wav --debug paula-noise-smoke.json
)
add_test(NAME chipper_render_paula_chip_poly_smoke
    COMMAND chipper_render --chip paula --accuracy authentic --macro manual --play-mode chip-poly --wave-shape 2 --clock 3546895 --rate 48000 --seconds 0.05 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/paula-chip-poly-events.txt --out paula-chip-poly-smoke.wav --debug paula-chip-poly-smoke.json
)
add_test(NAME chipper_render_paula_channel_shapes_smoke
    COMMAND chipper_render --chip paula --accuracy authentic --macro manual --play-mode chip-poly --paula-shape1 ramp --paula-shape2 tri --paula-shape3 sine --paula-shape4 noise --clock 3546895 --rate 48000 --seconds 0.05 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/paula-chip-poly-events.txt --out paula-channel-shapes-smoke.wav --debug paula-channel-shapes-smoke.json
)
add_test(NAME chipper_render_paula_led_filter_smoke
    COMMAND chipper_render --chip paula --accuracy authentic --macro bass --paula-output-filter led --wave-shape tri --clock 3546895 --rate 48000 --seconds 0.08 --note 48 --out paula-led-filter-smoke.wav --debug paula-led-filter-smoke.json
)
add_test(NAME chipper_render_paula_raw_left_smoke
    COMMAND chipper_render --chip paula --accuracy authentic --macro lead --paula-output-filter raw --wave-shape ramp --source1 1 --source2 0 --source3 0 --source4 0 --stereo-spread 1.0 --clock 3546895 --rate 48000 --seconds 0.08 --note 60 --out paula-raw-left-smoke.wav --debug paula-raw-left-smoke.json
)
if (Python3_EXECUTABLE)
    add_test(NAME chipper_generate_paula_8svx_fixture
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/write_binary_fixture.py ${CMAKE_CURRENT_BINARY_DIR}/generated-paula-sample.8svx "46 4f 52 4d 00 00 00 38 38 53 56 58 56 48 44 52 00 00 00 14 00 00 00 10 00 00 00 00 00 00 00 00 20 ab 01 00 00 01 00 00 42 4f 44 59 00 00 00 10 80 90 a0 b0 c0 d0 e0 f0 00 10 20 30 40 50 60 70"
    )
    set_tests_properties(chipper_generate_paula_8svx_fixture PROPERTIES FIXTURES_SETUP paula_8svx_fixture)
    add_test(NAME chipper_render_paula_8svx_sample_smoke
        COMMAND chipper_render --chip paula --accuracy authentic --macro lead --paula-sample ${CMAKE_CURRENT_BINARY_DIR}/generated-paula-sample.8svx --source1 1 --source2 0 --source3 0 --source4 0 --clock 3546895 --rate 48000 --seconds 0.06 --note 60 --out paula-8svx-sample-smoke.wav --debug paula-8svx-sample-smoke.json
    )
    set_tests_properties(chipper_render_paula_8svx_sample_smoke PROPERTIES DEPENDS chipper_generate_paula_8svx_fixture FIXTURES_REQUIRED paula_8svx_fixture)
    add_test(NAME chipper_generate_paula_wav_smpl_fixture
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/write_binary_fixture.py ${CMAKE_CURRENT_BINARY_DIR}/generated-paula-sample-loop.wav "52 49 46 46 78 00 00 00 57 41 56 45 66 6d 74 20 10 00 00 00 01 00 01 00 40 1f 00 00 40 1f 00 00 01 00 08 00 64 61 74 61 10 00 00 00 80 90 a0 b0 c0 d0 e0 f0 70 60 50 40 30 20 10 00 73 6d 70 6c 3c 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 3c 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00 04 00 00 00 0b 00 00 00 00 00 00 00 00 00 00 00"
    )
    set_tests_properties(chipper_generate_paula_wav_smpl_fixture PROPERTIES FIXTURES_SETUP paula_wav_smpl_fixture)
    add_test(NAME chipper_render_paula_wav_smpl_sample_smoke
        COMMAND chipper_render --chip paula --accuracy authentic --macro lead --paula-sample ${CMAKE_CURRENT_BINARY_DIR}/generated-paula-sample-loop.wav --source1 1 --source2 0 --source3 0 --source4 0 --clock 3546895 --rate 48000 --seconds 0.06 --note 60 --out paula-wav-smpl-sample-smoke.wav --debug paula-wav-smpl-sample-smoke.json
    )
    set_tests_properties(chipper_render_paula_wav_smpl_sample_smoke PROPERTIES DEPENDS chipper_generate_paula_wav_smpl_fixture FIXTURES_REQUIRED paula_wav_smpl_fixture)
    add_test(NAME chipper_render_paula_sample_slots_smoke
        COMMAND chipper_render --chip paula --accuracy authentic --macro manual --play-mode chip-poly --paula-sample ${CMAKE_CURRENT_BINARY_DIR}/generated-paula-sample.8svx --paula-sample ${CMAKE_CURRENT_BINARY_DIR}/generated-paula-sample-loop.wav --paula-sample-slot1 2 --paula-sample-slot2 1 --paula-sample-slot3 2 --paula-sample-slot4 1 --source1 1 --source2 1 --source3 1 --source4 1 --clock 3546895 --rate 48000 --seconds 0.06 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/paula-chip-poly-events.txt --out paula-sample-slots-smoke.wav --debug paula-sample-slots-smoke.json
    )
    set_tests_properties(chipper_render_paula_sample_slots_smoke PROPERTIES DEPENDS "chipper_generate_paula_8svx_fixture;chipper_generate_paula_wav_smpl_fixture" FIXTURES_REQUIRED "paula_8svx_fixture;paula_wav_smpl_fixture")
endif()
add_test(NAME chipper_render_huc6280_smoke
    COMMAND chipper_render --chip huc6280 --accuracy authentic --macro lead --wave-shape 1 --clock 3579545 --rate 48000 --seconds 0.05 --note 64 --out huc6280-smoke.wav --debug huc6280-smoke.json
)
add_test(NAME chipper_render_huc6280_noise_smoke
    COMMAND chipper_render --chip huc6280 --accuracy authentic --macro drum --wave-shape noise --source1 0 --source2 0 --source3 1 --source4 1 --clock 3579545 --rate 48000 --seconds 0.05 --note 48 --out huc6280-noise-smoke.wav --debug huc6280-noise-smoke.json
)
add_test(NAME chipper_render_huc6280_chip_poly_smoke
    COMMAND chipper_render --chip huc6280 --accuracy authentic --macro manual --play-mode chip-poly --wave-shape 2 --clock 3579545 --rate 48000 --seconds 0.05 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/huc6280-chip-poly-events.txt --out huc6280-chip-poly-smoke.wav --debug huc6280-chip-poly-smoke.json
)
add_test(NAME chipper_render_huc6280_lfo_smoke
    COMMAND chipper_render --chip huc6280 --accuracy authentic --macro lead --huc-lfo deep --control2 0.60 --control3 0.70 --source1 1 --source2 1 --source3 0 --source4 0 --source5 0 --source6 0 --clock 3579545 --rate 48000 --seconds 0.08 --note 64 --out huc6280-lfo-smoke.wav --debug huc6280-lfo-smoke.json
)
add_test(NAME chipper_render_huc6280_per_voice_waves_smoke
    COMMAND chipper_render --chip huc6280 --accuracy authentic --macro manual --play-mode chip-poly --huc-wave1 ramp --huc-wave2 tri --huc-wave3 square --huc-wave4 noise --huc-wave5 square --huc-wave6 tri --clock 3579545 --rate 48000 --seconds 0.05 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/huc6280-chip-poly-events.txt --out huc6280-per-voice-waves-smoke.wav --debug huc6280-per-voice-waves-smoke.json
)
add_test(NAME chipper_render_scc_smoke
    COMMAND chipper_render --chip scc --accuracy authentic --macro lead --wave-shape 1 --clock 3579545 --rate 48000 --seconds 0.05 --note 64 --out scc-smoke.wav --debug scc-smoke.json
)
add_test(NAME chipper_render_scc_steps_smoke
    COMMAND chipper_render --chip scc --accuracy authentic --macro drum --wave-shape noise --source1 0 --source2 0 --source3 1 --source4 1 --clock 3579545 --rate 48000 --seconds 0.05 --note 48 --out scc-steps-smoke.wav --debug scc-steps-smoke.json
)
add_test(NAME chipper_render_scc_chip_poly_smoke
    COMMAND chipper_render --chip scc --accuracy authentic --macro manual --play-mode chip-poly --wave-shape 2 --clock 3579545 --rate 48000 --seconds 0.05 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/scc-five-chip-poly-events.txt --out scc-chip-poly-smoke.wav --debug scc-chip-poly-smoke.json
)
add_test(NAME chipper_render_scc_per_channel_waves_smoke
    COMMAND chipper_render --chip scc --accuracy authentic --macro manual --play-mode chip-poly --scc-wave1 ramp --scc-wave2 tri --scc-wave3 pulse --scc-wave4 steps --scc-wave5 pulse --clock 3579545 --rate 48000 --seconds 0.05 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/scc-five-chip-poly-events.txt --out scc-per-channel-waves-smoke.wav --debug scc-per-channel-waves-smoke.json
)
add_test(NAME chipper_render_scc_register_trace_smoke
    COMMAND chipper_render --chip scc --accuracy authentic --macro manual --clock 3579545 --rate 48000 --seconds 0.05 --note 60 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/scc-register-trace-events.txt --out scc-register-trace-smoke.wav --debug scc-register-trace-smoke.json
)
add_test(NAME chipper_render_namco_wsg_smoke
    COMMAND chipper_render --chip namco --accuracy authentic --macro lead --wave-shape 1 --clock 96000 --rate 48000 --seconds 0.05 --note 64 --out namco-wsg-smoke.wav --debug namco-wsg-smoke.json
)
add_test(NAME chipper_render_namco_wsg_steps_smoke
    COMMAND chipper_render --chip namco --accuracy authentic --macro drum --wave-shape noise --source1 0 --source2 0 --source3 1 --source4 1 --clock 96000 --rate 48000 --seconds 0.05 --note 48 --out namco-wsg-steps-smoke.wav --debug namco-wsg-steps-smoke.json
)
add_test(NAME chipper_render_namco_wsg_chip_poly_smoke
    COMMAND chipper_render --chip namco --accuracy authentic --macro manual --play-mode chip-poly --wave-shape 2 --clock 96000 --rate 48000 --seconds 0.10 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/namco-wsg-chip-poly-events.txt --out namco-wsg-chip-poly-smoke.wav --debug namco-wsg-chip-poly-smoke.json
)
add_test(NAME chipper_render_namco_wsg_per_lane_waves_smoke
    COMMAND chipper_render --chip namco --accuracy authentic --macro manual --play-mode chip-poly --namco-wave1 ramp --namco-wave2 tri --namco-wave3 pulse --namco-wave4 steps --namco-wave5 pulse --namco-wave6 tri --namco-wave7 steps --namco-wave8 steps --clock 96000 --rate 48000 --seconds 0.10 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/namco-wsg-chip-poly-events.txt --out namco-wsg-per-lane-waves-smoke.wav --debug namco-wsg-per-lane-waves-smoke.json
)
add_test(NAME chipper_render_ym2612_smoke
    COMMAND chipper_render --chip ym2612 --accuracy authentic --macro lead --wave-shape 5 --clock 7670454 --rate 48000 --seconds 0.08 --note 69 --out ym2612-smoke.wav --debug ym2612-smoke.json
)
add_test(NAME chipper_render_ym2612_feedback7_smoke
    COMMAND chipper_render --chip ym2612 --accuracy authentic --macro manual --wave-shape 5 --fm-feedback 7 --clock 7670454 --rate 48000 --seconds 0.08 --note 69 --out ym2612-feedback7-smoke.wav --debug ym2612-feedback7-smoke.json
)
add_test(NAME chipper_render_ym2612_operator_levels_smoke
    COMMAND chipper_render --chip ym2612 --accuracy authentic --macro lead --wave-shape 5 --control3 0.5 --control4 0.8 --fm-op1-level 0 --fm-op4-level 1 --fm-op1-multiplier 0.5 --fm-op4-multiplier 15 --fm-op1-attack-rate 0 --fm-op4-attack-rate 31 --fm-op2-decay-rate 0 --fm-op4-decay-rate 31 --fm-op2-sustain-rate 0 --fm-op4-sustain-rate 31 --fm-op2-release-rate 0 --fm-op4-release-rate 15 --clock 7670454 --rate 48000 --seconds 0.08 --note 69 --out ym2612-operator-levels-smoke.wav --debug ym2612-operator-levels-smoke.json
)
add_test(NAME chipper_render_ym2612_operator_detune_smoke
    COMMAND chipper_render --chip ym2612 --accuracy authentic --macro lead --wave-shape 5 --control3 0.9 --clock 7670454 --rate 48000 --seconds 0.08 --note 69 --out ym2612-operator-detune-smoke.wav --debug ym2612-operator-detune-smoke.json
)
add_test(NAME chipper_render_ym2612_lfo_depth_smoke
    COMMAND chipper_render --chip ym2612 --accuracy authentic --macro lead --wave-shape 5 --opn2-lfo-depth 0.65 --clock 7670454 --rate 48000 --seconds 0.12 --note 69 --out ym2612-lfo-depth-smoke.wav --debug ym2612-lfo-depth-smoke.json
)
add_test(NAME chipper_render_ym2612_chip_poly_smoke
    COMMAND chipper_render --chip ym2612 --accuracy authentic --macro manual --play-mode chip-poly --wave-shape 8 --clock 7670454 --rate 48000 --seconds 0.05 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/scc-chip-poly-events.txt --out ym2612-chip-poly-smoke.wav --debug ym2612-chip-poly-smoke.json
)
add_test(NAME chipper_render_ym2612_six_channel_chip_poly_smoke
    COMMAND chipper_render --chip ym2612 --accuracy authentic --macro manual --play-mode chip-poly --wave-shape 8 --clock 7670454 --rate 48000 --seconds 0.05 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/ym2612-six-channel-events.txt --out ym2612-six-channel-chip-poly-smoke.wav --debug ym2612-six-channel-chip-poly-smoke.json
)
add_test(NAME chipper_render_ym2612_sources_off_smoke
    COMMAND chipper_render --chip ym2612 --accuracy authentic --macro manual --source1 0 --source2 0 --source3 0 --source4 0 --clock 7670454 --rate 48000 --seconds 0.04 --note 69 --out ym2612-sources-off-smoke.wav --debug ym2612-sources-off-smoke.json
)
add_test(NAME chipper_render_ym2612_pan_left_smoke
    COMMAND chipper_render --chip ym2612 --accuracy authentic --macro lead --opn2-pan left --clock 7670454 --rate 48000 --seconds 0.08 --note 69 --out ym2612-pan-left-smoke.wav --debug ym2612-pan-left-smoke.json
)
add_test(NAME chipper_render_ym2612_pan_right_smoke
    COMMAND chipper_render --chip ym2612 --accuracy authentic --macro lead --opn2-pan right --clock 7670454 --rate 48000 --seconds 0.08 --note 69 --out ym2612-pan-right-smoke.wav --debug ym2612-pan-right-smoke.json
)
add_test(NAME chipper_render_ym2612_env_pad_smoke
    COMMAND chipper_render --chip ym2612 --accuracy authentic --macro lead --opn2-envelope pad --clock 7670454 --rate 48000 --seconds 0.08 --note 69 --out ym2612-env-pad-smoke.wav --debug ym2612-env-pad-smoke.json
)
add_test(NAME chipper_render_ym2612_dac_smoke
    COMMAND chipper_render --chip ym2612 --accuracy authentic --macro drum --opn2-dac dac --opn2-pan both --clock 7670454 --rate 48000 --seconds 0.03 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/ym2612-dac-events.txt --out ym2612-dac-smoke.wav --debug ym2612-dac-smoke.json
)
add_test(NAME chipper_render_ym2612_dac_user_sample_smoke
    COMMAND chipper_render --chip ym2612 --accuracy authentic --macro drum --opn2-dac dac --opn2-dac-hex 8080c0ff80400020 --opn2-pan both --clock 7670454 --rate 48000 --seconds 0.03 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/ym2612-dac-events.txt --out ym2612-dac-user-sample-smoke.wav --debug ym2612-dac-user-sample-smoke.json
)
if (Python3_EXECUTABLE)
    add_test(NAME chipper_generate_opn2_dac_wav_fixture
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/write_binary_fixture.py ${CMAKE_CURRENT_BINARY_DIR}/generated-opn2-dac-sample.wav "52 49 46 46 34 00 00 00 57 41 56 45 66 6d 74 20 10 00 00 00 01 00 01 00 40 1f 00 00 40 1f 00 00 01 00 08 00 64 61 74 61 10 00 00 00 80 80 80 c0 c0 ff 80 40 40 00 20 20 f0 10 10 80"
    )
    set_tests_properties(chipper_generate_opn2_dac_wav_fixture PROPERTIES FIXTURES_SETUP opn2_dac_wav_fixture)
    add_test(NAME chipper_render_ym2612_dac_wav_sample_smoke
        COMMAND chipper_render --chip ym2612 --accuracy authentic --macro drum --opn2-dac dac --opn2-dac-sample ${CMAKE_CURRENT_BINARY_DIR}/generated-opn2-dac-sample.wav --opn2-dac-root 48 --opn2-dac-trim-start 2 --opn2-dac-trim-end 14 --opn2-dac-tail hold --source1 0 --source2 0 --source3 0 --source4 0 --source5 0 --source6 1 --opn2-pan both --clock 7670454 --rate 48000 --seconds 0.03 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/ym2612-dac-events.txt --out ym2612-dac-wav-sample-smoke.wav --debug ym2612-dac-wav-sample-smoke.json
    )
    set_tests_properties(chipper_render_ym2612_dac_wav_sample_smoke PROPERTIES DEPENDS chipper_generate_opn2_dac_wav_fixture FIXTURES_REQUIRED opn2_dac_wav_fixture)
    add_test(NAME chipper_generate_opn2_dac_aiff_fixture
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/write_binary_fixture.py ${CMAKE_CURRENT_BINARY_DIR}/generated-opn2-dac-sample.aiff "46 4f 52 4d 00 00 00 3e 41 49 46 46 43 4f 4d 4d 00 00 00 12 00 01 00 00 00 10 00 08 40 0b fa 00 00 00 00 00 00 00 53 53 4e 44 00 00 00 18 00 00 00 00 00 00 00 00 00 00 00 40 40 7f 00 c0 c0 80 a0 a0 70 90 90 00"
    )
    set_tests_properties(chipper_generate_opn2_dac_aiff_fixture PROPERTIES FIXTURES_SETUP opn2_dac_aiff_fixture)
    add_test(NAME chipper_render_ym2612_dac_aiff_sample_smoke
        COMMAND chipper_render --chip ym2612 --accuracy authentic --macro drum --opn2-dac dac --opn2-dac-sample ${CMAKE_CURRENT_BINARY_DIR}/generated-opn2-dac-sample.aiff --opn2-dac-root 48 --opn2-dac-trim-start 2 --opn2-dac-trim-end 14 --opn2-dac-tail hold --source1 0 --source2 0 --source3 0 --source4 0 --source5 0 --source6 1 --opn2-pan both --clock 7670454 --rate 48000 --seconds 0.03 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/ym2612-dac-events.txt --out ym2612-dac-aiff-sample-smoke.wav --debug ym2612-dac-aiff-sample-smoke.json
    )
    set_tests_properties(chipper_render_ym2612_dac_aiff_sample_smoke PROPERTIES DEPENDS chipper_generate_opn2_dac_aiff_fixture FIXTURES_REQUIRED opn2_dac_aiff_fixture)
endif()
add_test(NAME chipper_render_preset_opn2_dac_kick_smoke
    COMMAND chipper_render --preset opn2-dac-kick --rate 48000 --seconds 0.03 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/ym2612-dac-events.txt --out preset-opn2-dac-kick.wav --debug preset-opn2-dac-kick.json
)
add_test(NAME chipper_render_preset_opn2_dac_chord_hit_smoke
    COMMAND chipper_render --preset opn2-dac-chord-hit --rate 48000 --seconds 0.03 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/ym2612-dac-events.txt --out preset-opn2-dac-chord-hit.wav --debug preset-opn2-dac-chord-hit.json
)
add_test(NAME chipper_render_preset_opn2_dac_snare_smoke
    COMMAND chipper_render --preset opn2-dac-snare --rate 48000 --seconds 0.03 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/ym2612-dac-events.txt --out preset-opn2-dac-snare.wav --debug preset-opn2-dac-snare.json
)
add_test(NAME chipper_render_preset_opn2_envelope_bell_smoke
    COMMAND chipper_render --preset opn2-envelope-bell --rate 48000 --seconds 0.08 --note 76 --out preset-opn2-envelope-bell.wav --debug preset-opn2-envelope-bell.json
)
add_test(NAME chipper_render_preset_opn2_chord_pad_smoke
    COMMAND chipper_render --preset opn2-chord-pad --rate 48000 --seconds 0.12 --note 60 --out preset-opn2-chord-pad.wav --debug preset-opn2-chord-pad.json
)
add_test(NAME chipper_render_preset_opn2_crystal_pluck_smoke
    COMMAND chipper_render --preset opn2-crystal-pluck --rate 48000 --seconds 0.08 --note 76 --out preset-opn2-crystal-pluck.wav --debug preset-opn2-crystal-pluck.json
)
add_test(NAME chipper_render_preset_opn2_wide_ep_pad_smoke
    COMMAND chipper_render --preset opn2-wide-ep-pad --rate 48000 --seconds 0.12 --note 60 --out preset-opn2-wide-ep-pad.wav --debug preset-opn2-wide-ep-pad.json
)
add_test(NAME chipper_render_preset_opn2_feedback_bass_held_smoke
    COMMAND chipper_render --preset opn2-feedback-bass --rate 48000 --seconds 0.50 --note 48 --out preset-opn2-feedback-bass-held.wav --debug preset-opn2-feedback-bass-held.json
)
add_test(NAME chipper_render_ym2203_smoke
    COMMAND chipper_render --chip ym2203 --accuracy authentic --macro lead --wave-shape 5 --opn-feedback 4 --clock 3993600 --rate 48000 --seconds 0.08 --note 69 --out ym2203-smoke.wav --debug ym2203-smoke.json
)
add_test(NAME chipper_render_ym2203_feedback0_pitch_smoke
    COMMAND chipper_render --chip ym2203 --accuracy authentic --macro manual --wave-shape 5 --opn-feedback 0 --clock 3993600 --rate 48000 --seconds 0.04 --note 69 --out ym2203-feedback0-pitch-smoke.wav --debug ym2203-feedback0-pitch-smoke.json
)
add_test(NAME chipper_render_ym2203_feedback7_pitch_smoke
    COMMAND chipper_render --chip ym2203 --accuracy authentic --macro manual --wave-shape 5 --opn-feedback 7 --clock 3993600 --rate 48000 --seconds 0.04 --note 69 --out ym2203-feedback7-pitch-smoke.wav --debug ym2203-feedback7-pitch-smoke.json
)
add_test(NAME chipper_render_ym2203_chip_poly_smoke
    COMMAND chipper_render --chip ym2203 --accuracy authentic --macro manual --play-mode chip-poly --wave-shape 8 --clock 3993600 --rate 48000 --seconds 0.08 --note 1 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/ym2203-six-channel-events.txt --out ym2203-chip-poly-smoke.wav --debug ym2203-chip-poly-smoke.json
)
add_test(NAME chipper_render_ym2203_sources_off_smoke
    COMMAND chipper_render --chip ym2203 --accuracy authentic --macro manual --source1 0 --source2 0 --source3 0 --source4 0 --source5 0 --source6 0 --clock 3993600 --rate 48000 --seconds 0.04 --note 69 --out ym2203-sources-off-smoke.wav --debug ym2203-sources-off-smoke.json
)
add_test(NAME chipper_render_ym2203_ssg_smoke
    COMMAND chipper_render --chip ym2203 --accuracy authentic --macro coin --source1 0 --source2 0 --source3 0 --source4 1 --source5 1 --source6 1 --clock 3993600 --rate 48000 --seconds 0.08 --note 72 --out ym2203-ssg-smoke.wav --debug ym2203-ssg-smoke.json
)
add_test(NAME chipper_render_ym2203_ssg_noise_env_smoke
    COMMAND chipper_render --chip ym2203 --accuracy authentic --macro manual --source1 0 --source2 0 --source3 0 --source4 1 --source5 1 --source6 1 --control3 0.2 --envelope-decay 1.0 --opn-ssg-envelope tri --opn-ssg-a-mix noise --opn-ssg-b-mix both --opn-ssg-c-mix off --clock 3993600 --rate 48000 --seconds 0.08 --note 72 --out ym2203-ssg-noise-env-smoke.wav --debug ym2203-ssg-noise-env-smoke.json
)
add_test(NAME chipper_render_ym2608_smoke
    COMMAND chipper_render --chip ym2608 --accuracy authentic --macro lead --wave-shape 5 --fm-feedback 4 --clock 7987200 --rate 48000 --seconds 0.08 --note 69 --out ym2608-smoke.wav --debug ym2608-smoke.json
)
add_test(NAME chipper_render_ym2608_chip_poly_smoke
    COMMAND chipper_render --chip ym2608 --accuracy authentic --macro manual --play-mode chip-poly --wave-shape 8 --clock 7987200 --rate 48000 --seconds 0.08 --note 1 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/ym2608-nine-channel-events.txt --out ym2608-chip-poly-smoke.wav --debug ym2608-chip-poly-smoke.json
)
add_test(NAME chipper_render_ym2608_sources_off_smoke
    COMMAND chipper_render --chip ym2608 --accuracy authentic --macro manual --source1 0 --source2 0 --source3 0 --source4 0 --source5 0 --source6 0 --source7 0 --source8 0 --source9 0 --clock 7987200 --rate 48000 --seconds 0.04 --note 69 --out ym2608-sources-off-smoke.wav --debug ym2608-sources-off-smoke.json
)
add_test(NAME chipper_render_ym2608_ssg_smoke
    COMMAND chipper_render --chip ym2608 --accuracy authentic --macro coin --source1 0 --source2 0 --source3 0 --source4 0 --source5 0 --source6 0 --source7 1 --source8 1 --source9 1 --clock 7987200 --rate 48000 --seconds 0.08 --note 72 --out ym2608-ssg-smoke.wav --debug ym2608-ssg-smoke.json
)
add_test(NAME chipper_render_ym2608_rhythm_smoke
    COMMAND chipper_render --chip ym2608 --accuracy authentic --macro drum --source1 1 --source2 1 --source3 1 --source4 1 --source5 1 --source6 1 --source7 0 --source8 0 --source9 0 --clock 7987200 --rate 48000 --seconds 0.08 --note 60 --out ym2608-rhythm-smoke.wav --debug ym2608-rhythm-smoke.json
)
add_test(NAME chipper_render_ym2608_user_rhythm_rom_smoke
    COMMAND chipper_render --chip ym2608 --accuracy authentic --macro drum --source1 1 --source2 1 --source3 1 --source4 1 --source5 1 --source6 1 --source7 0 --source8 0 --source9 0 --opna-rhythm-rom-hex 000102030405060708090a0b0c0d0e0f --clock 7987200 --rate 48000 --seconds 0.08 --note 60 --out ym2608-user-rhythm-rom-smoke.wav --debug ym2608-user-rhythm-rom-smoke.json
)
add_test(NAME chipper_render_ym2608_adpcm_b_sample_smoke
    COMMAND chipper_render --chip ym2608 --accuracy authentic --macro drum --source1 1 --source2 1 --source3 1 --source4 1 --source5 1 --source6 1 --source7 0 --source8 0 --source9 0 --opna-adpcm-b-hex 11111111999999991111111199999999 --clock 7987200 --rate 48000 --seconds 0.08 --note 60 --out ym2608-adpcm-b-sample-smoke.wav --debug ym2608-adpcm-b-sample-smoke.json
)
add_test(NAME chipper_render_ym2608_feedback0_pitch_smoke
    COMMAND chipper_render --chip ym2608 --accuracy authentic --macro manual --fm-feedback 0 --source7 0 --source8 0 --source9 0 --clock 7987200 --rate 48000 --seconds 0.04 --note 69 --out ym2608-feedback0-pitch-smoke.wav --debug ym2608-feedback0-pitch-smoke.json
)
add_test(NAME chipper_render_ym2608_feedback7_pitch_smoke
    COMMAND chipper_render --chip ym2608 --accuracy authentic --macro manual --fm-feedback 7 --source7 0 --source8 0 --source9 0 --clock 7987200 --rate 48000 --seconds 0.04 --note 69 --out ym2608-feedback7-pitch-smoke.wav --debug ym2608-feedback7-pitch-smoke.json
)
add_test(NAME chipper_render_ym2610_smoke
    COMMAND chipper_render --chip ym2610 --accuracy authentic --macro lead --wave-shape 5 --fm-feedback 4 --clock 8000000 --rate 48000 --seconds 0.08 --note 69 --out ym2610-smoke.wav --debug ym2610-smoke.json
)
add_test(NAME chipper_render_ym2610_chip_poly_smoke
    COMMAND chipper_render --chip ym2610 --accuracy authentic --macro manual --play-mode chip-poly --wave-shape 8 --clock 8000000 --rate 48000 --seconds 0.08 --note 1 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/ym2610-seven-channel-events.txt --out ym2610-chip-poly-smoke.wav --debug ym2610-chip-poly-smoke.json
)
add_test(NAME chipper_render_ym2610_sources_off_smoke
    COMMAND chipper_render --chip ym2610 --accuracy authentic --macro manual --source1 0 --source2 0 --source3 0 --source4 0 --source5 0 --source6 0 --source7 0 --clock 8000000 --rate 48000 --seconds 0.04 --note 69 --out ym2610-sources-off-smoke.wav --debug ym2610-sources-off-smoke.json
)
add_test(NAME chipper_render_ym2610_ssg_smoke
    COMMAND chipper_render --chip ym2610 --accuracy authentic --macro coin --source1 0 --source2 0 --source3 0 --source4 0 --source5 1 --source6 1 --source7 1 --clock 8000000 --rate 48000 --seconds 0.08 --note 72 --out ym2610-ssg-smoke.wav --debug ym2610-ssg-smoke.json
)
add_test(NAME chipper_render_ym2610_adpcm_a_sample_smoke
    COMMAND chipper_render --chip ym2610 --accuracy authentic --macro drum --source1 1 --source2 1 --source3 1 --source4 1 --source5 1 --source6 1 --source7 0 --opnb-adpcm-a-hex 11111111999999991111111199999999 --clock 8000000 --rate 48000 --seconds 0.08 --note 60 --out ym2610-adpcm-a-sample-smoke.wav --debug ym2610-adpcm-a-sample-smoke.json
)
add_test(NAME chipper_render_ym2610_adpcm_b_sample_smoke
    COMMAND chipper_render --chip ym2610 --accuracy authentic --macro drum --source1 1 --source2 1 --source3 1 --source4 1 --source5 1 --source6 1 --source7 0 --opnb-adpcm-b-hex 11111111999999991111111199999999 --clock 8000000 --rate 48000 --seconds 0.08 --note 60 --out ym2610-adpcm-b-sample-smoke.wav --debug ym2610-adpcm-b-sample-smoke.json
)
add_test(NAME chipper_render_ym2610_feedback0_pitch_smoke
    COMMAND chipper_render --chip ym2610 --accuracy authentic --macro manual --fm-feedback 0 --source5 0 --source6 0 --source7 0 --clock 8000000 --rate 48000 --seconds 0.04 --note 69 --out ym2610-feedback0-pitch-smoke.wav --debug ym2610-feedback0-pitch-smoke.json
)
add_test(NAME chipper_render_ym2610_feedback7_pitch_smoke
    COMMAND chipper_render --chip ym2610 --accuracy authentic --macro manual --fm-feedback 7 --source5 0 --source6 0 --source7 0 --clock 8000000 --rate 48000 --seconds 0.04 --note 69 --out ym2610-feedback7-pitch-smoke.wav --debug ym2610-feedback7-pitch-smoke.json
)
add_test(NAME chipper_render_ym2610b_smoke
    COMMAND chipper_render --chip ym2610b --accuracy authentic --macro lead --wave-shape 5 --fm-feedback 4 --clock 8000000 --rate 48000 --seconds 0.08 --note 69 --out ym2610b-smoke.wav --debug ym2610b-smoke.json
)
add_test(NAME chipper_render_ym2610b_chip_poly_smoke
    COMMAND chipper_render --chip ym2610b --accuracy authentic --macro manual --play-mode chip-poly --wave-shape 8 --clock 8000000 --rate 48000 --seconds 0.08 --note 1 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/ym2610b-nine-channel-events.txt --out ym2610b-chip-poly-smoke.wav --debug ym2610b-chip-poly-smoke.json
)
add_test(NAME chipper_render_ym2610b_sources_off_smoke
    COMMAND chipper_render --chip ym2610b --accuracy authentic --macro manual --source1 0 --source2 0 --source3 0 --source4 0 --source5 0 --source6 0 --source7 0 --source8 0 --source9 0 --clock 8000000 --rate 48000 --seconds 0.04 --note 69 --out ym2610b-sources-off-smoke.wav --debug ym2610b-sources-off-smoke.json
)
add_test(NAME chipper_render_ym2610b_ssg_smoke
    COMMAND chipper_render --chip ym2610b --accuracy authentic --macro coin --source1 0 --source2 0 --source3 0 --source4 0 --source5 0 --source6 0 --source7 1 --source8 1 --source9 1 --clock 8000000 --rate 48000 --seconds 0.08 --note 72 --out ym2610b-ssg-smoke.wav --debug ym2610b-ssg-smoke.json
)
add_test(NAME chipper_render_ym2610b_adpcm_a_sample_smoke
    COMMAND chipper_render --chip ym2610b --accuracy authentic --macro drum --source1 1 --source2 1 --source3 1 --source4 1 --source5 1 --source6 1 --source7 0 --source8 0 --source9 0 --opnb-adpcm-a-hex 11111111999999991111111199999999 --clock 8000000 --rate 48000 --seconds 0.08 --note 60 --out ym2610b-adpcm-a-sample-smoke.wav --debug ym2610b-adpcm-a-sample-smoke.json
)
add_test(NAME chipper_render_ym2610b_adpcm_b_sample_smoke
    COMMAND chipper_render --chip ym2610b --accuracy authentic --macro drum --source1 1 --source2 1 --source3 1 --source4 1 --source5 1 --source6 1 --source7 0 --source8 0 --source9 0 --opnb-adpcm-b-hex 11111111999999991111111199999999 --clock 8000000 --rate 48000 --seconds 0.08 --note 60 --out ym2610b-adpcm-b-sample-smoke.wav --debug ym2610b-adpcm-b-sample-smoke.json
)
add_test(NAME chipper_render_ym2610b_feedback0_pitch_smoke
    COMMAND chipper_render --chip ym2610b --accuracy authentic --macro manual --fm-feedback 0 --source7 0 --source8 0 --source9 0 --clock 8000000 --rate 48000 --seconds 0.04 --note 69 --out ym2610b-feedback0-pitch-smoke.wav --debug ym2610b-feedback0-pitch-smoke.json
)
add_test(NAME chipper_render_ym2610b_feedback7_pitch_smoke
    COMMAND chipper_render --chip ym2610b --accuracy authentic --macro manual --fm-feedback 7 --source7 0 --source8 0 --source9 0 --clock 8000000 --rate 48000 --seconds 0.04 --note 69 --out ym2610b-feedback7-pitch-smoke.wav --debug ym2610b-feedback7-pitch-smoke.json
)
add_test(NAME chipper_render_preset_opn_feedback_bass_held_smoke
    COMMAND chipper_render --preset opn-feedback-bass --rate 48000 --seconds 0.50 --note 48 --out preset-opn-feedback-bass-held.wav --debug preset-opn-feedback-bass-held.json
)
add_test(NAME chipper_render_opl3_smoke
    COMMAND chipper_render --chip opl3 --accuracy authentic --macro lead --wave-shape 4 --clock 14318180 --rate 48000 --seconds 0.08 --note 69 --out opl3-smoke.wav --debug opl3-smoke.json
)
add_test(NAME chipper_render_opl3_stereo_route_smoke
    COMMAND chipper_render --chip opl3 --accuracy authentic --macro manual --play-mode chip-poly --opl-rhythm layer --opl-route alt --control1 1 --control2 1 --wave-shape 2 --clock 14318180 --rate 48000 --seconds 0.10 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/opl3-eighteen-channel-chip-poly-events.txt --out opl3-stereo-route-smoke.wav --debug opl3-stereo-route-smoke.json
)
add_test(NAME chipper_render_opl3_chip_poly_smoke
    COMMAND chipper_render --chip opl3 --accuracy authentic --macro manual --play-mode chip-poly --wave-shape 2 --clock 14318180 --rate 48000 --seconds 0.08 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/opl3-nine-channel-chip-poly-events.txt --out opl3-chip-poly-smoke.wav --debug opl3-chip-poly-smoke.json
)
add_test(NAME chipper_render_opl3_18ch_layer_smoke
    COMMAND chipper_render --chip opl3 --accuracy authentic --macro manual --play-mode chip-poly --opl-rhythm layer --wave-shape 2 --clock 14318180 --rate 48000 --seconds 0.10 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/opl3-eighteen-channel-chip-poly-events.txt --out opl3-18ch-layer-smoke.wav --debug opl3-18ch-layer-smoke.json
)
add_test(NAME chipper_render_opl3_four_op_pair_smoke
    COMMAND chipper_render --chip opl3 --accuracy authentic --macro manual --play-mode chip-poly --opl-rhythm 4op --wave-shape 2 --control1 1.0 --control3 0.35 --control4 0.9 --clock 14318180 --rate 48000 --seconds 0.10 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/opl3-four-op-pair-events.txt --out opl3-four-op-pair-smoke.wav --debug opl3-four-op-pair-smoke.json
)
add_test(NAME chipper_render_opl3_four_op_editor_smoke
    COMMAND chipper_render --chip opl3 --accuracy authentic --macro manual --play-mode chip-poly --opl-rhythm 4op --wave-shape 2 --control1 0.67 --control2 0.35 --control3 0.35 --control4 0.9 --fm-op1-level 0.65 --fm-op2-level 0.45 --fm-op3-level 0.75 --fm-op4-level 0.55 --fm-op1-multiplier 2 --fm-op2-multiplier 3 --fm-op3-multiplier 4 --fm-op4-multiplier 5 --fm-op1-attack-rate 3 --fm-op2-attack-rate 7 --fm-op3-attack-rate 11 --fm-op4-attack-rate 15 --fm-op1-decay-rate 2 --fm-op2-decay-rate 4 --fm-op3-decay-rate 6 --fm-op4-decay-rate 8 --fm-op1-sustain-rate 1 --fm-op2-sustain-rate 5 --fm-op3-sustain-rate 9 --fm-op4-sustain-rate 13 --fm-op1-release-rate 2 --fm-op2-release-rate 6 --fm-op3-release-rate 10 --fm-op4-release-rate 14 --clock 14318180 --rate 48000 --seconds 0.05 --note 60 --out opl3-four-op-editor-smoke.wav --debug opl3-four-op-editor-smoke.json
)
add_test(NAME chipper_render_opl3_operator_fields_smoke
    COMMAND chipper_render --chip opl3 --accuracy authentic --macro manual --play-mode chip-poly --opl-rhythm 4op --wave-shape 2 --control1 0.67 --control2 0.35 --control3 0.35 --control4 0.9 --fm-op1-level 0.65 --fm-op2-level 0.45 --fm-op3-level 0.75 --fm-op4-level 0.55 --fm-op1-multiplier 2 --fm-op2-multiplier 3 --fm-op3-multiplier 4 --fm-op4-multiplier 5 --opl-op1-flags all --opl-op2-flags am --opl-op3-flags vib --opl-op4-flags ksr --opl-op1-ksl 1 --opl-op2-ksl 2 --opl-op3-ksl 3 --opl-op4-ksl 0 --clock 14318180 --rate 48000 --seconds 0.05 --note 60 --out opl3-operator-fields-smoke.wav --debug opl3-operator-fields-smoke.json
)
add_test(NAME chipper_render_opl3_operator_fields_alias_smoke
    COMMAND chipper_render --chip opl3 --accuracy authentic --macro manual --play-mode big-mono --wave-shape 2 --opl3-op1-flags none --opl3-op2-flags preset --opl3-op1-ksl 0 --opl3-op2-ksl preset --clock 14318180 --rate 48000 --seconds 0.05 --note 60 --out opl3-operator-fields-alias-smoke.wav --debug opl3-operator-fields-alias-smoke.json
)
add_test(NAME chipper_render_opl3_sources_off_smoke
    COMMAND chipper_render --chip opl3 --accuracy authentic --macro manual --source1 0 --source2 0 --source3 0 --source4 0 --source5 0 --source6 0 --source7 0 --source8 0 --source9 0 --clock 14318180 --rate 48000 --seconds 0.04 --note 69 --out opl3-sources-off-smoke.wav --debug opl3-sources-off-smoke.json
)
add_test(NAME chipper_render_opl3_rhythm_smoke
    COMMAND chipper_render --chip opl3 --accuracy authentic --macro drum --opl-rhythm rhythm --clock 14318180 --rate 48000 --seconds 0.08 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/opl-rhythm-events.txt --out opl3-rhythm-smoke.wav --debug opl3-rhythm-smoke.json
)
add_test(NAME chipper_render_preset_opl2_rhythm_kit_smoke
    COMMAND chipper_render --preset opl2-rhythm-kit --rate 48000 --seconds 0.08 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/opl-rhythm-events.txt --out preset-opl2-rhythm-kit.wav --debug preset-opl2-rhythm-kit.json
)
add_test(NAME chipper_render_preset_opl2_rhythm_bell_hit_smoke
    COMMAND chipper_render --preset opl2-rhythm-bell-hit --rate 48000 --seconds 0.08 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/opl-rhythm-events.txt --out preset-opl2-rhythm-bell-hit.wav --debug preset-opl2-rhythm-bell-hit.json
)
add_test(NAME chipper_render_preset_opl2_bass_held_smoke
    COMMAND chipper_render --preset opl2-bass --rate 48000 --seconds 0.50 --note 48 --out preset-opl2-bass-held.wav --debug preset-opl2-bass-held.json
)
add_test(NAME chipper_render_preset_opl3_layer_arp_smoke
    COMMAND chipper_render --preset opl3-layer-arp --rate 48000 --seconds 0.10 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/opl3-eighteen-channel-chip-poly-events.txt --out preset-opl3-layer-arp.wav --debug preset-opl3-layer-arp.json
)
add_test(NAME chipper_render_ym2151_smoke
    COMMAND chipper_render --chip ym2151 --accuracy authentic --macro lead --wave-shape 5 --fm-op2-multiplier 0.5 --fm-op4-multiplier 15 --fm-op2-attack-rate 0 --fm-op4-attack-rate 31 --fm-op2-decay-rate 0 --fm-op4-decay-rate 31 --fm-op2-sustain-rate 0 --fm-op4-sustain-rate 31 --fm-op2-release-rate 0 --fm-op4-release-rate 15 --clock 3579545 --rate 48000 --seconds 0.08 --note 69 --out ym2151-smoke.wav --debug ym2151-smoke.json
)
add_test(NAME chipper_render_ym2151_operator_detune_smoke
    COMMAND chipper_render --chip ym2151 --accuracy authentic --macro manual --wave-shape 5 --fm-op1-multiplier 0.5 --fm-op2-multiplier 15 --fm-op3-multiplier 1 --fm-op4-multiplier 2 --fm-op1-sustain-rate 0 --fm-op2-sustain-rate 1 --fm-op3-sustain-rate 5 --fm-op4-sustain-rate 31 --opm-op1-dt1 3 --opm-op2-dt1 7 --opm-op3-dt1 0 --opm-op4-dt1 4 --opm-op1-dt2 0 --opm-op2-dt2 1 --opm-op3-dt2 2 --opm-op4-dt2 3 --clock 3579545 --rate 48000 --seconds 0.08 --note 69 --out ym2151-operator-detune-smoke.wav --debug ym2151-operator-detune-smoke.json
)
add_test(NAME chipper_render_ym2151_feedback6_smoke
    COMMAND chipper_render --chip ym2151 --accuracy authentic --macro manual --wave-shape 5 --opm-feedback 6 --clock 3579545 --rate 48000 --seconds 0.08 --note 69 --out ym2151-feedback6-smoke.wav --debug ym2151-feedback6-smoke.json
)
add_test(NAME chipper_render_ym2151_chip_poly_smoke
    COMMAND chipper_render --chip ym2151 --accuracy authentic --macro manual --play-mode chip-poly --wave-shape 8 --clock 3579545 --rate 48000 --seconds 0.08 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/ym2151-eight-channel-chip-poly-events.txt --out ym2151-chip-poly-smoke.wav --debug ym2151-chip-poly-smoke.json
)
add_test(NAME chipper_render_ym2151_sources_off_smoke
    COMMAND chipper_render --chip ym2151 --accuracy authentic --macro manual --source1 0 --source2 0 --source3 0 --source4 0 --source5 0 --source6 0 --source7 0 --source8 0 --clock 3579545 --rate 48000 --seconds 0.04 --note 69 --out ym2151-sources-off-smoke.wav --debug ym2151-sources-off-smoke.json
)
add_test(NAME chipper_render_ym2151_env_pad_smoke
    COMMAND chipper_render --chip ym2151 --accuracy authentic --macro lead --opm-envelope pad --clock 3579545 --rate 48000 --seconds 0.08 --note 69 --out ym2151-env-pad-smoke.wav --debug ym2151-env-pad-smoke.json
)
add_test(NAME chipper_render_ym2151_pan_right_smoke
    COMMAND chipper_render --chip ym2151 --accuracy authentic --macro lead --opm-pan right --clock 3579545 --rate 48000 --seconds 0.08 --note 69 --out ym2151-pan-right-smoke.wav --debug ym2151-pan-right-smoke.json
)
add_test(NAME chipper_render_ym2151_noise_high_smoke
    COMMAND chipper_render --chip ym2151 --accuracy authentic --macro drum --opm-noise high --clock 3579545 --rate 48000 --seconds 0.08 --note 48 --out ym2151-noise-high-smoke.wav --debug ym2151-noise-high-smoke.json
)
add_test(NAME chipper_render_ym2151_lfo_depth_smoke
    COMMAND chipper_render --chip ym2151 --accuracy authentic --macro lead --wave-shape 5 --opm-lfo-depth 0.65 --clock 3579545 --rate 48000 --seconds 0.12 --note 69 --out ym2151-lfo-depth-smoke.wav --debug ym2151-lfo-depth-smoke.json
)
add_test(NAME chipper_render_ym2151_direct_lfo_smoke
    COMMAND chipper_render --chip ym2151 --accuracy authentic --macro manual --wave-shape 5 --opm-feedback 7 --opm-lfo-depth 0.65 --opm-lfo-waveform noise --opm-pms 7 --opm-ams 3 --clock 3579545 --rate 48000 --seconds 0.08 --note 69 --out ym2151-direct-lfo-smoke.wav --debug ym2151-direct-lfo-smoke.json
)
add_test(NAME chipper_render_preset_opm_noise_envelope_hit_smoke
    COMMAND chipper_render --preset opm-noise-envelope-hit --rate 48000 --seconds 0.08 --note 48 --out preset-opm-noise-envelope-hit.wav --debug preset-opm-noise-envelope-hit.json
)
add_test(NAME chipper_render_preset_opm_arcade_bass_held_smoke
    COMMAND chipper_render --preset opm-arcade-bass --rate 48000 --seconds 0.50 --note 48 --out preset-opm-arcade-bass-held.wav --debug preset-opm-arcade-bass-held.json
)
add_test(NAME chipper_render_preset_opm_marble_bell_smoke
    COMMAND chipper_render --preset opm-marble-bell --rate 48000 --seconds 0.08 --note 76 --out preset-opm-marble-bell.wav --debug preset-opm-marble-bell.json
)
add_test(NAME chipper_render_preset_opm_slow_motion_pad_smoke
    COMMAND chipper_render --preset opm-slow-motion-pad --rate 48000 --seconds 0.12 --note 60 --out preset-opm-slow-motion-pad.wav --debug preset-opm-slow-motion-pad.json
)
add_test(NAME chipper_render_preset_opm_hollow_pad_smoke
    COMMAND chipper_render --preset opm-hollow-pad --rate 48000 --seconds 0.12 --note 60 --out preset-opm-hollow-pad.wav --debug preset-opm-hollow-pad.json
)
add_test(NAME chipper_render_ym2612_held_tail_smoke
    COMMAND chipper_render --chip ym2612 --accuracy authentic --macro bass --wave-shape 1 --clock 7670454 --rate 48000 --seconds 0.50 --note 48 --out ym2612-held-tail-smoke.wav --debug ym2612-held-tail-smoke.json
)
add_test(NAME chipper_render_ym2612_low_level_held_tail_smoke
    COMMAND chipper_render --chip ym2612 --accuracy authentic --macro lead --wave-shape 5 --control4 0.35 --clock 7670454 --rate 48000 --seconds 0.50 --note 60 --out ym2612-low-level-held-tail-smoke.wav --debug ym2612-low-level-held-tail-smoke.json
)
add_test(NAME chipper_render_ym2612_release_tail_smoke
    COMMAND chipper_render --chip ym2612 --accuracy authentic --macro bass --wave-shape 1 --clock 7670454 --rate 48000 --seconds 0.30 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/ym2612-release-events.txt --out ym2612-release-tail-smoke.wav --debug ym2612-release-tail-smoke.json
)
add_test(NAME chipper_render_opl3_held_tail_smoke
    COMMAND chipper_render --chip opl3 --accuracy authentic --macro bass --wave-shape 1 --clock 14318180 --rate 48000 --seconds 0.50 --note 48 --out opl3-held-tail-smoke.wav --debug opl3-held-tail-smoke.json
)
add_test(NAME chipper_render_opl3_low_level_held_tail_smoke
    COMMAND chipper_render --chip opl3 --accuracy authentic --macro lead --wave-shape 2 --control4 0.30 --clock 14318180 --rate 48000 --seconds 0.50 --note 60 --out opl3-low-level-held-tail-smoke.wav --debug opl3-low-level-held-tail-smoke.json
)
add_test(NAME chipper_render_opl3_release_tail_smoke
    COMMAND chipper_render --chip opl3 --accuracy authentic --macro bass --wave-shape 1 --clock 14318180 --rate 48000 --seconds 0.30 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/opl3-release-events.txt --out opl3-release-tail-smoke.wav --debug opl3-release-tail-smoke.json
)
add_test(NAME chipper_render_ym2151_held_tail_smoke
    COMMAND chipper_render --chip ym2151 --accuracy authentic --macro bass --wave-shape 1 --clock 3579545 --rate 48000 --seconds 0.50 --note 48 --out ym2151-held-tail-smoke.wav --debug ym2151-held-tail-smoke.json
)
add_test(NAME chipper_render_ym2151_low_level_held_tail_smoke
    COMMAND chipper_render --chip ym2151 --accuracy authentic --macro lead --wave-shape 5 --control4 0.35 --clock 3579545 --rate 48000 --seconds 0.50 --note 60 --out ym2151-low-level-held-tail-smoke.wav --debug ym2151-low-level-held-tail-smoke.json
)
add_test(NAME chipper_render_ym2151_release_tail_smoke
    COMMAND chipper_render --chip ym2151 --accuracy authentic --macro bass --wave-shape 1 --clock 3579545 --rate 48000 --seconds 0.30 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/ym2151-release-events.txt --out ym2151-release-tail-smoke.wav --debug ym2151-release-tail-smoke.json
)
add_test(NAME chipper_render_ym2413_smoke
    COMMAND chipper_render --chip ym2413 --accuracy authentic --macro lead --wave-shape trumpet --clock 3579545 --rate 48000 --seconds 0.08 --note 69 --out ym2413-smoke.wav --debug ym2413-smoke.json
)
add_test(NAME chipper_render_ym2413_custom_patch_smoke
    COMMAND chipper_render --chip ym2413 --accuracy authentic --macro lead --wave-shape custom --control3 0.42 --control4 0.72 --fm-op1-level 0.72 --fm-op2-level 0.68 --fm-op1-multiplier 3 --fm-op2-multiplier 1 --fm-op1-attack-rate 15 --fm-op2-attack-rate 14 --fm-op1-decay-rate 7 --fm-op2-decay-rate 5 --fm-op1-sustain-rate 4 --fm-op2-sustain-rate 5 --fm-op1-release-rate 6 --fm-op2-release-rate 4 --clock 3579545 --rate 48000 --seconds 0.08 --note 69 --out ym2413-custom-patch-smoke.wav --debug ym2413-custom-patch-smoke.json
)
add_test(NAME chipper_render_ym2413_high_instrument_smoke
    COMMAND chipper_render --chip ym2413 --accuracy authentic --macro lead --wave-shape electric-guitar --clock 3579545 --rate 48000 --seconds 0.08 --note 69 --out ym2413-high-instrument-smoke.wav --debug ym2413-high-instrument-smoke.json
)
add_test(NAME chipper_render_ym2413_held_tail_smoke
    COMMAND chipper_render --chip ym2413 --accuracy authentic --macro bass --wave-shape synth-bass --control4 0.86 --clock 3579545 --rate 48000 --seconds 2.00 --note 48 --out ym2413-held-tail-smoke.wav --debug ym2413-held-tail-smoke.json
)
add_test(NAME chipper_render_ym2413_low_level_held_tail_smoke
    COMMAND chipper_render --chip ym2413 --accuracy authentic --macro lead --wave-shape 3 --control4 0.35 --clock 3579545 --rate 48000 --seconds 0.50 --note 60 --out ym2413-low-level-held-tail-smoke.wav --debug ym2413-low-level-held-tail-smoke.json
)
add_test(NAME chipper_render_ym2413_release_tail_smoke
    COMMAND chipper_render --chip ym2413 --accuracy authentic --macro bass --wave-shape synth-bass --clock 3579545 --rate 48000 --seconds 0.30 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/ym2413-release-events.txt --out ym2413-release-tail-smoke.wav --debug ym2413-release-tail-smoke.json
)
add_test(NAME chipper_render_ym2413_chip_poly_smoke
    COMMAND chipper_render --chip ym2413 --accuracy authentic --macro manual --play-mode chip-poly --wave-shape 4 --clock 3579545 --rate 48000 --seconds 0.10 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/ym2413-nine-channel-chip-poly-events.txt --out ym2413-chip-poly-smoke.wav --debug ym2413-chip-poly-smoke.json
)
add_test(NAME chipper_render_ym2413_sources_off_smoke
    COMMAND chipper_render --chip ym2413 --accuracy authentic --macro manual --source1 0 --source2 0 --source3 0 --source4 0 --source5 0 --source6 0 --source7 0 --source8 0 --source9 0 --clock 3579545 --rate 48000 --seconds 0.04 --note 69 --out ym2413-sources-off-smoke.wav --debug ym2413-sources-off-smoke.json
)
add_test(NAME chipper_render_ym2413_rhythm_smoke
    COMMAND chipper_render --chip ym2413 --accuracy authentic --macro drum --opll-rhythm rhythm --clock 3579545 --rate 48000 --seconds 0.08 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/ym2413-rhythm-events.txt --out ym2413-rhythm-smoke.wav --debug ym2413-rhythm-smoke.json
)
add_test(NAME chipper_render_ym2413_rhythm_sources_smoke
    COMMAND chipper_render --chip ym2413 --accuracy authentic --macro drum --opll-rhythm rhythm --source7 1 --source8 0 --source9 1 --level7 1.0 --level8 0.5 --level9 0.25 --clock 3579545 --rate 48000 --seconds 0.08 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/ym2413-rhythm-events.txt --out ym2413-rhythm-sources-smoke.wav --debug ym2413-rhythm-sources-smoke.json
)
add_test(NAME chipper_render_preset_opll_rhythm_alarm_kit_smoke
    COMMAND chipper_render --preset opll-rhythm-alarm-kit --rate 48000 --seconds 0.08 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/ym2413-rhythm-events.txt --out preset-opll-rhythm-alarm-kit.wav --debug preset-opll-rhythm-alarm-kit.json
)
add_test(NAME chipper_render_preset_opll_preset_bass_held_smoke
    COMMAND chipper_render --preset opll-preset-bass --rate 48000 --seconds 0.50 --note 48 --out preset-opll-preset-bass-held.wav --debug preset-opll-preset-bass-held.json
)
add_test(NAME chipper_render_preset_opll_custom_bass_held_smoke
    COMMAND chipper_render --preset opll-custom-bass --rate 48000 --seconds 0.50 --note 48 --out preset-opll-custom-bass-held.wav --debug preset-opll-custom-bass-held.json
)
add_test(NAME chipper_render_preset_opll_envelope_bell_smoke
    COMMAND chipper_render --preset opll-envelope-bell --rate 48000 --seconds 0.08 --note 76 --out preset-opll-envelope-bell.wav --debug preset-opll-envelope-bell.json
)
add_test(NAME chipper_render_preset_opll_alarm_pulse_smoke
    COMMAND chipper_render --preset opll-alarm-pulse --rate 48000 --seconds 0.08 --note 69 --out preset-opll-alarm-pulse.wav --debug preset-opll-alarm-pulse.json
)
add_test(NAME chipper_render_list_descriptor_smoke
    COMMAND chipper_render --list-descriptors --debug chipper-descriptors.json
)
add_test(NAME chipper_render_list_presets_smoke
    COMMAND chipper_render --list-presets --debug chipper-presets.json
)
set_tests_properties(chipper_render_list_presets_smoke PROPERTIES FIXTURES_SETUP chipper_preset_catalog)
set(CHIPPER_PRESET_CHIP_MINIMUMS
    nes:16
    nesVrc6:12
    nesFds:12
    nesSunsoft5b:12
    nesMmc5:12
    nesVrc7:12
    dmg:16
    sid:19
    ym2149:17
    sn76489:17
    saa1099:12
    pcSpeaker:12
    zxSpectrumBeeper:12
    ym2612:19
    opl3:17
    ym2151:16
    ym2203:12
    ym2608:12
    ym2610:12
    ym2610b:12
    spc700:16
    pokey:15
    paula:13
    huc6280:13
    namcoWsg:16
    ym2413:19
    scc:17
)
foreach(chipper_preset_chip_minimum IN LISTS CHIPPER_PRESET_CHIP_MINIMUMS)
    string(REPLACE ":" ";" chipper_preset_chip_parts "${chipper_preset_chip_minimum}")
    list(GET chipper_preset_chip_parts 0 chipper_preset_chip)
    add_test(NAME chipper_render_list_${chipper_preset_chip}_presets_smoke
        COMMAND chipper_render --list-presets --chip ${chipper_preset_chip} --debug chipper-${chipper_preset_chip}-presets.json
    )
    set_tests_properties(chipper_render_list_${chipper_preset_chip}_presets_smoke PROPERTIES FIXTURES_SETUP chipper_${chipper_preset_chip}_preset_catalog)
endforeach()
add_test(NAME chipper_render_describe_nes_smoke
    COMMAND chipper_render --describe-chip nes --debug nes-descriptor.json
)

include(${CMAKE_CURRENT_LIST_DIR}/RendererAssertions.cmake)

include(${CMAKE_CURRENT_LIST_DIR}/DiscoveryAndReferenceTests.cmake)

include(${CMAKE_CURRENT_LIST_DIR}/Ym2149Tests.cmake)
