include_guard()

cmake_minimum_required(VERSION 3.16)
project(KIckDevelopment)

option(KICK_CLAP_HOST_ONLY "Build CLAP hosting without the unreleased hardware dependencies" OFF)
if(KICK_CLAP_HOST_ONLY)
    enable_testing()
    add_subdirectory(clap_host)
    return()
endif()

## Set CPM cache location
if( NOT CPM_SOURCE_CACHE )
    set(CPM_SOURCE_CACHE ${CMAKE_SOURCE_DIR}/../.3rdPartySourceCache )
endif()
message("")
message("###### 3rd PARTY SOURCE CACHE DIR=${CPM_SOURCE_CACHE}")
message("")


## CMake package management...
include(get_cpm.cmake)

# Add or Disable optional components
option( TIMESTAMP "Require fresh timestamp every build" ON )
option( USE_VALUE_TREE_DEBUGGER "Value Tree Debugger" OFF )
option( DISABLE_JUCE_MEMORY_LEAK_DETECTION "juce memory" OFF )
option( AUDIO_DEVELOPMENT "Local development sources for AudIO" OFF )
option( KICK_VST_PLUGIN "KICK VST Plugin" OFF )
option( KICK_SIMULATOR "KICK Simulator" OFF )
option( KICK_PLAYBACK "KICK Audio Playback App" OFF )
option( KICK_MULTITOUCH "KICK Multitouch demo App" OFF )
option( DEBUG_TOUCH "Enable mouse cursor" OFF )
option( USE_ASAN_KICK "enable address sanitizer" OFF )
option( PERFETTO "Enable tracing using Perfetto" OFF )

#Compile commands, useful for some IDEs like VS-Code
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

#Adds all the module sources so they appear correctly in the IDE
set_property(GLOBAL PROPERTY USE_FOLDERS YES)
option(JUCE_ENABLE_MODULE_SOURCE_GROUPS "Enable Module Source Groups" ON)

if (DISABLE_JUCE_MEMORY_LEAK_DETECTION)
    add_definitions(-DJUCE_CHECK_MEMORY_LEAKS=0)
endif()

if(USE_VALUE_TREE_DEBUGGER)
    add_definitions(-DUSE_VALUE_TREE_DEBUGGER=1)
endif()

if(USE_ASAN_KICK)
    target_link_libraries(${TargetName}   PRIVATE libsancov.lib)
    target_link_options(${TargetName}     PRIVATE /fsanitize=address)
    target_compile_options(${TargetName}  PRIVATE -fsanitize=address)
    target_compile_options(${TargetName}  PRIVATE -fsanitize-coverage=inline-8bit-counters)
    target_compile_options(${TargetName}  PRIVATE /fsanitize=address)
    target_compile_options(${TargetName}  PRIVATE /Od)
    set(addressSanitizerEnabled true)
endif()

if("${ENABLE_OPTICK}" STREQUAL "")
    set(USE_OPTICK_VAL 0)
else()
    set(USE_OPTICK_VAL ${ENABLE_OPTICK})
endif()

set(USE_VST 0)
set(USE_AU 0)

if(BUILD_VST)
    set(USE_VST 1)
endif()

## Standards
set(CMAKE_CXX_STANDARD 17 CACHE STRING "The C++ standard to use")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
## Position independent code
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# If we are using MSVC we want static runtime linkage
if(APPLE)
    set (CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE INTERNAL "")
endif(APPLE)

# Compile with werror in release builds
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w -Wno-unknown-pragmas -ffast-math -Ofast" )
    ## set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto -stdlib=libc++ -funroll-loops -ftree-vectorize -fvectorize")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ")
elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE INTERNAL "")
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_compile_options(
        $<$<CONFIG:Release>:/MT> # Runtime library: Multi-threaded
        $<$<CONFIG:RelWithDebInfo>:/MT> # Runtime library: Multi-threaded                           
        $<$<CONFIG:Debug>:/MTd> # Runtime library: Multi-threaded Debug
        /MP /Gm- /Zi
        /permissive-
        /fp:fast
        /Zc:twoPhase-
    )
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W1 /wd4065 /wd4100 /wd4101 /wd4127 /wd4189 /wd4201 /wd4244 /wd4267 /wd4505 /wd4702")
    #set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /WX")
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

## CMake package management...
## Fetch 3rd Party dependencies from their respective git repositories...
include(3rdParty.cmake)

set(KICK_CLAP_BUILD_JUCE ON CACHE BOOL "Build the KICK CLAP adapter" FORCE)
set(KICK_CLAP_BUILD_TESTS OFF CACHE BOOL "Build standalone CLAP tests separately")
add_subdirectory(clap_host)

# Add project applications...
if( TIMESTAMP )
    add_subdirectory(timestamp)
endif()
add_subdirectory(modules)
add_subdirectory(apps)

##
## Add CPack support
##
set(CPACK_GENERATOR "7Z")
set(CPACK_PACKAGE_DIRECTORY ${CMAKE_SOURCE_DIR}/.build_cache/dist)

set(CPACK_STRIP_FILES YES)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS YES)

set(CPACK_SOURCE_IGNORE_FILES
    /.git
    /dist
    /.*build.*
    /\\\\.DS_Store
    /JUCE
)
set(
    CPACK_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE
)

include(CPack)
