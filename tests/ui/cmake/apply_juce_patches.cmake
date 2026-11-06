# Applies the patches Kick needs to a fresh JUCE 7.0.12 checkout.
#   cmake -DJUCE_DIR=<dir> -DKICK_ROOT=<repo> -DTESTS_UI=<tests/ui> -P apply_juce_patches.cmake
#
# Every patch is mandatory: a half-patched JUCE can compile and misbehave, so any
# failure stops configuration. The flag file is written only after all patches land.
set(flag "${JUCE_DIR}/.kick_patches_applied")
if(EXISTS "${flag}")
    return()
endif()

function(kick_apply_patch patch)
    execute_process(
        COMMAND git apply --ignore-whitespace "${patch}"
        WORKING_DIRECTORY "${JUCE_DIR}"
        RESULT_VARIABLE rc
        ERROR_VARIABLE err)
    if(rc)
        message(FATAL_ERROR "Failed to apply ${patch} to JUCE in ${JUCE_DIR}:\n${err}")
    endif()
endfunction()

set(patches "${KICK_ROOT}/Kick/patches/juce_multitouch_7.0.12.patch")
if(APPLE)
    # The macOS 15+ SDK removed CGWindowListCreateImage, which JUCE 7.0.12 still calls.
    list(APPEND patches "${TESTS_UI}/patches/juce_7.0.12_macos15_sdk.patch")
endif()

foreach(p IN LISTS patches)
    kick_apply_patch("${p}")
endforeach()
file(WRITE "${flag}" "patched")
