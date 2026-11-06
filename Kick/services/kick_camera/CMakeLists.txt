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
# /
cmake_minimum_required(VERSION 3.16)
project(kick_camera VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Standalone on purpose: no JUCE, so the service adds little to the image.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(_kick_camera_top ON)
else()
    set(_kick_camera_top OFF)
endif()
option(KICK_CAMERA_BUILD_TESTS "Build the gtest suite (fetches googletest)" ${_kick_camera_top})
set(KICK_CAMERA_SANITIZE "" CACHE STRING "thread, address or empty")

find_package(Threads REQUIRED)

set(KICK_CAMERA_SOURCES
    src/config.cpp
    src/control_server.cpp
    src/controls.cpp
    src/service.cpp
    src/shm_region.cpp
    src/shm_ring.cpp
    src/sources.cpp
    src/types.cpp)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND KICK_CAMERA_SOURCES src/v4l2_source.cpp)
endif()

add_library(kick_camera_core STATIC ${KICK_CAMERA_SOURCES})
target_include_directories(kick_camera_core PUBLIC include)
target_compile_options(kick_camera_core PUBLIC -Wall -Wextra)
target_link_libraries(kick_camera_core PUBLIC Threads::Threads)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(kick_camera_core PUBLIC rt) # shm_open on older glibc
endif()
if(KICK_CAMERA_SANITIZE)
    target_compile_options(kick_camera_core PUBLIC -fsanitize=${KICK_CAMERA_SANITIZE} -fno-omit-frame-pointer -g)
    target_link_options(kick_camera_core PUBLIC -fsanitize=${KICK_CAMERA_SANITIZE})
endif()

add_executable(kick-camera src/main.cpp)
target_link_libraries(kick-camera PRIVATE kick_camera_core)

# Reads a frame from the ring in another process and writes it as a PGM. Not installed on the unit.
add_executable(kick-camera-peek tools/peek.cpp)
target_link_libraries(kick-camera-peek PRIVATE kick_camera_core)

include(GNUInstallDirs)
install(TARGETS kick-camera RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(KICK_CAMERA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
