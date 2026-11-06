set(KICK_TEST_LABEL integration)

function(kick_add_integration_test name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE GTest::gtest_main GTest::gmock kick_test_support Threads::Threads)
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
    target_compile_options(${name} PRIVATE -Wall -Wextra -Wno-unused-parameter)
    gtest_discover_tests(${name} DISCOVERY_TIMEOUT 60 PROPERTIES LABELS integration)
endfunction()

kick_add_integration_test(test_audio_graph audio_graph_test.cpp)
target_link_libraries(test_audio_graph PRIVATE kick_audio_core)

kick_add_integration_test(test_pktmgr_stream pktmgr_stream_test.cpp)
target_link_libraries(test_pktmgr_stream PRIVATE kick_pktmgr)
