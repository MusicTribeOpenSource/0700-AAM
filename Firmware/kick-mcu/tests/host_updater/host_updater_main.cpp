// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Runs the selected scenario and leaves with _exit. The host updater keeps a singleton and function
// statics that outlive the test, and a worker thread may still hold them; normal static
// destruction at exit would race with it.
#include <gtest/gtest.h>

#include <cstdio>
#include <unistd.h>

int main( int argc, char** argv )
{
    ::testing::InitGoogleTest( &argc, argv );
    const int result = RUN_ALL_TESTS();
    std::fflush( stdout );
    std::fflush( stderr );
    _exit( result );
}
