include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Allow offline / pre-seeded builds: -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/path
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    # v1.15.2, pinned by commit: a tag can be moved, a commit cannot.
    GIT_TAG        b514bdc898e2951020cbdca1304b75f5950d1f59)
FetchContent_MakeAvailable(googletest)
