cmake_minimum_required(VERSION 3.16)
project(KickTests LANGUAGES C CXX)

# Standalone test tree. Builds without JUCE, hardware, or the Buildroot toolchain.
#   cmake -S tests -B build/tests -G Ninja && cmake --build build/tests && ctest --test-dir build/tests
# JUCE-backed UI tests live in tests/ui and are opt-in: -DKICK_UI_TESTS=ON

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(KICK_SANITIZE "Build tests with ASan + UBSan" OFF)
option(KICK_TSAN "Build tests with ThreadSanitizer (exclusive with KICK_SANITIZE)" OFF)
option(KICK_COVERAGE "Instrument tests for line coverage" OFF)
option(KICK_UI_TESTS "Build JUCE-backed UI tests (fetches JUCE)" OFF)

set(KICK_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." CACHE PATH "Repo root")

include(cmake/Sanitizers.cmake)
include(cmake/Dependencies.cmake)

enable_testing()
include(GoogleTest)
find_package(Threads REQUIRED)

add_subdirectory(support)
add_subdirectory(unit)
add_subdirectory(integration)

if(KICK_UI_TESTS)
    add_subdirectory(ui)
endif()
