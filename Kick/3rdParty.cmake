
##
## Repo paths.
##
# Base URL of the git server hosting the AudIO and ComProtocol repositories.
# These were internal repositories and are not part of this release; override with
#   -DMT_PLATFORM_GIT_PATH=<your git base url>
set(MT_PLATFORM_GIT_PATH   "https://example.com/your-git-server" CACHE STRING "Base URL for private platform repositories")

##
## 3RD Party git repo tags
##
#set(JUCE_GIT_TAG           19ba6bf1935f570f97f0b63777b69dcaf0327b58) # 15-Aug-23
set(JUCE_GIT_TAG           7.0.12)
set(PERFETTO_GIT_TAG       origin/main)
set(AUDIO_GIT_TAG          8999ab9ba138121d7d038b80a817b1b5cff3980c)
set(COM_PROTOCOL_GIT_TAG   d00bb9c8a590d3677bd9f400eb7a22d38acf4bfa)

##
## JUCE
##
# IF you want to instead point it to a local version, you can invoke CMake with  -DCPM_JUCE_SOURCE="Path_To_JUCE_git_repo"
# Define the patch file based on the JUCE version tag
set(JUCE_PATCH_FILE "${CMAKE_SOURCE_DIR}/patches/juce_multitouch_${JUCE_GIT_TAG}.patch")

CPMAddPackage( 
    NAME JUCE
    GITHUB_REPOSITORY juce-framework/JUCE
    GIT_TAG ${JUCE_GIT_TAG}
    DOWNLOAD_ONLY True
    OPTIONS "JUCE_MODULES_ONLY ON" 
)

# Check if JUCE was added and apply patch
if(JUCE_ADDED)
    message(STATUS "Downloaded JUCE repository into \"${JUCE_SOURCE_DIR}\"")

    # Check if the patch file exists
    if(EXISTS ${JUCE_PATCH_FILE})
        message("### Patch file found..${JUCE_PATCH_FILE}")
    else()
        message(WARNING "### No patch file found for JUCE version ${JUCE_GIT_TAG}")
    endif()
    
    # Flag for Patch applied.
    set(PATCH_FLAG_FILE "${JUCE_SOURCE_DIR}/juce_patch_applied.flag")
    # Check if patch is already applied
    if(NOT EXISTS ${PATCH_FLAG_FILE})
	    # Apply patch to JUCE
	    execute_process(
		    COMMAND git apply ${JUCE_PATCH_FILE}
		    WORKING_DIRECTORY ${JUCE_SOURCE_DIR}
		    RESULT_VARIABLE patch_result
		    )
	    if(patch_result)
		    message(FATAL_ERROR "Failed to apply patch to JUCE: ${patch_result}")
	    else()
		    message(STATUS "## Patch applied successfully to JUCE.")
		    file(WRITE ${PATCH_FLAG_FILE} "JUCE patch applied")
	    endif()
    else()
	    message(STATUS "### Patch already applied to JUCE. Skipping patch step.")
    endif()

    # Emulator on macOS: JUCE 7.0.12 calls CGWindowListCreateImage, which the
    # macOS 15 SDK removed. Stub the one function that uses it.
    if(KICK_EMU AND APPLE)
        set(EMU_PATCH_FLAG_FILE "${JUCE_SOURCE_DIR}/juce_emu_macos15.flag")
        if(NOT EXISTS ${EMU_PATCH_FLAG_FILE})
            execute_process(
                COMMAND git apply ${KICK_EMU_DIR}/patches/juce_7.0.12_macos15_sdk.patch
                WORKING_DIRECTORY ${JUCE_SOURCE_DIR}
                RESULT_VARIABLE emu_patch_result
            )
            if(emu_patch_result)
                message(FATAL_ERROR "Failed to apply emu macOS 15 SDK patch to JUCE: ${emu_patch_result}")
            endif()
            file(WRITE ${EMU_PATCH_FLAG_FILE} "emu macOS 15 SDK patch applied")
        endif()
    endif()

    add_subdirectory(${JUCE_SOURCE_DIR} ./cmake_build_juce)
endif()

##
## AudIO and COM Protocol
##
if(KICK_EMU)
    # Emulator build: use the public copies that ship in this repo instead of
    # the private git server, plus stand-ins for the modules that were not
    # released (see emu/README.md).
    message(STATUS "KICK_EMU: using in-tree AudIO, ComProtocol and emu stand-in modules")
    set(KICK_REPO_ROOT ${CMAKE_SOURCE_DIR}/..)
    juce_add_module(${KICK_REPO_ROOT}/AudIO/AudIO)
    juce_add_module(${KICK_EMU_DIR}/modules/PktMgr)
    juce_add_modules(
        ${KICK_EMU_DIR}/modules/AudIO_constants
        ${KICK_EMU_DIR}/modules/AudIO_juce
        ${KICK_EMU_DIR}/modules/kick_emu
    )
else()
    ##
    ## AudIO
    ##
    # IF you want to instead point it to a local version, you can invoke CMake with  -DCPM_AudIO_SOURCE="Path_To_AudIO_git_repo"
    if(AUDIO_DEVELOPMENT)
        message(STATUS "Using local sources for AudIO...")
        if (MSVC)
            message(STATUS "MSVC")
            CPMAddPackage( NAME AudIO DOWNLOAD_ONLY True SOURCE_DIR D:/azure/AudIO )
        endif (MSVC)
        if (UNIX)
            message(STATUS "UNIX")
            CPMAddPackage( NAME AudIO DOWNLOAD_ONLY True SOURCE_DIR /mnt/c/azure/AudIO )
        endif (UNIX)
    else(AUDIO_DEVELOPMENT)
        message(STATUS "Using GIT sources for AudIO")
        CPMAddPackage( 
            NAME AudIO
            GIT_REPOSITORY ${MT_PLATFORM_GIT_PATH}/AudIO 
            GIT_TAG ${AUDIO_GIT_TAG}
            DOWNLOAD_ONLY True
        )
    endif(AUDIO_DEVELOPMENT)

    if(AudIO_ADDED)	 
        message(STATUS "Added AudIO package into \"${AudIO_SOURCE_DIR}\"")
        include(${AudIO_SOURCE_DIR}/juce_modules.cmake)
    endif()

    ##
    ## COM Protocol
    ##
    CPMAddPackage( 
            NAME COM_PROTOCOL
            GIT_REPOSITORY ${MT_PLATFORM_GIT_PATH}/ComProtocol 
            GIT_TAG ${COM_PROTOCOL_GIT_TAG}
            DOWNLOAD_ONLY True
        )

    if(COM_PROTOCOL_ADDED)	 
        message(STATUS "Added Com Protocol package into \"${COM_PROTOCOL_SOURCE_DIR}\"")
        juce_add_modules(${COM_PROTOCOL_SOURCE_DIR}/PktMgr)
    endif()


endif(KICK_EMU)

##
## Perfetto Profiler
##
if(PERFETTO)
    CPMAddPackage( 
        NAME melatonin_perfetto
        GITHUB_REPOSITORY sudara/melatonin_perfetto
        GIT_TAG ${PERFETTO_GIT_TAG}
    )

    if(melatonin_perfetto_ADDED)	 
        message(STATUS "Added melatonin_perfetto into \"${melatonin_perfetto_SOURCE_DIR}\"")
        FetchContent_MakeAvailable(melatonin_perfetto)
    endif()
endif()

