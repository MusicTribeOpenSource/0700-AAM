function(kick_add_test name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE GTest::gtest_main GTest::gmock)
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
    target_compile_options(${name} PRIVATE -Wall -Wextra)
    gtest_discover_tests(${name} DISCOVERY_TIMEOUT 60 PROPERTIES LABELS "${KICK_TEST_LABEL}")
endfunction()

set(KICK_TEST_LABEL unit)
kick_add_test(test_pktmgr pktmgr_test.cpp)
target_link_libraries(test_pktmgr PRIVATE kick_pktmgr kick_test_support)

kick_add_test(test_audio_utils audio_utils_test.cpp)
target_link_libraries(test_audio_utils PRIVATE kick_audio_core)

kick_add_test(test_audio_containers audio_containers_test.cpp)
target_link_libraries(test_audio_containers PRIVATE kick_audio_core kick_test_support Threads::Threads)

kick_add_test(test_audio_dsp audio_dsp_test.cpp)
target_link_libraries(test_audio_dsp PRIVATE kick_audio_core)
