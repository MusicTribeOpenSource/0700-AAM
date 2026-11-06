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
include(FetchContent)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
    URL_HASH SHA256=7b42b4d6ed48810c5362c265a17faebe90dc2373c885e5216439d37927f02926
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

function(kick_camera_test name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE kick_camera_core GTest::gtest_main)
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    gtest_discover_tests(${name} DISCOVERY_TIMEOUT 30)
endfunction()

kick_camera_test(ring_test ring_test.cpp)
kick_camera_test(sources_test sources_test.cpp)
kick_camera_test(controls_test controls_test.cpp)
kick_camera_test(config_test config_test.cpp)
kick_camera_test(service_test service_test.cpp)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    kick_camera_test(v4l2_test v4l2_test.cpp)
endif()

# The real binary, driven from a shell: command line, control socket, a reader in another process, clean shutdown.
add_test(NAME cli_smoke COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cli_smoke.sh $<TARGET_FILE:kick-camera> $<TARGET_FILE:kick-camera-peek>)
