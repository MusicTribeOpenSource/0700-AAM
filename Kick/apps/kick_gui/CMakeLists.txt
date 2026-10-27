cmake_minimum_required(VERSION 3.15)

## Read version information from the file...
file(READ "version.txt" ver)

## Parse version info, and read variables...
string(REGEX MATCH "VERSION_MAJOR ([0-9]*)" _ ${ver})
set(PROJECT_VERSION_MAJOR ${CMAKE_MATCH_1})

string(REGEX MATCH "VERSION_MINOR ([0-9]*)" _ ${ver})
set(PROJECT_VERSION_MINOR ${CMAKE_MATCH_1})

string(REGEX MATCH "VERSION_PATCH ([0-9]*)" _ ${ver})
set(PROJECT_VERSION_PATCH ${CMAKE_MATCH_1})

## Setup target configuration...
set(TargetName     KickUserInterface)
set(TargetCompany  MusicTribe)
set(TargetVersion  ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH})

project(${TargetName} VERSION ${TargetVersion})

## Assets Directory
file(COPY "${CMAKE_SOURCE_DIR}/assets" DESTINATION "${CMAKE_BINARY_DIR}")
set(TargetSrc_DIR ".")
#message( "## Assets Directory: ${TargetSrc_DIR}/assets")
#message( "## ${PROJECT_BINARY_DIR}" )

## Templates Directory
file(COPY "${CMAKE_SOURCE_DIR}/templates" DESTINATION "${CMAKE_BINARY_DIR}")
set(TargetSrc_DIR ".")

## Demos Directory
file(COPY "${CMAKE_SOURCE_DIR}/demos" DESTINATION "${CMAKE_BINARY_DIR}")
set(TargetSrc_DIR ".")


## Generate configuration file...
configure_file(${CMAKE_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)
#message( "## Config file directory: ${CMAKE_CURRENT_BINARY_DIR}" )

message( "## Project: ${TargetName}" ) 
message( "## Version: ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}" )

set(CMAKE_INCLUDE_CURRENT_DIR ON)

juce_add_gui_app(${TargetName} 
        PRODUCT_NAME ${TargetName}
        COMPANY_NAME ${TargetCompany}
        REQUIRES_FULL_SCREEN TRUE
)

set_target_properties(${TargetName} PROPERTIES 
        CXX_STANDARD                    17 
        VS_DEBUGGER_WORKING_DIRECTORY   "${CMAKE_CURRENT_SOURCE_DIR}"
)
if(MSVC OR APPLE)
# Get Targets in ${CMAKE_CURRENT_SOURCE_DIR}
get_property(current_targets DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY BUILDSYSTEM_TARGETS)
foreach(TARGET ${current_targets})
    set_target_properties(${TARGET} PROPERTIES
                        RUNTIME_OUTPUT_DIRECTORY  ${CMAKE_BINARY_DIR}/$<0:> 
                        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<0:>
                        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<0:>
    )
endforeach()
endif()


target_include_directories(${TargetName}
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
)

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS "*.cpp" "*.h" )

target_sources(${TargetName} 
    PRIVATE
        ${SRC_FILES}
        ${CMAKE_SOURCE_DIR}/juce.natvis
)

target_compile_definitions(${TargetName} 
    PRIVATE
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:${TargetName},JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:${TargetName},JUCE_VERSION>"
        JUCE_ALSA=1
        JUCE_JACK=1

        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCER_ENABLE_GPL_MODE=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_ASIO=0
        JUCE_ENABLE_REPAINT_DEBUGGING=0

        JUCE_MODAL_LOOPS_PERMITTED=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1

        JUCE_PLUGINHOST_LADSPA=0
        JUCE_PLUGINHOST_VST=0
        JUCE_PLUGINHOST_VST3=${USE_VST}
        JUCE_PLUGINHOST_AU=${USE_AU}

        THPAR_SQLITE=1
        USE_OPTICK=${USE_OPTICK_VAL}
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_link_libraries(${TargetName}
      PRIVATE
      "-latomic"
      )
  #  target_link_options(${TargetName} PRIVATE "-m64")
endif()

target_link_libraries(${TargetName} 
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags

    PRIVATE
        kick_ui
        program_edit_ui
        juce::juce_gui_extra
        ipc
		contracts
)

if(TIMESTAMP)
target_link_libraries(${TargetName} PRIVATE timestamp)
endif()

## CPack variables...
set(CPACK_PACKAGE_NAME ${PROJECT_NAME} )
set(CPACK_PACKAGE_VENDOR "MusicTribe Innovation Pvt. Ltd.")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})

## Create Install package...
install(
    TARGETS ${TargetName}
    EXPORT "${TargetName}Targets" DESTINATION "."
    COMPONENT ${TargetName} # must be here, not any line lower
)
## Add resource directory...
install(
    DIRECTORY "${CMAKE_SOURCE_DIR}/assets" DESTINATION "."
    PATTERN "${CMAKE_SOURCE_DIR}/assets/*"
)

install(
    DIRECTORY "${CMAKE_SOURCE_DIR}/templates" DESTINATION "."
    PATTERN "${CMAKE_SOURCE_DIR}/templates/*"
)

install(
    DIRECTORY "${CMAKE_SOURCE_DIR}/demos" DESTINATION "."
    PATTERN "${CMAKE_SOURCE_DIR}/demos/*"
)

# Install the isolated CLAP scan worker beside the host executable.
kick_bundle_clap_scanner(${TargetName})
