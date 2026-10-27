foreach(kind normal crash hang)
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
