// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Every LED, PAD and SLIDER command with a payload the host really sends, then the same command
// with each kind of bad payload. The payloads in the "host" tests are the ones
// DclContractsHelper builds (Kick/modules/contracts/dcl/dcl_contracts_helper.cpp).
#include <gtest/gtest.h>

#include "kick_mcu/output/commands.h"
#include "test_support.h"

using namespace kick_mcu;
using namespace kick_mcu::output;
using namespace testing_support;
using R = CommandResult;

namespace
{
struct Decode : ::testing::Test
{
    LedState       state;
    CommandDecoder decoder{ state };

    R send( uint8_t type, uint8_t cmd, const Bytes& payload )
    {
        link::Frame f;
        f.type   = type;
        f.dir    = contract::DIR_REQUEST;
        f.cmd    = cmd;
        f.length = (uint8_t)payload.size();
        for( size_t i = 0; i < payload.size(); ++i )
            f.payload[i] = payload[i];
        return decoder.handle( f );
    }
    R led( uint8_t cmd, const Bytes& p ) { return send( contract::PKT_LED, cmd, p ); }
    R pad( uint8_t cmd, const Bytes& p ) { return send( contract::PKT_PAD, cmd, p ); }
    R slider( uint8_t cmd, const Bytes& p ) { return send( contract::PKT_SLIDER, cmd, p ); }

    const LedFrame& table() const { return state.front(); }

    // True if the table is the power-up table.
    bool untouched() const
    {
        const LedFrame blank;
        return std::memcmp( &blank, &table(), sizeof( LedFrame ) ) == 0;
    }
};

constexpr uint8_t ON = contract::STATE_ON, OFF = contract::STATE_OFF, TRANS = contract::kPadColourTrans;
} // namespace

// ---- LED -----------------------------------------------------------------------------------

TEST_F( Decode, LedSetSingleHoldsStateAndColour )
{
    EXPECT_EQ( R::Applied, led( contract::LED_SET_SINGLE_LED, { 13, ON, 0 } ) ); // setLEDONMessage( EN_PAD_MIX )
    EXPECT_EQ( ON, table().buttons[13].state );
    EXPECT_EQ( 0, table().buttons[13].colour );

    EXPECT_EQ( R::Applied, led( contract::LED_SET_SINGLE_LED, { 52, contract::STATE_SAMPLE_ASSIGNED, 9 } ) );
    EXPECT_EQ( contract::STATE_SAMPLE_ASSIGNED, table().buttons[52].state );
    EXPECT_EQ( 9, table().buttons[52].colour );

    EXPECT_EQ( R::Applied, led( contract::LED_SET_SINGLE_LED, { 13, OFF, 0 } ) ); // setLEDOFFMessage
    EXPECT_EQ( OFF, table().buttons[13].state );
}

TEST_F( Decode, LedSetSingleAcceptsTheTwoByteBankMessage )
{
    // setBankMessage sends [EN_BANK_A + bank, 1] with no colour byte.
    EXPECT_EQ( R::Applied, led( contract::LED_SET_SINGLE_LED, { 3, ON } ) );
    EXPECT_EQ( ON, table().buttons[3].state );
    EXPECT_EQ( 0, table().buttons[3].colour );
    EXPECT_EQ( R::Applied, led( contract::LED_SET_SINGLE_LED, { 5, ON } ) );
    EXPECT_EQ( OFF, table().buttons[3].state ) << "bank radio group still applies";
}

TEST_F( Decode, LedSetSingleRejectsBadPayloads )
{
    EXPECT_EQ( R::Malformed, led( contract::LED_SET_SINGLE_LED, {} ) );
    EXPECT_EQ( R::Malformed, led( contract::LED_SET_SINGLE_LED, { 5 } ) );
    EXPECT_EQ( R::OutOfRange, led( contract::LED_SET_SINGLE_LED, { 53, ON, 0 } ) );
    EXPECT_EQ( R::OutOfRange, led( contract::LED_SET_SINGLE_LED, { 54, ON, 0 } ) ) << "Color Edit: the host sends 54, the table ends at 52";
    EXPECT_EQ( R::OutOfRange, led( contract::LED_SET_SINGLE_LED, { 255, ON, 0 } ) );
    EXPECT_TRUE( untouched() );
    EXPECT_EQ( 0u, decoder.stats().applied );
    EXPECT_EQ( 2u, decoder.stats().malformed );
    EXPECT_EQ( 3u, decoder.stats().outOfRange );
}

TEST_F( Decode, LedBlinkSingle )
{
    EXPECT_EQ( R::Applied, led( contract::LED_BLINK_SINGLE, { 42 } ) );
    EXPECT_EQ( contract::STATE_ON_BLINK, table().buttons[42].state );
    EXPECT_EQ( R::Applied, led( contract::LED_BLINK_SINGLE, { 43, 7 } ) );
    EXPECT_EQ( 7, table().buttons[43].colour );
    EXPECT_EQ( R::Malformed, led( contract::LED_BLINK_SINGLE, {} ) );
    EXPECT_EQ( R::OutOfRange, led( contract::LED_BLINK_SINGLE, { 60 } ) );
}

TEST_F( Decode, LedSetAllSetsEveryButton )
{
    EXPECT_EQ( R::Applied, led( contract::LED_SET_ALL, { ON } ) );
    for( const auto& b : table().buttons )
        EXPECT_EQ( ON, b.state );
    EXPECT_EQ( R::Applied, led( contract::LED_SET_ALL, { OFF, 3 } ) );
    for( const auto& b : table().buttons )
    {
        EXPECT_EQ( OFF, b.state );
        EXPECT_EQ( 3, b.colour );
    }
    EXPECT_EQ( R::Malformed, led( contract::LED_SET_ALL, {} ) );
}

TEST_F( Decode, ExtraPayloadBytesAreIgnored )
{
    EXPECT_EQ( R::Applied, led( contract::LED_SET_SINGLE_LED, { 20, ON, 0, 99, 99 } ) );
    EXPECT_EQ( ON, table().buttons[20].state );
}

// ---- radio groups --------------------------------------------------------------------------

TEST_F( Decode, LightingABankTurnsTheOthersOff )
{
    EXPECT_EQ( R::Applied, led( contract::LED_SET_SINGLE_LED, { 0, ON, 1 } ) ); // setBankMessage( 0 )
    EXPECT_EQ( R::Applied, led( contract::LED_SET_SINGLE_LED, { 2, ON, 1 } ) );
    EXPECT_EQ( OFF, table().buttons[0].state );
    EXPECT_EQ( ON, table().buttons[2].state );
    EXPECT_EQ( R::Applied, led( contract::LED_SET_SINGLE_LED, { 7, ON, 1 } ) );
    int lit = 0;
    for( unsigned i = 0; i < 8; ++i )
        lit += table().buttons[i].state != OFF;
    EXPECT_EQ( 1, lit );
    EXPECT_EQ( ON, table().buttons[7].state );
}

TEST_F( Decode, PlayStopAndFullHalfLevelAreRadioPairs )
{
    led( contract::LED_SET_SINGLE_LED, { 45, ON, 1 } ); // Play
    led( contract::LED_SET_SINGLE_LED, { 44, ON, 1 } ); // Stop
    EXPECT_EQ( OFF, table().buttons[45].state );
    EXPECT_EQ( ON, table().buttons[44].state );

    led( contract::LED_SET_SINGLE_LED, { 48, ON, 0 } );
    led( contract::LED_SET_SINGLE_LED, { 49, ON, 0 } );
    EXPECT_EQ( OFF, table().buttons[48].state );
    EXPECT_EQ( ON, table().buttons[49].state );
}

TEST_F( Decode, OtherButtonsAreNotInAGroup )
{
    led( contract::LED_SET_SINGLE_LED, { 8, ON, 0 } );  // Main
    led( contract::LED_SET_SINGLE_LED, { 9, ON, 0 } );  // Grid
    led( contract::LED_SET_SINGLE_LED, { 42, ON, 0 } ); // Record
    led( contract::LED_SET_SINGLE_LED, { 43, ON, 0 } ); // Overdub
    led( contract::LED_SET_SINGLE_LED, { 46, ON, 0 } ); // Play Start sits next to the Play/Stop pair
    led( contract::LED_SET_SINGLE_LED, { 50, ON, 0 } ); // 16 Levels sits next to Full/Half
    for( int id : { 8, 9, 42, 43, 46, 50 } )
        EXPECT_EQ( ON, table().buttons[id].state ) << (int)id;
    EXPECT_EQ( nullptr, radioGroupOf( 46 ) );
    EXPECT_EQ( nullptr, radioGroupOf( 50 ) );
    EXPECT_EQ( nullptr, radioGroupOf( 8 ) );
}

TEST_F( Decode, SwitchingAGroupMemberOffLeavesTheRestAlone )
{
    led( contract::LED_SET_SINGLE_LED, { 3, ON, 1 } );
    led( contract::LED_SET_SINGLE_LED, { 5, OFF, 1 } );
    EXPECT_EQ( ON, table().buttons[3].state );
}

TEST_F( Decode, BlinkingAMemberTakesTheGroupToo )
{
    led( contract::LED_SET_SINGLE_LED, { 1, ON, 1 } );
    led( contract::LED_BLINK_SINGLE, { 4 } );
    EXPECT_EQ( OFF, table().buttons[1].state );
    EXPECT_EQ( contract::STATE_ON_BLINK, table().buttons[4].state );
}

TEST_F( Decode, SetAllIsNotARadioOperation )
{
    led( contract::LED_SET_ALL, { ON } );
    for( unsigned i = 0; i < 8; ++i )
        EXPECT_EQ( ON, table().buttons[i].state ) << i;
}

// ---- PAD -----------------------------------------------------------------------------------

TEST_F( Decode, PadSetSingle )
{
    EXPECT_EQ( R::Applied, pad( contract::PAD_SET_SINGLE_PAD, { 5, ON, 12 } ) ); // setPadDataMessage
    const PadLed& p = table().pads[5];
    EXPECT_EQ( ON, p.state );
    EXPECT_EQ( 12, p.colour );
    EXPECT_EQ( TRANS, p.offColour );
    EXPECT_EQ( 127, p.velocity );

    EXPECT_EQ( R::Malformed, pad( contract::PAD_SET_SINGLE_PAD, { 5, ON } ) );
    EXPECT_EQ( R::Malformed, pad( contract::PAD_SET_SINGLE_PAD, {} ) );
    EXPECT_EQ( R::OutOfRange, pad( contract::PAD_SET_SINGLE_PAD, { 16, ON, 1 } ) ) << "16 is EN_ALL_PADS, not a pad";
}

TEST_F( Decode, PadSetAllSingleColour )
{
    EXPECT_EQ( R::Applied, pad( contract::PAD_SET_ALL_SINGLE_COLOUR, { ON, 8 } ) );
    for( const auto& p : table().pads )
    {
        EXPECT_EQ( ON, p.state );
        EXPECT_EQ( 8, p.colour );
    }
    EXPECT_EQ( R::Applied, pad( contract::PAD_SET_ALL_SINGLE_COLOUR, { 0, TRANS } ) ); // clearAllPadsMessage
    for( const auto& p : table().pads )
    {
        EXPECT_EQ( OFF, p.state );
        EXPECT_EQ( TRANS, p.colour );
    }
    EXPECT_EQ( R::Malformed, pad( contract::PAD_SET_ALL_SINGLE_COLOUR, { ON } ) );
}

TEST_F( Decode, PadSetAllMultiColourSizesItselfFromThePayload )
{
    Bytes full{ ON };
    for( uint8_t i = 0; i < 16; ++i )
        full.push_back( i );
    EXPECT_EQ( R::Applied, pad( contract::PAD_SET_ALL_MULTI_COLOUR, full ) );
    for( uint8_t i = 0; i < 16; ++i )
    {
        EXPECT_EQ( ON, table().pads[i].state );
        EXPECT_EQ( i, table().pads[i].colour );
    }

    // Four colours: pads 0 to 3 change, the other twelve keep what they had.
    EXPECT_EQ( R::Applied, pad( contract::PAD_SET_ALL_MULTI_COLOUR, { contract::STATE_SAMPLE_ASSIGNED, 17, 16, 15, 14 } ) );
    EXPECT_EQ( contract::STATE_SAMPLE_ASSIGNED, table().pads[0].state );
    EXPECT_EQ( 17, table().pads[0].colour );
    EXPECT_EQ( 14, table().pads[3].colour );
    EXPECT_EQ( ON, table().pads[4].state );
    EXPECT_EQ( 4, table().pads[4].colour );

    EXPECT_EQ( R::Malformed, pad( contract::PAD_SET_ALL_MULTI_COLOUR, { ON } ) ) << "no colours";
    EXPECT_EQ( R::Malformed, pad( contract::PAD_SET_ALL_MULTI_COLOUR, {} ) );
    Bytes seventeen{ ON };
    seventeen.resize( 18, 1 );
    EXPECT_EQ( R::Malformed, pad( contract::PAD_SET_ALL_MULTI_COLOUR, seventeen ) ) << "17 colours do not fit 16 pads";
    EXPECT_EQ( 17, table().pads[0].colour ) << "a rejected command changes nothing";
}

TEST_F( Decode, PadSettingsAreStoredNotApplied )
{
    EXPECT_FALSE( decoder.padSettings().received );
    EXPECT_EQ( R::Applied, pad( contract::PAD_SETTINGS, { 16, 10, 20, 30 } ) );
    EXPECT_TRUE( decoder.padSettings().received );
    EXPECT_EQ( 16, decoder.padSettings().count );
    EXPECT_EQ( 10, decoder.padSettings().threshold );
    EXPECT_EQ( 20, decoder.padSettings().sensitivity );
    EXPECT_EQ( 30, decoder.padSettings().curve );
    EXPECT_TRUE( untouched() );
    EXPECT_EQ( R::Malformed, pad( contract::PAD_SETTINGS, { 10, 20, 30 } ) ) << "the host's test app sends 3 bytes; the real senders send 4";
}

TEST_F( Decode, PadColourModeIsStored )
{
    EXPECT_EQ( -1, decoder.padColourMode() );
    EXPECT_EQ( R::Applied, pad( contract::PAD_COLOR_MODE, { 4 } ) );
    EXPECT_EQ( 4, decoder.padColourMode() );
    EXPECT_EQ( R::Malformed, pad( contract::PAD_COLOR_MODE, {} ) );
    EXPECT_EQ( 4, decoder.padColourMode() );
    EXPECT_TRUE( untouched() );
}

TEST_F( Decode, PadSetColourStateTakesColoursThenStates )
{
    Bytes p;
    for( uint8_t i = 0; i < 16; ++i )
        p.push_back( (uint8_t)( 16 - i ) ); // colours
    for( uint8_t i = 0; i < 16; ++i )
        p.push_back( (uint8_t)( i % 4 ) ); // states
    EXPECT_EQ( R::Applied, pad( contract::PAD_SET_COLOUR_STATE, p ) ); // setPadColourAndStateInfoMessage
    EXPECT_EQ( 16, table().pads[0].colour );
    EXPECT_EQ( 0, table().pads[0].state );
    EXPECT_EQ( 1, table().pads[15].colour );
    EXPECT_EQ( 3, table().pads[15].state );
    EXPECT_EQ( 3, table().pads[7].state );

    // Two pads: [c0, c1, s0, s1]
    EXPECT_EQ( R::Applied, pad( contract::PAD_SET_COLOUR_STATE, { 9, 8, 1, 2 } ) );
    EXPECT_EQ( 9, table().pads[0].colour );
    EXPECT_EQ( 1, table().pads[0].state );
    EXPECT_EQ( 8, table().pads[1].colour );
    EXPECT_EQ( 2, table().pads[1].state );

    EXPECT_EQ( R::Malformed, pad( contract::PAD_SET_COLOUR_STATE, { 9, 8, 1 } ) ) << "odd length";
    EXPECT_EQ( R::Malformed, pad( contract::PAD_SET_COLOUR_STATE, {} ) );
    EXPECT_EQ( R::Malformed, pad( contract::PAD_SET_COLOUR_STATE, Bytes( 34, 1 ) ) ) << "17 pads";
}

TEST_F( Decode, PadSetBlinking )
{
    EXPECT_EQ( R::Applied, pad( contract::PAD_SET_BLINKING, { 2, contract::STATE_ON_BLINK, 12, 16 } ) ); // setPadBlinkingMessage
    const PadLed& p = table().pads[2];
    EXPECT_EQ( contract::STATE_ON_BLINK, p.state );
    EXPECT_EQ( 12, p.colour );
    EXPECT_EQ( 16, p.offColour );
    EXPECT_EQ( 127, p.velocity );
    EXPECT_EQ( R::Malformed, pad( contract::PAD_SET_BLINKING, { 2, 4, 12 } ) );
    EXPECT_EQ( R::OutOfRange, pad( contract::PAD_SET_BLINKING, { 16, 4, 12, 16 } ) );
}

TEST_F( Decode, PadSetStepSingle )
{
    EXPECT_EQ( R::Applied, pad( contract::PAD_SET_STEP_SINGLE_PAD, { 9, ON, 6, 64 } ) ); // setPadColourByVelocityMessage
    const PadLed& p = table().pads[9];
    EXPECT_EQ( ON, p.state );
    EXPECT_EQ( 6, p.colour );
    EXPECT_EQ( TRANS, p.offColour );
    EXPECT_EQ( 64, p.velocity );
    EXPECT_EQ( R::Malformed, pad( contract::PAD_SET_STEP_SINGLE_PAD, { 9, ON, 6 } ) );
    EXPECT_EQ( R::OutOfRange, pad( contract::PAD_SET_STEP_SINGLE_PAD, { 200, ON, 6, 64 } ) );
}

TEST_F( Decode, PadSetStepAllIs33BytesOnTheWire )
{
    Bytes p{ ON };
    for( uint8_t i = 0; i < 16; ++i )
        p.push_back( i );
    for( uint8_t i = 0; i < 16; ++i )
        p.push_back( (uint8_t)( 100 + i ) );
    ASSERT_EQ( 33u, p.size() );
    EXPECT_EQ( R::Applied, pad( contract::PAD_SET_STEP_ALL_PAD, p ) ); // the largest frame the host sends
    for( uint8_t i = 0; i < 16; ++i )
    {
        EXPECT_EQ( i, table().pads[i].colour );
        EXPECT_EQ( 100 + i, table().pads[i].velocity );
        EXPECT_EQ( ON, table().pads[i].state );
    }

    // Three pads: [state, c0 c1 c2, v0 v1 v2]
    EXPECT_EQ( R::Applied, pad( contract::PAD_SET_STEP_ALL_PAD, { OFF, 17, 16, 15, 1, 2, 3 } ) );
    EXPECT_EQ( 17, table().pads[0].colour );
    EXPECT_EQ( 3, table().pads[2].velocity );
    EXPECT_EQ( 3, table().pads[3].colour );

    EXPECT_EQ( R::Malformed, pad( contract::PAD_SET_STEP_ALL_PAD, { ON, 1, 2, 3 } ) ) << "even length";
    EXPECT_EQ( R::Malformed, pad( contract::PAD_SET_STEP_ALL_PAD, { ON } ) );
    EXPECT_EQ( R::Malformed, pad( contract::PAD_SET_STEP_ALL_PAD, Bytes( 35, 1 ) ) ) << "17 pads";
}

TEST_F( Decode, ASuccessfulMultiPadCommandIsOneCommit )
{
    const uint32_t before = state.commits();
    Bytes          p{ ON };
    for( uint8_t i = 0; i < 16; ++i )
        p.push_back( i );
    pad( contract::PAD_SET_ALL_MULTI_COLOUR, p );
    EXPECT_EQ( before + 1, state.commits() );
    pad( contract::PAD_SET_ALL_MULTI_COLOUR, { ON } ); // malformed
    EXPECT_EQ( before + 1, state.commits() ) << "a rejected command publishes nothing";
}

TEST_F( Decode, AftertouchEnableIsTheInputLayersAndPadStateIsNotACommand )
{
    EXPECT_EQ( R::NotMine, pad( contract::PAD_SET_AFTER_TOUCH, { 1 } ) );
    EXPECT_EQ( R::Unknown, pad( contract::PAD_STATE, { 0, 1, 100 } ) );
    EXPECT_EQ( R::Unknown, pad( 11, {} ) );
    EXPECT_EQ( R::Unknown, pad( 255, {} ) );
}

// ---- SLIDER --------------------------------------------------------------------------------

TEST_F( Decode, SliderBarAndLevelSetThePercentage )
{
    EXPECT_EQ( R::Applied, slider( contract::SLIDER_BAR_LED, { 40 } ) ); // sendSliderValueInPercentMessage
    EXPECT_EQ( 40, table().strip.percent );
    EXPECT_EQ( StripMode::Bar, table().strip.mode );

    EXPECT_EQ( R::Applied, slider( contract::SLIDER_LEVEL_LED, { 75 } ) );
    EXPECT_EQ( 75, table().strip.percent );
    EXPECT_EQ( StripMode::Level, table().strip.mode );

    EXPECT_EQ( R::Applied, slider( contract::SLIDER_BAR_LED, { 250 } ) );
    EXPECT_EQ( 100, table().strip.percent ) << "clamped, as the Surface does";

    EXPECT_EQ( R::Malformed, slider( contract::SLIDER_BAR_LED, {} ) );
    EXPECT_EQ( R::Malformed, slider( contract::SLIDER_LEVEL_LED, {} ) );
    EXPECT_EQ( 100, table().strip.percent );
}

TEST_F( Decode, SliderIndividual )
{
    EXPECT_EQ( R::Applied, slider( contract::SLIDER_BAR_LED, { 50 } ) );
    EXPECT_EQ( R::Applied, slider( contract::SLIDER_INDIVIDUAL_LED, { 3, 80 } ) );
    EXPECT_EQ( StripMode::Individual, table().strip.mode );
    EXPECT_EQ( 80, table().strip.individual[3] );
    EXPECT_EQ( 0, table().strip.individual[4] );
    EXPECT_EQ( 50, table().strip.percent ) << "percent keeps its value";

    EXPECT_EQ( R::Applied, slider( contract::SLIDER_INDIVIDUAL_LED, { 5, 250 } ) );
    EXPECT_EQ( 100, table().strip.individual[5] );
    EXPECT_EQ( 80, table().strip.individual[3] ) << "a second index adds to the first";

    EXPECT_EQ( R::Applied, slider( contract::SLIDER_LEVEL_LED, { 10 } ) );
    EXPECT_EQ( StripMode::Level, table().strip.mode );
    EXPECT_EQ( 0, table().strip.individual[3] ) << "back to a bar clears the individual levels";

    EXPECT_EQ( R::Malformed, slider( contract::SLIDER_INDIVIDUAL_LED, { 3 } ) );
    EXPECT_EQ( R::OutOfRange, slider( contract::SLIDER_INDIVIDUAL_LED, { 12, 50 } ) );
}

TEST_F( Decode, SliderTouchInputIsNotACommand )
{
    EXPECT_EQ( R::Unknown, slider( contract::SLIDER_TOUCH_INPUT, { 50 } ) );
    EXPECT_EQ( R::Unknown, slider( 4, { 50 } ) );
}

// ---- everything else -----------------------------------------------------------------------

TEST_F( Decode, UnknownLedCommandsAreCountedAndIgnored )
{
    EXPECT_EQ( R::Unknown, led( 3, { 1, 1, 1 } ) );
    EXPECT_EQ( R::Unknown, led( 200, {} ) );
    EXPECT_TRUE( untouched() );
    EXPECT_EQ( 2u, decoder.stats().unknown );
    EXPECT_EQ( contract::PKT_LED, decoder.stats().lastRejectedType );
    EXPECT_EQ( 200, decoder.stats().lastRejectedCmd );
    EXPECT_EQ( 0, decoder.stats().lastRejectedLength );
}

TEST_F( Decode, OtherPacketTypesAreNotOutputCommands )
{
    for( int type : { (int)contract::PKT_SYSTEM, (int)contract::PKT_BUTTON, (int)contract::PKT_KNOBS, (int)contract::PKT_MIDI, (int)contract::PKT_CV, 99 } )
        EXPECT_EQ( R::NotMine, send( (uint8_t)type, 0, { 1, 2, 3 } ) ) << (int)type;
    EXPECT_TRUE( untouched() );
    EXPECT_EQ( 0u, decoder.stats().unknown + decoder.stats().malformed + decoder.stats().outOfRange + decoder.stats().applied );
}

TEST_F( Decode, CountersSeparateTheKindsOfRejection )
{
    led( contract::LED_SET_SINGLE_LED, { 1, 1, 1 } );     // applied
    led( contract::LED_SET_SINGLE_LED, { 1 } );           // malformed
    led( contract::LED_SET_SINGLE_LED, { 90, 1, 1 } );    // out of range
    led( 77, {} );                                        // unknown
    EXPECT_EQ( 1u, decoder.stats().applied );
    EXPECT_EQ( 1u, decoder.stats().malformed );
    EXPECT_EQ( 1u, decoder.stats().outOfRange );
    EXPECT_EQ( 1u, decoder.stats().unknown );
}

// Garbage in every position must never touch the table or crash, whatever the type, cmd and length.
TEST_F( Decode, RandomPayloadsNeverCorruptMemory )
{
    Rng rng( 12345 );
    for( int i = 0; i < 20000; ++i )
    {
        Bytes p( rng.below( contract::kMaxPayloadFromHost + 1 ) );
        for( auto& b : p )
            b = (uint8_t)rng.next();
        send( (uint8_t)( contract::PKT_LED + rng.below( 6 ) ), (uint8_t)rng.below( 14 ), p );
    }
    const auto& st = decoder.stats();
    EXPECT_GT( st.applied, 0u );
    EXPECT_GT( st.malformed, 0u );
    EXPECT_GT( st.unknown, 0u );
    // ASan and UBSan (-DKICK_SANITIZE=ON) are the real assertion: no read or write past a table.
}
