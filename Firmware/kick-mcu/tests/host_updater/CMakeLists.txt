# Host updater harness: the host's real kick_FirmwareUpdate.cpp against the firmware update path.
# Off by default because it builds JUCE's core module. Turn it on with
#   -DKICK_MCU_HOST_UPDATER=ON -DKICK_JUCE_DIR=<JUCE checkout>
# (the repo caches one at .3rdPartySourceCache/juce/<hash>).
#
# Verified on macOS. The host file's Linux-only code (image scan, USB gadget) is excluded by its
# own #if on other platforms; a Linux build is untested.

if(NOT KICK_JUCE_DIR)
    file(GLOB _kick_juce_candidates "${KICK_REPO_ROOT}/.3rdPartySourceCache/juce/*/modules")
    list(GET _kick_juce_candidates 0 _kick_juce_modules)
    if(_kick_juce_modules)
        get_filename_component(KICK_JUCE_DIR "${_kick_juce_modules}/.." ABSOLUTE)
    endif()
endif()
if(NOT KICK_JUCE_DIR OR NOT EXISTS "${KICK_JUCE_DIR}/modules/juce_core/juce_core.h")
    message(FATAL_ERROR "KICK_MCU_HOST_UPDATER needs a JUCE checkout: set KICK_JUCE_DIR")
endif()

set(_juce_core "${KICK_JUCE_DIR}/modules/juce_core/juce_core.cpp")
if(APPLE)
    # juce_core.mm is the Objective-C++ twin that pulls in the Apple parts.
    set(_juce_core "${KICK_JUCE_DIR}/modules/juce_core/juce_core.mm")
endif()

add_library(kick_host_juce_core STATIC ${_juce_core})
target_include_directories(kick_host_juce_core PUBLIC "${KICK_JUCE_DIR}/modules")
target_compile_definitions(kick_host_juce_core PUBLIC
    NDEBUG=1
    JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
    JUCE_STANDALONE_APPLICATION=1
    JUCE_MODULE_AVAILABLE_juce_core=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0)
target_compile_options(kick_host_juce_core PRIVATE -w)
if(APPLE)
    target_link_libraries(kick_host_juce_core PUBLIC "-framework Foundation" "-framework CoreServices" "-framework AppKit" "-framework IOKit" "-framework Security")
else()
    target_link_libraries(kick_host_juce_core PUBLIC pthread dl rt)
endif()

add_executable(kick_host_updater_tests
    host_shim.cpp
    host_updater_test.cpp
    host_updater_main.cpp
    ${KICK_REPO_ROOT}/ComProtocol/PktMgr/PktMgr.cpp
)
target_include_directories(kick_host_updater_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../sim
    ${KICK_REPO_ROOT}/Kick/modules
    ${KICK_REPO_ROOT}/Kick/modules/hardware/source
    ${KICK_REPO_ROOT}/Kick/modules/contracts
    ${KICK_REPO_ROOT}/Kick/modules/contracts/dcl
    ${KICK_REPO_ROOT}/Kick/modules/3rdParty
    ${KICK_REPO_ROOT}/ComProtocol
    ${KICK_REPO_ROOT}/ComProtocol/PktMgr
)
target_compile_definitions(kick_host_updater_tests PRIVATE KICK_STAMPED_HEX="${CMAKE_BINARY_DIR}/tests/payload-stamped.hex")
target_link_libraries(kick_host_updater_tests PRIVATE kick_mcu_bsp_sim kick_host_juce_core GTest::gtest)
# Host code, not ours: keep its warnings out of the log.
set_source_files_properties(host_shim.cpp ${KICK_REPO_ROOT}/ComProtocol/PktMgr/PktMgr.cpp PROPERTIES COMPILE_OPTIONS "-w")
if(TARGET kick_mcu_stamped_image)
    add_dependencies(kick_host_updater_tests kick_mcu_stamped_image)
endif()

# One process per scenario: the host updater cannot run twice in a process (see the test file).
foreach(_scenario
        HappyPath
        ImageWithABadCrcIsRefusedAndTheHostReportsFailure
        ADroppedAckIsEndedByTheMcuTimeoutAndTheHostFails
        PowerLostInTheMiddleOfTheTransferLeavesTheOldImageRunning
        PowerLostWhileErasingTheBootControlCopyKeepsTheOldImage
        PowerLostInTheMiddleOfTheBootControlRecordKeepsTheOldImage)
    add_test(NAME HostUpdater.${_scenario} COMMAND kick_host_updater_tests --gtest_filter=HostRig.${_scenario})
endforeach()
