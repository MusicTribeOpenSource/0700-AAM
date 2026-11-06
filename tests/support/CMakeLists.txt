# Product code under test, compiled without JUCE.

set(AUDIO_ROOT ${KICK_REPO_ROOT}/AudIO/AudIO)

# audio_unity.cpp hand-mirrors AudIO.cpp. Fail configuration if it includes a source
# AudIO.cpp no longer builds, so a rename or removal cannot leave the tests testing a
# file the product has dropped.
function(kick_included_sources file out_var)
    file(STRINGS ${file} lines REGEX "^[ \t]*#[ \t]*include \"[A-Za-z]+/[A-Za-z]+\\.cpp\"")
    set(sources "")
    foreach(line IN LISTS lines)
        string(REGEX REPLACE "^.*\"([^\"]+)\".*$" "\\1" path "${line}")
        list(APPEND sources "${path}")
    endforeach()
    set(${out_var} "${sources}" PARENT_SCOPE)
endfunction()

kick_included_sources(${AUDIO_ROOT}/AudIO.cpp product_sources)
kick_included_sources(${CMAKE_CURRENT_SOURCE_DIR}/audio_unity.cpp unity_sources)
foreach(src IN LISTS unity_sources)
    if(NOT src IN_LIST product_sources)
        message(FATAL_ERROR "audio_unity.cpp includes ${src}, which AudIO.cpp does not. Update the mirror.")
    endif()
endforeach()

add_library(kick_pktmgr STATIC ${KICK_REPO_ROOT}/ComProtocol/PktMgr/PktMgr.cpp)
target_include_directories(kick_pktmgr PUBLIC ${KICK_REPO_ROOT}/ComProtocol/PktMgr)

add_library(kick_audio_core OBJECT audio_unity.cpp)
target_include_directories(kick_audio_core PUBLIC
    ${AUDIO_ROOT}/Includes ${AUDIO_ROOT}/3rdparty ${AUDIO_ROOT}/Objects ${AUDIO_ROOT}/Dsp ${AUDIO_ROOT}/Core ${AUDIO_ROOT})
# Product sources carry their own warnings; keep the test log readable.
target_compile_options(kick_audio_core PRIVATE -w)
target_compile_options(kick_pktmgr PRIVATE -w)

add_library(kick_test_support INTERFACE)
target_include_directories(kick_test_support INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
