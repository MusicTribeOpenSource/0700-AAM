# Copyright (c) 2024 MUSIC Tribe. SPDX-License-Identifier: AGPL-3.0-or-later (see COPYING at the repo root).
cmake_minimum_required(VERSION 3.22)

# Deployment target before project(), see ~/.claude/rules/macos-codesigning.md.
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version" FORCE)
endif()

project(GestureIpcTest VERSION 0.1.0 LANGUAGES C CXX)

# Proves the real pieces fit: kick-gesture's LoopbackIpcSink on one side, the app's JUCE IPC server and
# GestureIpcServer on the other. No UI, no engine. Not part of emu/build.sh; run it with run-gesture-ipc-test.sh.
get_filename_component(REPO "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(KICK_JUCE_DIR "" CACHE PATH "JUCE source tree (7.0.12)")
if(NOT KICK_JUCE_DIR)
    file(GLOB _juce_candidates "${REPO}/.3rdPartySourceCache/juce/*")
    list(GET _juce_candidates 0 KICK_JUCE_DIR)
endif()
if(NOT EXISTS "${KICK_JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE not found. Run emu/build.sh first or pass -DKICK_JUCE_DIR=<path>.")
endif()
add_subdirectory(${KICK_JUCE_DIR} juce)

juce_add_module(${REPO}/Kick/modules/ipc)

juce_add_console_app(GestureIpcTest PRODUCT_NAME "gesture-ipc-test")
target_sources(GestureIpcTest PRIVATE
    main.cpp
    ${REPO}/Kick/modules/gesture/ipc/gesture_decode.cpp
    ${REPO}/Kick/modules/gesture/ipc/gesture_ipc_server.cpp
    ${REPO}/Kick/modules/contracts/gesture/gesture_signals.cpp
    ${REPO}/Kick/services/kick_gesture/src/sink.cpp
    ${REPO}/Kick/services/kick_camera/src/types.cpp)
target_include_directories(GestureIpcTest PRIVATE
    ${REPO}/Kick/modules
    ${REPO}/Kick/modules/gesture/core
    ${REPO}/Kick/services/kick_gesture/include
    ${REPO}/Kick/services/kick_camera/include)
target_compile_definitions(GestureIpcTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(GestureIpcTest PRIVATE
    ipc
    juce::juce_events
    juce::juce_recommended_config_flags)
set_target_properties(GestureIpcTest PROPERTIES CXX_STANDARD 17)
