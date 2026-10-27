foreach(kind normal crash hang slow held_state process_crash process_hang)
    add_library(kick-clap-fixture-${kind} MODULE Fixture.cpp)
    target_include_directories(kick-clap-fixture-${kind} PRIVATE ../third_party/clap/include)
    target_compile_features(kick-clap-fixture-${kind} PRIVATE cxx_std_17)
    target_link_libraries(kick-clap-fixture-${kind} PRIVATE kick_clap_sanitizers)
    set_target_properties(kick-clap-fixture-${kind} PROPERTIES PREFIX "" SUFFIX ".clap")
    if(NOT kind STREQUAL "normal")
        target_compile_definitions(kick-clap-fixture-${kind} PRIVATE FIXTURE_${kind}=1)
    endif()
endforeach()
add_executable(kick-clap-tests HostTests.cpp)
target_link_libraries(kick-clap-tests PRIVATE kick_clap_host)
add_test(NAME kick-clap-host COMMAND kick-clap-tests
    $<TARGET_FILE:kick-clap-fixture-normal> $<TARGET_FILE:kick-clap-scan>
    $<TARGET_FILE:kick-clap-fixture-crash> $<TARGET_FILE:kick-clap-fixture-hang>)
set_tests_properties(kick-clap-host PROPERTIES TIMEOUT 30)
if(NOT WIN32)
    add_test(NAME kick-clap-scanner-cli COMMAND ${CMAKE_COMMAND} -E env
        "PATH=$<TARGET_FILE_DIR:kick-clap-scan>:$ENV{PATH}"
        kick-clap-scan $<TARGET_FILE:kick-clap-fixture-normal>)
    set_tests_properties(kick-clap-scanner-cli PROPERTIES TIMEOUT 10)
endif()
if(TARGET kick_clap_juce)
    add_executable(kick-clap-session-tests SessionTests.cpp)
    target_link_libraries(kick-clap-session-tests PRIVATE kick_clap_juce)
    add_test(NAME kick-clap-session COMMAND kick-clap-session-tests $<TARGET_FILE:kick-clap-fixture-normal>)
    set_tests_properties(kick-clap-session PROPERTIES TIMEOUT 30)

    add_executable(kick-clap-persistence-tests PersistenceTests.cpp ../../modules/engine/project/kick_xml_manager.cpp)
    target_link_libraries(kick-clap-persistence-tests PRIVATE kick_clap_juce)
    target_compile_definitions(kick-clap-persistence-tests PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)
    add_test(NAME kick-clap-persistence COMMAND kick-clap-persistence-tests $<TARGET_FILE:kick-clap-fixture-normal>)
    set_tests_properties(kick-clap-persistence PROPERTIES TIMEOUT 30)

    add_executable(kick-clap-hardware-tests HardwareInputTests.cpp
        ../../modules/contracts/dcl/dcl_signals.cpp
        ../../modules/contracts/dcl/dcl_contracts_helper.cpp
        ../../modules/contracts/utils/keyboard/keyboard_signals.cpp
        ../../modules/ipc/utils/mt_ipc_json.cpp
        ../../modules/ipc/utils/mt_ipc_message_constructor.cpp)
    target_include_directories(kick-clap-hardware-tests PRIVATE ../../modules ../../../ComProtocol)
    target_link_libraries(kick-clap-hardware-tests PRIVATE juce::juce_events kick_clap_sanitizers)
    target_compile_definitions(kick-clap-hardware-tests PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_USE_CURL=0)
    add_test(NAME kick-clap-hardware COMMAND kick-clap-hardware-tests)
    set_tests_properties(kick-clap-hardware PROPERTIES TIMEOUT 10)
endif()

add_executable(kick-clap-benchmark-tests BenchmarkTests.cpp)
target_link_libraries(kick-clap-benchmark-tests PRIVATE kick_clap_benchmark)
add_test(NAME kick-clap-benchmark COMMAND kick-clap-benchmark-tests
    $<TARGET_FILE:kick-clap-fixture-normal> $<TARGET_FILE:kick-clap-fixture-slow> $<TARGET_FILE:kick-clap-fixture-held_state>)
set_tests_properties(kick-clap-benchmark PROPERTIES TIMEOUT 20)
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME kick-clap-benchmark-cli COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/BenchmarkCliTests.py"
        $<TARGET_FILE:kick-clap-bench> $<TARGET_FILE:kick-clap-fixture-normal>
        $<TARGET_FILE:kick-clap-fixture-process_crash> $<TARGET_FILE:kick-clap-fixture-process_hang>
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/benchmark_matrix.py")
    set_tests_properties(kick-clap-benchmark-cli PROPERTIES TIMEOUT 30)
endif()
if(TARGET kick-clap-device-bench)
    add_executable(kick-clap-device-benchmark-tests DeviceBenchmarkTests.cpp)
    target_link_libraries(kick-clap-device-benchmark-tests PRIVATE kick_clap_benchmark juce::juce_audio_devices)
    target_compile_definitions(kick-clap-device-benchmark-tests PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
    add_test(NAME kick-clap-device-benchmark COMMAND kick-clap-device-benchmark-tests $<TARGET_FILE:kick-clap-fixture-normal>)
    set_tests_properties(kick-clap-device-benchmark PROPERTIES TIMEOUT 10)
endif()
if(COMMAND _juce_fixup_module_source_groups)
    _juce_fixup_module_source_groups()
endif()
