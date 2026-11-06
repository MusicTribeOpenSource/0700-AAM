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

# Downloads ONNX Runtime and the model files at configure time, checks every file against cmake/pins.cmake, and
# sets:
#   KICK_GESTURE_ONNX_AVAILABLE   TRUE when the runtime and its headers are usable
#   KICK_GESTURE_MODELS_AVAILABLE TRUE when all model files are in KICK_GESTURE_MODEL_DIR
#   KICK_GESTURE_MODEL_DIR        where the model files are
#   target onnxruntime::onnxruntime
# A blocked network is not an error: the service still builds, without the model backend, and the tests that need
# the models skip themselves.
include(${CMAKE_CURRENT_LIST_DIR}/pins.cmake)

set(KICK_GESTURE_ONNX_AVAILABLE FALSE)
set(KICK_GESTURE_MODELS_AVAILABLE FALSE)

if(KICK_GESTURE_DOWNLOAD_CACHE)
    set(_kg_cache "${KICK_GESTURE_DOWNLOAD_CACHE}")
else()
    set(_kg_cache "${CMAKE_BINARY_DIR}/kick-gesture-downloads")
endif()
file(MAKE_DIRECTORY "${_kg_cache}")

# file(DOWNLOAD) with a hash check. A file that is already there and matches is kept, one that does not is replaced.
function(kick_gesture_download url dest sha256 ok_var)
    set(${ok_var} FALSE PARENT_SCOPE)
    if(EXISTS "${dest}")
        file(SHA256 "${dest}" _have)
        if(_have STREQUAL "${sha256}")
            set(${ok_var} TRUE PARENT_SCOPE)
            return()
        endif()
        file(REMOVE "${dest}")
    endif()
    if(NOT KICK_GESTURE_FETCH)
        return()
    endif()
    file(DOWNLOAD "${url}" "${dest}" EXPECTED_HASH SHA256=${sha256} STATUS _st TLS_VERIFY ON)
    list(GET _st 0 _code)
    if(_code EQUAL 0)
        set(${ok_var} TRUE PARENT_SCOPE)
    else()
        file(REMOVE "${dest}")
        message(WARNING "kick-gesture: could not fetch ${url}: ${_st}")
    endif()
endfunction()

# ---- ONNX Runtime ----
if(KICK_GESTURE_WITH_ONNX)
    if(KICK_GESTURE_ORT_ROOT)
        set(_ort_root "${KICK_GESTURE_ORT_ROOT}")
    else()
        if(CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
            set(_ort_platform "osx-arm64")
        elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
            set(_ort_platform "linux-x64")
        elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
            set(_ort_platform "linux-aarch64")
        else()
            set(_ort_platform "")
            message(WARNING "kick-gesture: no pinned ONNX Runtime for ${CMAKE_SYSTEM_NAME}/${CMAKE_SYSTEM_PROCESSOR}; set KICK_GESTURE_ORT_ROOT")
        endif()
        if(_ort_platform)
            set(_ort_name "onnxruntime-${_ort_platform}-${KICK_GESTURE_ORT_VERSION}")
            set(_ort_archive "${_kg_cache}/${_ort_name}.tgz")
            kick_gesture_download("${KICK_GESTURE_ORT_URL_BASE}/${_ort_name}.tgz" "${_ort_archive}" "${KICK_GESTURE_ORT_SHA256_${_ort_platform}}" _ort_ok)
            if(_ort_ok)
                set(_ort_root "${_kg_cache}/${_ort_name}")
                if(NOT EXISTS "${_ort_root}/include")
                    file(ARCHIVE_EXTRACT INPUT "${_ort_archive}" DESTINATION "${_kg_cache}")
                endif()
            endif()
        endif()
    endif()
    if(_ort_root AND EXISTS "${_ort_root}/include" AND EXISTS "${_ort_root}/lib")
        file(GLOB _ort_libs "${_ort_root}/lib/libonnxruntime.dylib" "${_ort_root}/lib/libonnxruntime.so")
        list(GET _ort_libs 0 _ort_lib)
        if(_ort_lib)
            add_library(onnxruntime::onnxruntime SHARED IMPORTED)
            set_target_properties(onnxruntime::onnxruntime PROPERTIES
                IMPORTED_LOCATION "${_ort_lib}"
                INTERFACE_INCLUDE_DIRECTORIES "${_ort_root}/include")
            if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
                set_target_properties(onnxruntime::onnxruntime PROPERTIES IMPORTED_NO_SONAME FALSE)
            endif()
            set(KICK_GESTURE_ORT_LIB_DIR "${_ort_root}/lib")
            set(KICK_GESTURE_ONNX_AVAILABLE TRUE)
        endif()
    endif()
    if(NOT KICK_GESTURE_ONNX_AVAILABLE)
        message(WARNING "kick-gesture: ONNX Runtime is not available, building without the model backend")
    endif()
endif()

# ---- models ----
if(KICK_GESTURE_MODEL_DIR)
    set(_model_dir "${KICK_GESTURE_MODEL_DIR}")
else()
    set(_model_dir "${_kg_cache}/models")
endif()
file(MAKE_DIRECTORY "${_model_dir}")
set(_models_ok TRUE)
foreach(_m IN LISTS KICK_GESTURE_MODEL_FILES)
    kick_gesture_download("${KICK_GESTURE_ZOO_URL_BASE}/${KICK_GESTURE_MODEL_URL_${_m}}" "${_model_dir}/${_m}" "${KICK_GESTURE_MODEL_SHA256_${_m}}" _one_ok)
    if(NOT _one_ok)
        set(_models_ok FALSE)
    endif()
endforeach()
set(KICK_GESTURE_MODEL_DIR "${_model_dir}")
set(KICK_GESTURE_MODELS_AVAILABLE ${_models_ok})
if(NOT _models_ok)
    message(WARNING "kick-gesture: model files are missing from ${_model_dir}; the model-backed tests will be skipped")
endif()

# ---- test photos (tests only) ----
set(KICK_GESTURE_TESTIMG_DIR "${_kg_cache}/testimg")
set(KICK_GESTURE_TESTIMG_AVAILABLE FALSE)
if(KICK_GESTURE_BUILD_TESTS)
    file(MAKE_DIRECTORY "${KICK_GESTURE_TESTIMG_DIR}")
    set(_img_ok TRUE)
    foreach(_i IN LISTS KICK_GESTURE_TESTIMG_FILES)
        kick_gesture_download("${KICK_GESTURE_TESTIMG_URL_BASE}/${_i}" "${KICK_GESTURE_TESTIMG_DIR}/${_i}" "${KICK_GESTURE_TESTIMG_SHA256_${_i}}" _one_ok)
        if(NOT _one_ok)
            set(_img_ok FALSE)
        endif()
    endforeach()
    set(KICK_GESTURE_TESTIMG_AVAILABLE ${_img_ok})
endif()
