cmake_minimum_required(VERSION 3.15)

set(TargetName KT_KickEM)
set(TargetVersion 0.2.0)
set(TargetCompany MUSICTribe)


project(${TargetName} VERSION ${TargetVersion})
set(CMAKE_INCLUDE_CURRENT_DIR ON)

juce_add_gui_app(${TargetName} 
        PRODUCT_NAME ${TargetName}
        COMPANY_NAME ${TargetCompany}
)

juce_generate_juce_header(${TargetName})
set_target_properties(${TargetName} PROPERTIES 
        CXX_STANDARD                    17 
        VS_DEBUGGER_WORKING_DIRECTORY   "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_include_directories(${TargetName}
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
)

target_sources(${TargetName} 
    PRIVATE
        simulator/kick_SimulatorInterfaceImpl.h
        simulator/kick_SimulatorControls.h
        simulator/kick_Simulator.h
        simulator/kick_SimulatorMainControl.h
        simulator/kick_Simulator.cpp
        simulator/kick_SimulatorMainControl.cpp
        simulator/kick_SimulatorControls.cpp
        Main.cpp
        simulator/kick_SimulatorInterfaceImpl.cpp)

target_compile_definitions(${TargetName} 
    PRIVATE
        SIM_BUILD=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:${TargetName},JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:${TargetName},JUCE_VERSION>"

        JUCER_ENABLE_GPL_MODE=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_DISPLAY_SPLASH_SCREEN=0

        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_STRICT_REFCOUNTEDPOINTER=1

        JUCE_ASIO=0

        JUCE_PLUGINHOST_LADSPA=0
        JUCE_PLUGINHOST_VST=0
        JUCE_PLUGINHOST_VST3=${USE_VST}
        JUCE_PLUGINHOST_AU=${USE_AU}

        JUCE_ENABLE_REPAINT_DEBUGGING=0
        THPAR_SQLITE=1
        USE_OPTICK=${USE_OPTICK_VAL}
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_link_libraries(${TargetName} PRIVATE "-latomic")
  target_link_options(${TargetName} PRIVATE "-m64")
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
        juce::juce_audio_devices
        juce::juce_audio_processors
        juce::juce_audio_utils
)

option( USE_ASAN_KICK "enable address sanitizer" OFF )

if(USE_ASAN_KICK)
   target_link_libraries(${TargetName} PRIVATE libsancov.lib)
   target_link_options(${TargetName}  PRIVATE /fsanitize=address)
   target_compile_options(${TargetName} PRIVATE -fsanitize=address)
   target_compile_options(${TargetName}  PRIVATE -fsanitize-coverage=inline-8bit-counters)
   target_compile_options(${TargetName}  PRIVATE /fsanitize=address)
   target_compile_options(${TargetName}  PRIVATE /Od)
   set(addressSanitizerEnabled true)
endif()

# Install the isolated CLAP scan worker beside the host executable.
kick_bundle_clap_scanner(${TargetName})
