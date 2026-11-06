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

cmake_minimum_required(VERSION 3.16)
project(kick_gesture VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Standalone on purpose: no JUCE, so the service adds little to the image.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(_kick_gesture_top ON)
else()
    set(_kick_gesture_top OFF)
endif()
option(KICK_GESTURE_BUILD_TESTS "Build the gtest suite (fetches googletest)" ${_kick_gesture_top})
option(KICK_GESTURE_WITH_ONNX "Build the ONNX Runtime hand model backend" ON)
option(KICK_GESTURE_FETCH "Download ONNX Runtime and the model files at configure time" ON)
set(KICK_GESTURE_ORT_ROOT "" CACHE PATH "Unpacked ONNX Runtime release (include/ and lib/). Skips the download when set.")
set(KICK_GESTURE_MODEL_DIR "" CACHE PATH "Directory holding the model files. Skips the download when set.")
set(KICK_GESTURE_DOWNLOAD_CACHE "$ENV{KICK_GESTURE_DOWNLOAD_CACHE}" CACHE PATH "Where downloads are kept between build trees")
set(KICK_GESTURE_SANITIZE "" CACHE STRING "thread, address or empty")

include(GNUInstallDirs)
find_package(Threads REQUIRED)

# The service reads frames through the camera service's ring reader. Added as a subdirectory so one build
# covers both. Its tests and install rules stay out of this build.
set(KICK_CAMERA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../kick_camera ${CMAKE_BINARY_DIR}/kick_camera EXCLUDE_FROM_ALL)

include(cmake/FetchDeps.cmake)

set(KICK_GESTURE_SOURCES
    src/config.cpp
    src/engine.cpp
    src/event_json.cpp
    src/features.cpp
    src/frame_reader.cpp
    src/guard.cpp
    src/image.cpp
    src/json.cpp
    src/palm_anchors.cpp
    src/record.cpp
    src/replay.cpp
    src/service.cpp
    src/service_config.cpp
    src/sink.cpp
    src/synth.cpp
    src/types.cpp)
if(KICK_GESTURE_ONNX_AVAILABLE)
    list(APPEND KICK_GESTURE_SOURCES src/onnx_hand_model.cpp src/bench.cpp)
endif()

add_library(kick_gesture_core STATIC ${KICK_GESTURE_SOURCES})
# The reconnect backoff is shared with the Kick app side and tested there (Kick/modules/gesture/tests).
target_include_directories(kick_gesture_core PUBLIC include ${CMAKE_CURRENT_SOURCE_DIR}/../../modules/gesture/core)
target_compile_options(kick_gesture_core PUBLIC -Wall -Wextra)
target_link_libraries(kick_gesture_core PUBLIC kick_camera_core Threads::Threads)
if(KICK_GESTURE_ONNX_AVAILABLE)
    target_link_libraries(kick_gesture_core PUBLIC onnxruntime::onnxruntime)
    target_compile_definitions(kick_gesture_core PUBLIC KICK_GESTURE_HAVE_ONNX=1)
    # Find libonnxruntime next to the binary's ../lib on the unit, and in the download directory on a dev machine.
    set(CMAKE_BUILD_RPATH "${KICK_GESTURE_ORT_LIB_DIR}")
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
endif()
if(KICK_GESTURE_SANITIZE)
    target_compile_options(kick_gesture_core PUBLIC -fsanitize=${KICK_GESTURE_SANITIZE} -fno-omit-frame-pointer -g)
    target_link_options(kick_gesture_core PUBLIC -fsanitize=${KICK_GESTURE_SANITIZE})
endif()

add_executable(kick-gesture src/main.cpp)
target_link_libraries(kick-gesture PRIVATE kick_gesture_core)
target_compile_definitions(kick-gesture PRIVATE KICK_GESTURE_DEFAULT_MODEL_DIR="${CMAKE_INSTALL_FULL_DATADIR}/kick-gesture/models")

# Writes synthetic landmark streams for the demo, the eval example and the tests. Not installed on the unit.
add_executable(kick-gesture-synth tools/synth.cpp)
target_link_libraries(kick-gesture-synth PRIVATE kick_gesture_core)

install(TARGETS kick-gesture RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(KICK_GESTURE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
