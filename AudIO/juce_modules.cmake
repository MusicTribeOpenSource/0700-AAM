# SPDX-License-Identifier: AGPL-3.0-or-later
# Integrate the redistributable engine without building its example applications.
juce_add_modules("${CMAKE_CURRENT_LIST_DIR}/juce_modules/AudIO_constants"
                 "${CMAKE_CURRENT_LIST_DIR}/juce_modules/AudIO_juce")
target_include_directories(AudIO_constants INTERFACE "${CMAKE_CURRENT_LIST_DIR}")

add_library(kick_audio_core STATIC "${CMAKE_CURRENT_LIST_DIR}/src/audio_core.cpp")
target_compile_features(kick_audio_core PUBLIC cxx_std_17)
target_include_directories(kick_audio_core PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
set_target_properties(kick_audio_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
find_package(Threads REQUIRED)
target_link_libraries(kick_audio_core PUBLIC Threads::Threads)
add_library(kick_audio_juce STATIC "${CMAKE_CURRENT_LIST_DIR}/src/juce_adapter.cpp")
target_include_directories(kick_audio_juce PUBLIC "${CMAKE_CURRENT_LIST_DIR}/juce_modules")
target_link_libraries(kick_audio_juce PUBLIC kick_audio_core AudIO_constants juce::juce_audio_utils juce::juce_audio_formats juce::juce_dsp)
target_compile_definitions(kick_audio_juce PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
set_target_properties(kick_audio_juce PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(AudIO_juce INTERFACE kick_audio_juce)
if(COMMAND _juce_fixup_module_source_groups)
    _juce_fixup_module_source_groups()
endif()
