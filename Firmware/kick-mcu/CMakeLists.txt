# SPDX-License-Identifier: AGPL-3.0-or-later
#
# Firmware for the KICK control-surface MCU, built on the desktop against a simulated board.
# Standalone: nothing here is wired into the Kick app build.
#
#   cmake -S Firmware/kick-mcu -B build/mcu -G Ninja
#   cmake --build build/mcu
#   ctest --test-dir build/mcu --output-on-failure
#
# Options:
#   -DKICK_SANITIZE=ON        build the library and tests with ASan + UBSan
#   -DKICK_MCU_BUILD_TESTS=OFF
cmake_minimum_required(VERSION 3.22)

project(KickMcu VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(KICK_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(KICK_MCU_BUILD_TESTS "Build the unit tests" ON)

set(KICK_MCU_WARNINGS -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion)

if(KICK_SANITIZE)
    set(KICK_MCU_SANITIZE_FLAGS -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined)
endif()

# The portable core. It is what ships on the MCU, so it is built the way the target build will
# be: no exceptions, no RTTI, and nothing in it allocates.
add_library(kick_mcu_core STATIC
    src/link/frame_codec.cpp
    src/link/rx_parser.cpp
    src/link/tx_queue.cpp
    src/link/session.cpp
    src/link/link.cpp
    src/input/debounce.cpp
    src/input/velocity_curve.cpp
    src/input/pads.cpp
    src/input/encoders.cpp
    src/input/strip.cpp
    src/output/colours.cpp
    src/output/commands.cpp
    src/output/current_budget.cpp
    src/output/led_renderer.cpp
    src/system/image.cpp
    src/system/boot_control.cpp
    src/system/updater.cpp
    src/system/self_check.cpp
    src/system/system.cpp
    src/app/scan_loop.cpp
    src/app/firmware.cpp
)
target_include_directories(kick_mcu_core PUBLIC include)
target_compile_options(kick_mcu_core PRIVATE ${KICK_MCU_WARNINGS} -fno-exceptions -fno-rtti)
if(KICK_SANITIZE)
    target_compile_options(kick_mcu_core PUBLIC ${KICK_MCU_SANITIZE_FLAGS})
    target_link_options(kick_mcu_core PUBLIC ${KICK_MCU_SANITIZE_FLAGS})
endif()

# Desktop implementations of the BSP interfaces (header only).
add_library(kick_mcu_bsp_sim INTERFACE)
target_include_directories(kick_mcu_bsp_sim INTERFACE bsp/sim)
target_link_libraries(kick_mcu_bsp_sim INTERFACE kick_mcu_core)

# The desktop simulator and the script player it uses. POSIX only: it opens a pty or a TCP socket.
option(KICK_MCU_BUILD_SIM "Build kick-mcu-sim" ON)
if(KICK_MCU_BUILD_SIM AND NOT WIN32)
    add_library(kick_mcu_sim_lib STATIC sim/script_player.cpp sim/serial_transport.cpp sim/led_json.cpp sim/trace_replay.cpp sim/hex_image.cpp sim/system_json.cpp)
    target_include_directories(kick_mcu_sim_lib PUBLIC sim)
    target_link_libraries(kick_mcu_sim_lib PUBLIC kick_mcu_bsp_sim)
    target_compile_options(kick_mcu_sim_lib PRIVATE ${KICK_MCU_WARNINGS})

    add_executable(kick-mcu-sim sim/kick-mcu-sim.cpp)
    target_link_libraries(kick-mcu-sim PRIVATE kick_mcu_sim_lib)
    target_compile_options(kick-mcu-sim PRIVATE ${KICK_MCU_WARNINGS})
endif()

if(KICK_MCU_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
