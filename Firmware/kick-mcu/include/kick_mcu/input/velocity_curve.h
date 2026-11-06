// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Table-driven pad velocity curve.
#pragma once

#include <cstddef>
#include <cstdint>

namespace kick_mcu
{
namespace input
{
    /// Maps a normalised strike strength (0 to 127) to the MIDI velocity the host receives. Feel
    /// is a taste call, so the shape is data: a 128-entry table that can be replaced at run time.
    /// Every entry is 1 to 127, because velocity 0 is note-off in MIDI.
    class VelocityCurve
    {
    public:
        static constexpr size_t kSize = 128;

        enum class Preset : uint8_t
        {
            Linear, ///< out = in
            Soft,   ///< out = 127 * (in / 127) ^ 0.6: light strikes come out louder
            Hard,   ///< out = 127 * (in / 127) ^ 1.6: it takes a firm strike to get loud
        };

        VelocityCurve() { load( Preset::Linear ); }
        explicit VelocityCurve( Preset preset ) { load( preset ); }

        void load( Preset preset );

        /// Replaces the table. Entries outside 1 to 127 are clamped into it. Returns false and
        /// leaves the curve alone when `table` is null.
        bool set( const uint8_t* table, size_t count );

        uint8_t map( unsigned strength ) const { return table_[strength < kSize ? strength : kSize - 1]; }
        const uint8_t* table() const { return table_; }

    private:
        uint8_t table_[kSize]{};
    };

} // namespace input
} // namespace kick_mcu
