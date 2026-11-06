# SPDX-License-Identifier: AGPL-3.0-or-later
include(FetchContent)

# Same pin as the repo's other gtest tree, so the two can merge later.
# Pass -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=<path> to build offline.
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

get_filename_component(KICK_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)

add_executable(kick_mcu_tests
    alloc_hook.cpp
    frame_codec_test.cpp
    rx_parser_test.cpp
    tx_queue_test.cpp
    session_test.cpp
    link_test.cpp
    storm_test.cpp
    trace_replay_test.cpp
    no_alloc_test.cpp
    debounce_test.cpp
    velocity_curve_test.cpp
    pads_test.cpp
    encoders_test.cpp
    strip_test.cpp
    scan_loop_test.cpp
    latency_test.cpp
    commands_test.cpp
    led_state_test.cpp
    colours_test.cpp
    blink_test.cpp
    current_budget_test.cpp
    led_renderer_test.cpp
    leds_firmware_test.cpp
    system_basics_test.cpp
    updater_test.cpp
    system_test.cpp
)
target_link_libraries(kick_mcu_tests PRIVATE kick_mcu_bsp_sim GTest::gtest_main)
if(TARGET kick_mcu_sim_lib)
    target_sources(kick_mcu_tests PRIVATE script_player_test.cpp led_replay_test.cpp)
    target_link_libraries(kick_mcu_tests PRIVATE kick_mcu_sim_lib)
endif()
target_compile_options(kick_mcu_tests PRIVATE ${KICK_MCU_WARNINGS})
target_compile_definitions(kick_mcu_tests PRIVATE
    KICK_TRACE_DIR="${KICK_REPO_ROOT}/emu/tests/traces"
    KICK_STAMPED_HEX="${CMAKE_CURRENT_BINARY_DIR}/payload-stamped.hex"
)

# The image tool's output, produced at build time so the C++ test checks what the tool really writes.
# Without Python the image tool tests are left out.
find_package(Python3 COMPONENTS Interpreter)
set(KICK_IMAGE_TOOL "${KICK_REPO_ROOT}/tools/mcu-image-header.py")
if(Python3_Interpreter_FOUND)
    add_custom_command(
        OUTPUT payload-stamped.hex
        COMMAND ${Python3_EXECUTABLE} ${KICK_IMAGE_TOOL} stamp --hex ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/payload.hex
                -o ${CMAKE_CURRENT_BINARY_DIR}/payload-stamped.hex --version 3.1.4 --revision 15
        DEPENDS ${KICK_IMAGE_TOOL} ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/payload.hex
        COMMENT "Stamping the test image with tools/mcu-image-header.py"
        VERBATIM
    )
    add_custom_target(kick_mcu_stamped_image DEPENDS payload-stamped.hex)
    add_dependencies(kick_mcu_tests kick_mcu_stamped_image)
    target_sources(kick_mcu_tests PRIVATE image_tool_test.cpp)
    add_test(NAME ImageToolPython COMMAND ${Python3_EXECUTABLE} ${KICK_REPO_ROOT}/tools/test_mcu_image_header.py)
else()
    message(STATUS "Python3 not found: the image tool tests are not built")
endif()

# The contract checks compile against the real host headers. dcl_contracts.h pulls in JUCE only
# for a type it does not use, so a stub juce_core.h keeps JUCE out of the build.
add_executable(kick_mcu_contract_tests
    contract_test.cpp
    pktmgr_crosscheck_test.cpp
    system_contract_test.cpp
    ${KICK_REPO_ROOT}/ComProtocol/PktMgr/PktMgr.cpp
)
target_include_directories(kick_mcu_contract_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../sim
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
    ${KICK_REPO_ROOT}/Kick/modules/contracts/dcl
    ${KICK_REPO_ROOT}/ComProtocol/PktMgr
    ${KICK_REPO_ROOT}/Kick/modules/input_events
    ${KICK_REPO_ROOT}/Kick/modules/hardware/source
)
target_link_libraries(kick_mcu_contract_tests PRIVATE kick_mcu_bsp_sim GTest::gtest_main)
# PktMgr.cpp is host code, not ours: keep its warnings out of the log.
set_source_files_properties(${KICK_REPO_ROOT}/ComProtocol/PktMgr/PktMgr.cpp PROPERTIES COMPILE_OPTIONS "-w")
target_compile_options(kick_mcu_contract_tests PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-unused-parameter>)

# The host updater harness builds JUCE's core module, so it is opt-in. See host_updater/CMakeLists.txt.
option(KICK_MCU_HOST_UPDATER "Build the harness that runs the host's real MCU updater against the firmware" OFF)
if(KICK_MCU_HOST_UPDATER AND TARGET kick_mcu_sim_lib)
    add_subdirectory(host_updater)
endif()

include(GoogleTest)
gtest_discover_tests(kick_mcu_tests DISCOVERY_MODE PRE_TEST)
gtest_discover_tests(kick_mcu_contract_tests DISCOVERY_MODE PRE_TEST)
