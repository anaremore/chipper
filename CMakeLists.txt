cmake_minimum_required(VERSION 3.22)

project(Chipper VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CHIPPER_BUILD_PLUGIN "Build the JUCE VST3 plugin" ON)
option(CHIPPER_BUILD_TESTS "Build and register the complete test suite" ON)
if(CHIPPER_BUILD_TESTS)
    find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
    enable_testing()
endif()

add_library(chipper_engine STATIC
    ThirdParty/emu2149/emu2149.c
    ThirdParty/emu2149/emu2149.h
    ThirdParty/emu2212/emu2212.c
    ThirdParty/emu2212/emu2212.h
    ThirdParty/emu2413/emu2413.c
    ThirdParty/emu2413/emu2413.h
    ThirdParty/emu76489/emu76489.c
    ThirdParty/emu76489/emu76489.h
    ThirdParty/ymfm/src/ymfm_adpcm.cpp
    ThirdParty/ymfm/src/ymfm_adpcm.h
    ThirdParty/ymfm/src/ymfm_fm.h
    ThirdParty/ymfm/src/ymfm_fm.ipp
    ThirdParty/ymfm/src/ymfm_misc.cpp
    ThirdParty/ymfm/src/ymfm_misc.h
    ThirdParty/ymfm/src/ymfm_opl.cpp
    ThirdParty/ymfm/src/ymfm_opl.h
    ThirdParty/ymfm/src/ymfm_opm.cpp
    ThirdParty/ymfm/src/ymfm_opm.h
    ThirdParty/ymfm/src/ymfm_opn.cpp
    ThirdParty/ymfm/src/ymfm_opn.h
    ThirdParty/ymfm/src/ymfm_opq.cpp
    ThirdParty/ymfm/src/ymfm_opq.h
    ThirdParty/ymfm/src/ymfm_opz.cpp
    ThirdParty/ymfm/src/ymfm_opz.h
    ThirdParty/ymfm/src/ymfm_pcm.cpp
    ThirdParty/ymfm/src/ymfm_pcm.h
    ThirdParty/ymfm/src/ymfm_ssg.cpp
    ThirdParty/ymfm/src/ymfm_ssg.h
    ThirdParty/ymfm/src/ymfm.h
    Source/Engine/ControlRegistry.cpp
    Source/Engine/ControlRegistry.h
    Source/Engine/ChipDescriptors.cpp
    Source/Engine/ChipDescriptors.h
    Source/Engine/ChipCore.cpp
    Source/Engine/ChipCore.h
    Source/Engine/Cores/UnsupportedCore.cpp
    Source/Engine/Cores/DmgApuCore.cpp
    Source/Engine/Cores/SidCore.cpp
    Source/Engine/Cores/NesApuCore.cpp
    Source/Engine/Cores/Ym2149Core.cpp
    Source/Engine/Cores/PokeyCore.cpp
    Source/Engine/Cores/Sn76489Core.cpp
    Source/Engine/Cores/Saa1099Core.cpp
    Source/Engine/Cores/PcSpeakerCore.cpp
    Source/Engine/Cores/ZxSpectrumBeeperCore.cpp
    Source/Engine/Cores/Huc6280Core.cpp
    Source/Engine/Cores/SccCore.cpp
    Source/Engine/Cores/NamcoWsgCore.cpp
    Source/Engine/Cores/Spc700Core.cpp
    Source/Engine/Cores/Ym2413Core.cpp
    Source/Engine/Cores/Ym2203Core.cpp
    Source/Engine/Cores/Ym2608Core.cpp
    Source/Engine/Cores/Ym2610Core.cpp
    Source/Engine/Cores/Ym2612Core.cpp
    Source/Engine/Cores/Opl3Core.cpp
    Source/Engine/Cores/Ym2151Core.cpp
    Source/Engine/Cores/PaulaCore.cpp
    Source/Engine/MotionData.cpp
    Source/Engine/MotionData.h
    Source/Engine/WavetableData.cpp
    Source/Engine/WavetableData.h
    Source/Engine/YamahaAdpcmBank.cpp
    Source/Engine/YamahaAdpcmBank.h
    Source/Engine/YamahaAdpcmCodec.cpp
    Source/Engine/YamahaAdpcmCodec.h
    Source/FeaturedPresets.cpp
    Source/Presets.cpp
    Source/Presets.h
)

target_include_directories(chipper_engine PUBLIC Source ThirdParty/emu2149 ThirdParty/emu2212 ThirdParty/emu2413 ThirdParty/emu76489 ThirdParty/ymfm/src)
target_compile_features(chipper_engine PUBLIC cxx_std_20)
set_target_properties(chipper_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_executable(chipper_render tools/chipper_render.cpp)
target_link_libraries(chipper_render PRIVATE chipper_engine)

if(CHIPPER_BUILD_TESTS)
    include(cmake/tests/EngineTests.cmake)
endif()

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.13
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(JUCE)

target_compile_definitions(chipper_render PRIVATE
    CHIPPER_RENDER_HAS_JUCE_AUDIO_FORMATS=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
)
target_link_libraries(chipper_render PRIVATE
    juce::juce_audio_formats
    juce::juce_recommended_config_flags
)

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

set(CHIPPER_PLUGIN_SOURCES
    Source/BuildInfo.cpp
    Source/BuildInfo.h
    Source/Parameters.cpp
    Source/Parameters.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/State/MotionState.cpp
    Source/State/MotionState.h
    Source/State/PluginStateSchema.cpp
    Source/State/PluginStateSchema.h
    Source/State/ProjectState.cpp
    Source/State/ProjectState.h
    Source/State/SampleAsset.h
    Source/State/SampleStateMetadata.h
    Source/State/WavetableState.cpp
    Source/State/WavetableState.h
    Source/UI/ChipUiModel.cpp
    Source/UI/ChipUiModel.h
    Source/UI/ChipperSsgChannel.cpp
    Source/UI/ChipperSsgChannel.h
    Source/UI/ChipperChoiceControl.cpp
    Source/UI/ChipperChoiceControl.h
    Source/UI/ChipperFmEditor.cpp
    Source/UI/ChipperFmEditor.h
    Source/UI/ChipperFocusOutline.cpp
    Source/UI/ChipperFocusOutline.h
    Source/UI/ChipperWorkflowBar.cpp
    Source/UI/ChipperWorkflowBar.h
    Source/UI/ChipperMotionLab.cpp
    Source/UI/ChipperMotionLab.h
    Source/UI/ChipperWaveLab.cpp
    Source/UI/ChipperWaveLab.h
    Source/UI/ChipperPresetBrowser.cpp
    Source/UI/ChipperPresetBrowser.h
    Source/UI/ChipperRelationshipMap.cpp
    Source/UI/ChipperRelationshipMap.h
    Source/UI/ChipperEditorShell.cpp
    Source/UI/ChipperEditorShell.h
    Source/UI/ChipperWorkspaces.cpp
    Source/UI/ChipperWorkspaces.h
)

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
        ${CHIPPER_PLUGIN_SOURCES}
)

target_include_directories(Chipper PUBLIC Source "${CHIPPER_GENERATED_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/Chipper_artefacts/JuceLibraryCode")

target_compile_definitions(Chipper
    PUBLIC
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_MODAL_LOOPS_PERMITTED=1
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

# Link tests to the same compiled processor/editor and JUCE code as the plugin.
# Inherit its header configuration without compiling JUCE's INTERFACE_SOURCES again.
function(chipper_add_plugin_tool target source)
    add_executable(${target} ${source})
    add_dependencies(${target} Chipper)
    target_link_libraries(${target} PRIVATE Chipper chipper_engine)
    target_include_directories(${target} PRIVATE $<TARGET_PROPERTY:Chipper,INCLUDE_DIRECTORIES>)
    target_compile_definitions(${target} PRIVATE $<TARGET_PROPERTY:Chipper,COMPILE_DEFINITIONS>)
endfunction()

chipper_add_plugin_tool(chipper_ui_snapshot tools/chipper_ui_snapshot.cpp)

if(CHIPPER_BUILD_TESTS)
    include(cmake/tests/PluginTests.cmake)
endif()
