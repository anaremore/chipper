foreach(test IN ITEMS parameter_midi_cc processor_midi_cc processor_performance processor_native_motion editor_size)
    chipper_add_plugin_tool(chipper_${test}_smoke tests/${test}_smoke.cpp)
    add_test(NAME chipper_${test}_smoke COMMAND chipper_${test}_smoke)
endforeach()
target_compile_definitions(chipper_processor_midi_cc_smoke PRIVATE
    CHIPPER_STATE_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/state")
if(MSVC)
    target_link_options(chipper_processor_midi_cc_smoke PRIVATE /STACK:8388608)
endif()
set_tests_properties(chipper_processor_midi_cc_smoke PROPERTIES TIMEOUT 30 LABELS "state;release-gate")
set_tests_properties(chipper_processor_performance_smoke PROPERTIES TIMEOUT 30 LABELS "performance;release-gate" RUN_SERIAL TRUE)

chipper_add_plugin_tool(chipper_processor_state_concurrency_smoke tests/processor_state_concurrency_smoke.cpp)
add_test(NAME chipper_processor_state_concurrency_smoke COMMAND chipper_processor_state_concurrency_smoke)
set_tests_properties(chipper_processor_state_concurrency_smoke PROPERTIES
    TIMEOUT 60 LABELS "state;performance;release-gate" RUN_SERIAL TRUE)

add_executable(chipper_vst3_host_state_smoke
    tests/vst3_host_state_smoke.cpp
)

add_dependencies(chipper_vst3_host_state_smoke Chipper_VST3)

target_compile_definitions(chipper_vst3_host_state_smoke
    PRIVATE
        JUCE_PLUGINHOST_VST3=1
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
)

target_link_libraries(chipper_vst3_host_state_smoke
    PRIVATE
        juce::juce_audio_formats
        juce::juce_audio_processors_headless
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

add_test(NAME chipper_vst3_host_state_smoke
    COMMAND chipper_vst3_host_state_smoke "$<TARGET_FILE:Chipper_VST3>"
)
set_tests_properties(chipper_vst3_host_state_smoke PROPERTIES
    LABELS "host;release-gate;state"
    TIMEOUT 30
)


add_test(NAME chipper_ui_snapshot_smoke
    COMMAND chipper_ui_snapshot
        --output "${CMAKE_CURRENT_BINARY_DIR}/ui-snapshot-smoke"
        --manifest-only
)

add_test(NAME chipper_ui_browser_snapshot_smoke
    COMMAND chipper_ui_snapshot
        --output "${CMAKE_CURRENT_BINARY_DIR}/ui-browser-snapshot-smoke"
        --chip nes
        --width both
        --workspace browser
        --manifest-only
)

add_test(NAME chipper_ui_motion_snapshot_smoke
    COMMAND chipper_ui_snapshot
        --output "${CMAKE_CURRENT_BINARY_DIR}/ui-motion-snapshot-smoke"
        --chip nes
        --width both
        --workspace motion
        --manifest-only
)

add_test(NAME chipper_ui_opl3_four_op_snapshot_smoke
    COMMAND chipper_ui_snapshot
        --output "${CMAKE_CURRENT_BINARY_DIR}/ui-opl3-four-op-snapshot-smoke"
        --chip opl3
        --width both
        --workspace opl4op
)

add_test(NAME chipper_ui_png_snapshot_smoke
    COMMAND chipper_ui_snapshot
        --output "${CMAKE_CURRENT_BINARY_DIR}/ui-png-snapshot-smoke"
        --chip sid
        --width 1180
        --workspace all
)
