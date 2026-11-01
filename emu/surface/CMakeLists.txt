cmake_minimum_required(VERSION 3.22)

# Deployment target has to be set before project(), see ~/.claude/rules/macos-codesigning.md.
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version" FORCE)
endif()

project(KickSurface VERSION 0.1.0)

# Reuse the JUCE checkout the Kick build already fetched (it carries the macOS 15 SDK patch).
# Pass -DKICK_JUCE_DIR=<path> to use another one.
set(KICK_JUCE_DIR "" CACHE PATH "JUCE source tree (7.0.12)")
if(NOT KICK_JUCE_DIR)
    file(GLOB _juce_candidates "${CMAKE_CURRENT_LIST_DIR}/../../.3rdPartySourceCache/juce/*")
    list(GET _juce_candidates 0 KICK_JUCE_DIR)
endif()
if(NOT EXISTS "${KICK_JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE not found. Run emu/build.sh (builds Kick first) or pass -DKICK_JUCE_DIR=<path>.")
endif()
add_subdirectory(${KICK_JUCE_DIR} juce)

juce_add_gui_app(KickSurface
    PRODUCT_NAME "KICK Surface"
    COMPANY_NAME "Bombest"
)

target_sources(KickSurface PRIVATE Main.cpp)

target_compile_definitions(KickSurface PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="KICK Surface"
    JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}"
)

target_link_libraries(KickSurface PRIVATE
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
)

set_target_properties(KickSurface PROPERTIES CXX_STANDARD 17)
