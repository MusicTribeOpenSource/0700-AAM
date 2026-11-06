// SPDX-License-Identifier: AGPL-3.0-or-later
#include "kick_mcu/system/boot_control.h"

#include "kick_mcu/system/crc32.h"

namespace kick_mcu
{
namespace system
{
    namespace
    {
        constexpr uint32_t kBootMagic = 0x3143424Bu; // 'K' 'B' 'C' '1' read as a little-endian u32

        void put32( uint8_t* p, uint32_t v )
        {
            p[0] = (uint8_t)( v & 0xFF );
            p[1] = (uint8_t)( v >> 8 );
            p[2] = (uint8_t)( v >> 16 );
            p[3] = (uint8_t)( v >> 24 );
        }
        uint32_t get32( const uint8_t* p ) { return (uint32_t)p[0] | ( (uint32_t)p[1] << 8 ) | ( (uint32_t)p[2] << 16 ) | ( (uint32_t)p[3] << 24 ); }

        bool newer( uint32_t a, uint32_t b ) { return (int32_t)( a - b ) > 0; }

        void encode( const BootRecord& r, uint8_t out[kBootRecordBytes] )
        {
            put32( out, kBootMagic );
            put32( out + 4, r.seq );
            out[8]  = (uint8_t)r.state;
            out[9]  = (uint8_t)r.good;
            out[10] = (uint8_t)r.trial;
            out[11] = 0;
            put32( out + 12, crc32( out, 12 ) );
        }

        bool decode( const uint8_t in[kBootRecordBytes], BootRecord& out )
        {
            if( get32( in ) != kBootMagic || get32( in + 12 ) != crc32( in, 12 ) )
                return false;
            if( in[8] < (uint8_t)BootState::Confirmed || in[8] > (uint8_t)BootState::TrialBooted || in[9] > 1 || in[10] > 1 )
                return false;
            out.seq   = get32( in + 4 );
            out.state = (BootState)in[8];
            out.good  = (Slot)in[9];
            out.trial = (Slot)in[10];
            return true;
        }
    } // namespace

    bool BootControl::readCopy( unsigned copy, BootRecord& out ) const
    {
        uint8_t raw[kBootRecordBytes];
        if( !flash_.read( layout_.ctlBase[copy], raw, sizeof( raw ) ) )
            return false;
        return decode( raw, out );
    }

    bool BootControl::load( BootRecord& out ) const
    {
        BootRecord a, b;
        const bool okA = readCopy( 0, a );
        const bool okB = readCopy( 1, b );
        if( okA && okB )
            out = newer( b.seq, a.seq ) ? b : a;
        else if( okA )
            out = a;
        else if( okB )
            out = b;
        return okA || okB;
    }

    bool BootControl::store( BootState state, Slot good, Slot trial )
    {
        BootRecord a, b;
        const bool okA = readCopy( 0, a );
        const bool okB = readCopy( 1, b );

        // Target the copy that is not the newest valid one. With none valid, copy 0.
        unsigned target = 0;
        uint32_t seq    = 1;
        if( okA && okB )
        {
            const bool bNewer = newer( b.seq, a.seq );
            target            = bNewer ? 0 : 1;
            seq               = ( bNewer ? b.seq : a.seq ) + 1;
        }
        else if( okA )
        {
            target = 1;
            seq    = a.seq + 1;
        }
        else if( okB )
        {
            target = 0;
            seq    = b.seq + 1;
        }

        BootRecord record;
        record.seq   = seq;
        record.state = state;
        record.good  = good;
        record.trial = trial;
        uint8_t raw[kBootRecordBytes];
        encode( record, raw );

        return flash_.eraseBlock( layout_.ctlBase[target] ) && flash_.write( layout_.ctlBase[target], raw, sizeof( raw ) );
    }

    BootDecision selectBoot( bsp::IFlash& flash, const FlashLayout& layout )
    {
        BootDecision d;
        BootControl  control( flash, layout );

        auto check = [&]( Slot s ) { return verifyImage( flash, layout.baseOf( s ), layout.slotBytes ); };
        auto settle = [&]( Slot s, const ImageCheck& c ) {
            d.bootable = true;
            d.slot     = s;
            d.image    = c;
        };

        BootRecord rec;
        if( !control.load( rec ) )
        {
            d.recordMissing = true;
            for( unsigned i = 0; i < 2 && !d.bootable; ++i )
            {
                const Slot       s = (Slot)i;
                const ImageCheck c = check( s );
                if( c.status == ImageStatus::Ok )
                    settle( s, c );
            }
            if( d.bootable )
            {
                if( !control.confirm( d.slot ) )
                    d.controlWriteFailed = true;
                if( !control.load( d.record ) )
                    d.record = BootRecord{ 0, BootState::Confirmed, d.slot, d.slot };
            }
            return d;
        }

        // The state to fall back on when a trial is abandoned.
        auto bootGood = [&]( BootRecord& r ) {
            const ImageCheck c = check( r.good );
            if( c.status == ImageStatus::Ok )
            {
                settle( r.good, c );
                return;
            }
            const Slot       other = otherSlot( r.good );
            const ImageCheck oc    = check( other );
            if( oc.status == ImageStatus::Ok )
            {
                settle( other, oc );
                d.fellBack = true;
                if( !control.confirm( other ) )
                    d.controlWriteFailed = true;
                if( !control.load( r ) )
                    r = BootRecord{ r.seq, BootState::Confirmed, other, other };
            }
        };

        switch( rec.state )
        {
            case BootState::Confirmed:
                bootGood( rec );
                break;

            case BootState::TrialPending:
            {
                const ImageCheck c = check( rec.trial );
                if( c.status == ImageStatus::Ok && rec.trial != rec.good && control.store( BootState::TrialBooted, rec.good, rec.trial ) )
                {
                    settle( rec.trial, c );
                    d.trial = true;
                    control.load( rec );
                    break;
                }
                // The candidate is not valid, or the mark could not be written: stay on the old image.
                d.reverted = true;
                if( !control.confirm( rec.good ) )
                    d.controlWriteFailed = true;
                control.load( rec );
                bootGood( rec );
                break;
            }

            case BootState::TrialBooted:
                d.reverted = true;
                if( !control.confirm( rec.good ) )
                    d.controlWriteFailed = true;
                control.load( rec );
                bootGood( rec );
                break;
        }

        d.record = rec;
        return d;
    }

} // namespace system
} // namespace kick_mcu
