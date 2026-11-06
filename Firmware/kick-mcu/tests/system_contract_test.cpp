// SPDX-License-Identifier: AGPL-3.0-or-later
//
// The system and update values restated in the firmware, checked against the host's headers.
#include <functional>

#include "kick_Hwcommon.h" // mt::MCUUpdateAction

// kick_Hwcommon.h defines INFO, FAIL, OKAY and DATA as macros; FAIL collides with googletest's.
#undef INFO
#undef FAIL
#undef OKAY
#undef DATA

#include <gtest/gtest.h>

#include "Defines.hpp"
#include "kick_mcu/contract.h"
#include "kick_mcu/system/image.h"

namespace c = kick_mcu::contract;
namespace s = kick_mcu::system;

#define SAME( mine, theirs ) static_assert( (int)( mine ) == (int)( theirs ), #mine " differs from " #theirs )

SAME( c::SW_UPDATE_INITIATE, mt::EN_SW_UPDATE_INITIATE );
SAME( c::SW_UPDATE_BEGIN_TRANSFER, mt::EN_SW_UPDATE_BEGIN_TRANSFER );
SAME( c::SW_UPDATE_DATA, mt::EN_SW_UPDATE_DATA );
SAME( c::SW_UPDATE_DATABASE, mt::EN_SW_UPDATE_DATABASE );
SAME( c::SW_UPDATE_END, mt::EN_SW_UPDATE_END );
SAME( c::SW_UPDATE_ACK, mt::EN_SW_UPDATE_ACK );
SAME( c::SW_UPDATE_FAILURE, mt::EN_SW_UPDATE_FAILURE );
SAME( c::SW_UPDATE_IDLE, mt::EN_SW_UPDATE_IDLE );

static_assert( s::kFlashValid == FLASH_VALID, "FLASH_VALID in Defines.hpp" );

// VersionNo maps field for field onto typeVersionNo. The host struct is 4 bytes with an alignment
// byte that is "not to be used"; this one carries the three real fields.
static_assert( sizeof( typeVersionNo ) == 4, "typeVersionNo is main, sub, bug and a dummy byte" );

TEST( SystemContract, VersionFieldsMapOntoTheHostsTypeVersionNo )
{
    typeVersionNo host{ 1, 2, 3, 0 };
    s::VersionNo  mine{ host.ui8Main, host.ui8Sub, host.ui8Bug };
    EXPECT_EQ( 1, mine.major );
    EXPECT_EQ( 2, mine.minor );
    EXPECT_EQ( 3, mine.bug );
}

TEST( SystemContract, TheHostsUpdateGateReadsTwoDecimalBytes )
{
    // kick_AppHome.cpp builds the surface version as juce::String( data[0] ) + "." + juce::String( data[1] ).
    // Restated here with std::to_string, which formats a uint8_t value the same way as juce::String( int ).
    const uint8_t data[2] = { 2, 10 };
    EXPECT_EQ( "2.10", std::to_string( data[0] ) + "." + std::to_string( data[1] ) );
}
