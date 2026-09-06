add_test(NAME chipper_featured_presets_audition
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_featured_presets.py
        --renderer $<TARGET_FILE:chipper_render> --work-dir ${CMAKE_CURRENT_BINARY_DIR}/featured-auditions)
set_tests_properties(chipper_featured_presets_audition PROPERTIES LABELS "audio;preset;release-gate" TIMEOUT 120)

foreach(channel IN ITEMS b c)
    set(name ym2149-ayumi-tone-${channel})
    add_test(NAME chipper_${name}_metadata
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_reference_metadata.py
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/references/${name}.json)
    add_test(NAME chipper_${name}_render
        COMMAND chipper_render --chip ym2149 --accuracy authentic --macro manual --clock 1773400
            --rate 48000 --seconds 0.25 --events ${CMAKE_CURRENT_SOURCE_DIR}/tests/events/ym2149-reference-tone-${channel}-events.txt
            --source1 1 --source2 1 --source3 1 --source4 1 --level1 1 --level2 1 --level3 1 --level4 1
            --stereo-spread 0 --output-db 0 --out ${name}-candidate.wav --debug ${name}-candidate.json)
    set_tests_properties(chipper_${name}_render PROPERTIES FIXTURES_SETUP ${name})
    add_test(NAME chipper_${name}_compare
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_reference_wav.py
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/references/${name}.wav ${CMAKE_CURRENT_BINARY_DIR}/${name}-candidate.wav
            --min-correlation 0.97 --max-normalized-rmse 0.25 --min-rms-ratio 1.05 --max-rms-ratio 1.30
            --alignment-window-frames 32 --max-accepted-lag-frames 16 --max-length-difference-frames 0
            --json ${CMAKE_CURRENT_BINARY_DIR}/${name}-comparison.json)
    set_tests_properties(chipper_${name}_compare PROPERTIES FIXTURES_REQUIRED ${name} DEPENDS chipper_${name}_metadata)
    set_tests_properties(chipper_${name}_metadata chipper_${name}_render chipper_${name}_compare
        PROPERTIES LABELS "audio;reference;release-gate")
endforeach()
