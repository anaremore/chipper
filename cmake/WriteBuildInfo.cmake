if (NOT DEFINED OUTPUT_FILE)
    message(FATAL_ERROR "OUTPUT_FILE is required")
endif()

if (NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "SOURCE_DIR is required")
endif()

if (NOT DEFINED CHIPPER_VERSION)
    set(CHIPPER_VERSION "0.0.0")
endif()

execute_process(
    COMMAND git -C "${SOURCE_DIR}" rev-parse --short=10 HEAD
    OUTPUT_VARIABLE CHIPPER_GIT_SHA
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if ("${CHIPPER_GIT_SHA}" STREQUAL "")
    set(CHIPPER_GIT_SHA "unknown")
endif()

execute_process(
    COMMAND git -C "${SOURCE_DIR}" status --porcelain --untracked-files=no
    OUTPUT_VARIABLE CHIPPER_GIT_STATUS
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

set(CHIPPER_GIT_STATE "clean")
set(CHIPPER_DIRTY_SUFFIX "")
if (NOT "${CHIPPER_GIT_STATUS}" STREQUAL "")
    set(CHIPPER_GIT_STATE "dirty")
    set(CHIPPER_DIRTY_SUFFIX "-dirty")
endif()

string(TIMESTAMP CHIPPER_BUILT_AT_UTC "%Y-%m-%d %H:%M:%S UTC" UTC)
set(CHIPPER_BUILD_LABEL "v${CHIPPER_VERSION} ${CHIPPER_GIT_SHA}${CHIPPER_DIRTY_SUFFIX}")

file(WRITE "${OUTPUT_FILE}" "#pragma once\n\nnamespace chipper::build\n{\ninline constexpr auto version = \"${CHIPPER_VERSION}\";\ninline constexpr auto gitSha = \"${CHIPPER_GIT_SHA}\";\ninline constexpr auto gitState = \"${CHIPPER_GIT_STATE}\";\ninline constexpr auto builtAtUtc = \"${CHIPPER_BUILT_AT_UTC}\";\ninline constexpr auto label = \"${CHIPPER_BUILD_LABEL}\";\n}\n")
