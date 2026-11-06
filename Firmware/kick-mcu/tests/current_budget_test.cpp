// SPDX-License-Identifier: AGPL-3.0-or-later
//
// The numbers here are the ones quoted in docs/mcu-led-current-budget.md. If the defaults in
// CurrentModel change, change the document with them.
#include <gtest/gtest.h>

#include "kick_mcu/output/current_budget.h"

using namespace kick_mcu::output;

namespace
{
constexpr uint32_t kFullWhiteWall = 16 * 3 * 255;
constexpr uint32_t kAllButtons    = 53 * 255;
constexpr uint32_t kFullStrip     = 12 * 255;
} // namespace

TEST( CurrentBudget, DefaultsAreTheDocumentedAssumptions )
{
    CurrentModel m;
    EXPECT_EQ( 20, m.padChannelMa );
    EXPECT_EQ( 5, m.buttonMa );
    EXPECT_EQ( 5, m.stripMa );
    EXPECT_EQ( 600, m.capMa );
}

TEST( CurrentBudget, TheLargestPossibleDraws )
{
    CurrentModel m;
    EXPECT_EQ( 960u, estimateMa( m, { kFullWhiteWall, 0, 0 } ) ) << "16 pads * 3 channels * 20 mA";
    EXPECT_EQ( 265u, estimateMa( m, { 0, kAllButtons, 0 } ) ) << "53 buttons * 5 mA";
    EXPECT_EQ( 60u, estimateMa( m, { 0, 0, kFullStrip } ) ) << "12 strip LEDs * 5 mA";
    EXPECT_EQ( 1285u, estimateMa( m, { kFullWhiteWall, kAllButtons, kFullStrip } ) );
}

TEST( CurrentBudget, AFullWhiteWallIsScaledTo5Eighths )
{
    CurrentModel m;
    // 600 / 960 = 0.625 = 160 / 256
    EXPECT_EQ( 160, scaleQ8( m, { kFullWhiteWall, 0, 0 } ) );
}

TEST( CurrentBudget, EverythingOnIsScaledBelowHalf )
{
    CurrentModel m;
    // 600 / 1285 = 0.467; 119 / 256 = 0.465
    EXPECT_EQ( 119, scaleQ8( m, { kFullWhiteWall, kAllButtons, kFullStrip } ) );
}

TEST( CurrentBudget, UnderTheCapIsLeftAlone )
{
    CurrentModel m;
    EXPECT_EQ( 256, scaleQ8( m, {} ) );
    EXPECT_EQ( 256, scaleQ8( m, { 0, kAllButtons, kFullStrip } ) ) << "325 mA";
    // Exactly at the cap: 600 mA = 30 channels at full duty.
    EXPECT_EQ( 600u, estimateMa( m, { 30 * 255, 0, 0 } ) );
    EXPECT_EQ( 256, scaleQ8( m, { 30 * 255, 0, 0 } ) );
    EXPECT_LT( scaleQ8( m, { 30 * 255 + 1, 0, 0 } ), 256 );
}

// Scaling every duty by scaleQ8/256 (rounding down) must land at or under the cap for any load.
TEST( CurrentBudget, TheScaledLoadNeverExceedsTheCap )
{
    CurrentModel m;
    for( uint32_t pad = 0; pad <= kFullWhiteWall; pad += 977 )
        for( uint32_t btn = 0; btn <= kAllButtons; btn += 1301 )
            for( uint32_t strip = 0; strip <= kFullStrip; strip += 511 )
            {
                const LedLoad  load{ pad, btn, strip };
                const uint32_t s = scaleQ8( m, load );
                const LedLoad  scaled{ ( pad * s ) >> 8, ( btn * s ) >> 8, ( strip * s ) >> 8 };
                ASSERT_LE( estimateMa( m, scaled ), m.capMa ) << pad << " " << btn << " " << strip;
            }
}

TEST( CurrentBudget, ACapOfZeroTurnsEverythingOffWithoutDividingByZero )
{
    CurrentModel m;
    m.capMa = 0;
    EXPECT_EQ( 0, scaleQ8( m, { kFullWhiteWall, 0, 0 } ) );
    EXPECT_EQ( 256, scaleQ8( m, {} ) );
}

TEST( CurrentBudget, LargeSettingsDoNotOverflow )
{
    CurrentModel m;
    m.padChannelMa = 65535;
    m.buttonMa     = 65535;
    m.stripMa      = 65535;
    m.capMa        = 1;
    const uint16_t s = scaleQ8( m, { kFullWhiteWall, kAllButtons, kFullStrip } );
    EXPECT_LT( s, 2 );
}
