# Copyright (c) 2024 MUSIC Tribe
#
# This file is part of Kick.
#
# Kick is free software: you can redistribute it and/or modify it under the
# terms of the GNU Affero General Public License as published by the Free
# Software Foundation, either version 3 of the License, or (at your option)
# any later version.
#
# Kick is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
# more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with Kick. If not, see <https://www.gnu.org/licenses/>.
#
# SPDX-License-Identifier: AGPL-3.0-or-later

# Unit tests for the gesture mapping core: no JUCE, no app. Run:
#   cmake -S Kick/modules/gesture/tests -B build/gesture-core && cmake --build build/gesture-core && ctest --test-dir build/gesture-core
cmake_minimum_required(VERSION 3.16)
project(kick_gesture_core_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug)
endif()
set(KICK_GESTURE_CORE_SANITIZE "" CACHE STRING "address, undefined, or empty")

include(FetchContent)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
    URL_HASH SHA256=7b42b4d6ed48810c5362c265a17faebe90dc2373c885e5216439d37927f02926
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

set(CORE ${CMAKE_CURRENT_SOURCE_DIR}/../core)
add_library(kick_gesture_app_core STATIC
    ${CORE}/gesture_mapping.cpp
    ${CORE}/gesture_smoother.cpp
    ${CORE}/gesture_gate.cpp
    ${CORE}/gesture_router.cpp
    ${CORE}/camera_status.cpp)
# Includes of the form "contracts/gesture/gesture_event.h" resolve against Kick/modules.
target_include_directories(kick_gesture_app_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../.. ${CORE})
target_compile_options(kick_gesture_app_core PUBLIC -Wall -Wextra)
if(KICK_GESTURE_CORE_SANITIZE)
    target_compile_options(kick_gesture_app_core PUBLIC -fsanitize=${KICK_GESTURE_CORE_SANITIZE} -fno-omit-frame-pointer)
    target_link_options(kick_gesture_app_core PUBLIC -fsanitize=${KICK_GESTURE_CORE_SANITIZE})
endif()

enable_testing()
function(core_test name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE kick_gesture_app_core GTest::gtest_main)
    gtest_discover_tests(${name} DISCOVERY_TIMEOUT 30)
endfunction()

core_test(mapping_test mapping_test.cpp)
core_test(smoother_test smoother_test.cpp)
core_test(gate_test gate_test.cpp)
core_test(router_test router_test.cpp)
core_test(backoff_test backoff_test.cpp)
core_test(camera_status_test camera_status_test.cpp)
