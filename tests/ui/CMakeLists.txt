# JUCE-backed UI tests: real Kick UI controls, headless (no window server needed for
# off-screen rendering and synthesized input).
#
# Two modules the public release omits (AudIO_constants, PktMgr's JUCE module header)
# are provided as stand-ins under stubs/. JUCE 7.0.12 plus Kick's multitouch patch is
# fetched on first configure; pass -DFETCHCONTENT_SOURCE_DIR_JUCE=<dir> to reuse a
# checkout that is already patched.

set(KICK_ROOT ${KICK_REPO_ROOT})

FetchContent_Declare(juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.12
    GIT_SHALLOW    TRUE
    UPDATE_DISCONNECTED TRUE
    PATCH_COMMAND  ${CMAKE_COMMAND}
        -DJUCE_DIR=<SOURCE_DIR> -DKICK_ROOT=${KICK_ROOT} -DTESTS_UI=${CMAKE_CURRENT_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/apply_juce_patches.cmake)
FetchContent_MakeAvailable(juce)

# Generated config.h (project/version info) that Kick's `common` module includes.
set(TargetName     KickUiTests)
set(TargetCompany  Bombest)
set(TargetSrc_DIR  ".")
set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_VERSION_MINOR 0)
set(PROJECT_VERSION_PATCH 0)
configure_file(${KICK_ROOT}/Kick/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/gen/config.h)

juce_add_modules(
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs/AudIO_constants
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs/PktMgr
    ${KICK_ROOT}/Kick/modules/3rdParty/fpm
    ${KICK_ROOT}/Kick/modules/3rdParty/magic_enum
    ${KICK_ROOT}/Kick/modules/3rdParty/signal
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs/juce_serialport
    ${KICK_ROOT}/Kick/modules/common
    ${KICK_ROOT}/Kick/modules/contracts
    ${KICK_ROOT}/Kick/modules/input_events
    ${KICK_ROOT}/Kick/modules/ipc
    ${KICK_ROOT}/Kick/modules/kick_ui_common
    ${KICK_ROOT}/Kick/modules/kick_ui_controls)

function(kick_add_ui_exe name label)
    juce_add_console_app(${name} PRODUCT_NAME "${name}" VERSION 0.1.0)
    target_sources(${name} PRIVATE ui_test_main.cpp ${ARGN})
    target_include_directories(${name} PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/gen
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${KICK_ROOT}/Kick/modules
        ${KICK_ROOT}/Kick/modules/3rdParty
        ${KICK_ROOT}/Kick
        ${KICK_ROOT})
    target_compile_definitions(${name} PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_USE_MP3AUDIOFORMAT=0 JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(${name} PRIVATE
        kick_ui_controls
        contracts
        ipc
        kick_pktmgr
        kick_test_support
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
        GTest::gtest GTest::gmock)
    set_target_properties(${name} PROPERTIES CXX_STANDARD 17)
    gtest_discover_tests(${name}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        DISCOVERY_TIMEOUT 120
        PROPERTIES LABELS "${label}")
endfunction()

# Kick resolves assets/ relative to the working directory on macOS (and /app/assets on
# Linux, so the UI tier is wired for macOS runs). Give the tests a cwd that holds a copy.
file(COPY ${KICK_ROOT}/Kick/assets DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

kick_add_ui_exe(KickUiTests  ui       controls_test.cpp)
kick_add_ui_exe(KickUiE2E    e2e      e2e_hardware_test.cpp)
