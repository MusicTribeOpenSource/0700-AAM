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

include(FetchContent)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
    URL_HASH SHA256=7b42b4d6ed48810c5362c265a17faebe90dc2373c885e5216439d37927f02926
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

function(kick_gesture_test name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE kick_gesture_core GTest::gtest_main)
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../../kick_camera/tests)
    gtest_discover_tests(${name} DISCOVERY_TIMEOUT 30)
endfunction()

kick_gesture_test(json_test json_test.cpp)
kick_gesture_test(config_test config_test.cpp)
kick_gesture_test(engine_test engine_test.cpp)
kick_gesture_test(image_test image_test.cpp)
kick_gesture_test(guard_test guard_test.cpp)
kick_gesture_test(record_test record_test.cpp)
kick_gesture_test(sink_test sink_test.cpp)
kick_gesture_test(service_test service_test.cpp)
target_compile_definitions(config_test PRIVATE KICK_GESTURE_SHIPPED_JSON="${CMAKE_CURRENT_SOURCE_DIR}/../systemd/gesture.json")

if(KICK_GESTURE_ONNX_AVAILABLE)
    add_executable(onnx_model_test onnx_model_test.cpp)
    target_link_libraries(onnx_model_test PRIVATE kick_gesture_core GTest::gtest_main)
    target_include_directories(onnx_model_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../../kick_camera/tests)
    # The photo tests read PPM files. A fixture converts the fetched JPEGs when OpenCV or Pillow is there.
    gtest_discover_tests(onnx_model_test DISCOVERY_TIMEOUT 30 PROPERTIES FIXTURES_REQUIRED kick_gesture_test_images)
    target_compile_definitions(onnx_model_test PRIVATE
        KICK_GESTURE_TEST_MODEL_DIR="${KICK_GESTURE_MODEL_DIR}"
        KICK_GESTURE_TEST_IMG_DIR="${KICK_GESTURE_TESTIMG_DIR}"
        KICK_GESTURE_REFERENCE_JSON="${CMAKE_CURRENT_SOURCE_DIR}/../testdata/reference_landmarks.json")
endif()

# Python tools and the command line. These need python3 and the two binaries, not the model.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    set(_kg_tools "${CMAKE_CURRENT_SOURCE_DIR}/../tools")
    set(_kg_docs "${CMAKE_CURRENT_SOURCE_DIR}/../docs")

    add_test(NAME cli_test
        COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_CURRENT_SOURCE_DIR}/cli_test.py
            $<TARGET_FILE:kick-gesture> $<TARGET_FILE:kick-gesture-synth>
            ${_kg_tools}/validate_events.py ${_kg_docs}/gesture-event.schema.json
            ${CMAKE_CURRENT_SOURCE_DIR}/../systemd/gesture.json)

    add_test(NAME gesture_eval_unit
        COMMAND ${Python3_EXECUTABLE} -I ${_kg_tools}/gesture-eval/test_eval.py)

    # Generates the synthetic eval set, runs the real binary over it, and applies the 95 % / 1 per 10 min floors.
    add_test(NAME gesture_eval_synthetic
        COMMAND ${CMAKE_COMMAND}
            -DSYNTH=$<TARGET_FILE:kick-gesture-synth> -DGESTURE=$<TARGET_FILE:kick-gesture>
            -DEVAL=${_kg_tools}/gesture-eval/gesture_eval.py -DPYTHON=${Python3_EXECUTABLE}
            -DOUT=${CMAKE_CURRENT_BINARY_DIR}/eval-set
            -P ${CMAKE_CURRENT_SOURCE_DIR}/run_eval.cmake)

    if(KICK_GESTURE_ONNX_AVAILABLE)
        add_test(NAME prepare_test_images
            COMMAND ${Python3_EXECUTABLE} -I ${_kg_tools}/prepare_test_images.py --if-possible ${KICK_GESTURE_TESTIMG_DIR})
        set_tests_properties(prepare_test_images PROPERTIES FIXTURES_SETUP kick_gesture_test_images)
    endif()

    add_test(NAME pins_match_docs
        COMMAND ${Python3_EXECUTABLE} -I ${_kg_tools}/check_pins.py)
else()
    message(WARNING "kick-gesture: python3 not found, the CLI, eval and pin tests are not registered")
endif()
