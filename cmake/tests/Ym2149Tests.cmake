# YM2149 renderer and independent-oracle gates.

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

add_test(NAME chipper_render_ym2149_ayumi_reference
    COMMAND chipper_render --chip ym2149 --accuracy authentic --macro manual --clock 1773400 --rate 48000 --seconds 0.25 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/ym2149-reference-tone-events.txt --source1 1 --source2 1 --source3 1 --source4 1 --level1 1 --level2 1 --level3 1 --level4 1 --stereo-spread 0 --output-db 0 --out ym2149-ayumi-reference-candidate.wav --debug ym2149-ayumi-reference-candidate.json
)
set_tests_properties(chipper_render_ym2149_ayumi_reference PROPERTIES
    LABELS "audio;reference;release-gate"
)

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
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/ym-envelope-fast-smoke.json --implemented-contains partial --macro Manual --register-writes 0 --note-events 0 --min-peak 0.02 --core-min envelopeDecayControl=1 --core-max envelopeDecayControl=1 --core-min envelopeShapeChoice=4 --core-max envelopeShapeChoice=4 --core-min envelopeShape=14 --core-max envelopeShape=14 --core-min envelopePeriod=16 --core-max envelopePeriod=16 --descriptor-param-kind envelopeDecay=chipRegister --descriptor-param-surface envelopeDecay=slider --descriptor-param-label "envelopeDecay=Hardware Envelope Speed"
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

add_test(NAME chipper_ym2149_ayumi_reference_metadata
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_reference_metadata.py ${CMAKE_CURRENT_SOURCE_DIR}/tests/references/ym2149-ayumi-tone-a.json
)
set_tests_properties(chipper_ym2149_ayumi_reference_metadata PROPERTIES
    LABELS "audio;reference;release-gate"
)

add_test(NAME chipper_render_ym2149_ayumi_reference_assert
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_render_json.py ${CMAKE_CURRENT_BINARY_DIR}/ym2149-ayumi-reference-candidate.json --implemented-contains emu2149 --macro Manual --stereo-spread 0 --register-writes 14 --note-events 1 --min-peak 0.35 --max-peak 0.45 --min-rms 0.20 --max-rms 0.26 --core-min periodA=288 --core-max periodA=288 --core-min periodB=1 --core-max periodB=1 --core-min periodC=1 --core-max periodC=1 --core-min mixer=62 --core-max mixer=62 --core-min toneEnabledA=1 --core-max toneEnabledA=1 --core-min toneEnabledB=0 --core-max toneEnabledB=0 --core-min toneEnabledC=0 --core-max toneEnabledC=0 --core-min noiseEnabledA=0 --core-max noiseEnabledA=0 --core-min volumeA=15 --core-max volumeA=15 --core-min volumeB=0 --core-max volumeB=0 --core-min volumeC=0 --core-max volumeC=0
)
set_tests_properties(chipper_render_ym2149_ayumi_reference_assert PROPERTIES
    DEPENDS chipper_render_ym2149_ayumi_reference
    LABELS "audio;reference;release-gate"
)

add_test(NAME chipper_render_ym2149_ayumi_reference_compare
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_reference_wav.py ${CMAKE_CURRENT_SOURCE_DIR}/tests/references/ym2149-ayumi-tone-a.wav ${CMAKE_CURRENT_BINARY_DIR}/ym2149-ayumi-reference-candidate.wav --min-correlation 0.97 --max-normalized-rmse 0.25 --min-rms-ratio 1.05 --max-rms-ratio 1.30 --alignment-window-frames 32 --max-accepted-lag-frames 16 --max-length-difference-frames 0 --json ${CMAKE_CURRENT_BINARY_DIR}/ym2149-ayumi-reference-comparison.json
)
set_tests_properties(chipper_render_ym2149_ayumi_reference_compare PROPERTIES
    DEPENDS "chipper_render_ym2149_ayumi_reference;chipper_ym2149_ayumi_reference_metadata"
    LABELS "audio;reference;release-gate"
)
