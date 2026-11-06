// SPDX-License-Identifier: AGPL-3.0-or-later
#include "kick_mcu/system/self_check.h"

namespace kick_mcu
{
namespace system
{
    SelfCheckResult runSelfCheck( bsp::IFlash* flash, const FlashLayout& layout, const BootDecision& boot, const VersionNo& buildVersion )
    {
        SelfCheckResult r;
        r.version = buildVersion;
        if( flash == nullptr || !layout.fits( *flash ) )
            return r; // NoFlash

        r.slot     = boot.slot;
        r.trial    = boot.trial;
        r.reverted = boot.reverted;
        r.fellBack = boot.fellBack;

        const ImageCheck c = verifyImage( *flash, layout.baseOf( boot.slot ), layout.slotBytes );
        r.status           = c.status;
        if( c.status == ImageStatus::Ok || c.status == ImageStatus::BadPayloadCrc )
        {
            r.flashState    = c.header.flashState;
            r.storedCrc32   = c.header.payloadCrc32;
            r.computedCrc32 = c.computedCrc32;
        }
        if( c.status == ImageStatus::Ok )
        {
            r.version    = c.header.version;
            r.swRevision = c.header.swRevision;
        }
        return r;
    }

    size_t encodeVersionReport( const SelfCheckResult& r, uint8_t out[kVersionReportBytes] )
    {
        out[0] = r.version.major;
        out[1] = r.version.minor;
        out[2] = r.version.bug;
        out[3] = (uint8_t)r.status;
        for( int i = 0; i < 4; ++i )
        {
            out[4 + i]  = (uint8_t)( r.flashState >> ( 24 - 8 * i ) );
            out[8 + i]  = (uint8_t)( r.computedCrc32 >> ( 24 - 8 * i ) );
        }
        out[12] = (uint8_t)( r.swRevision >> 8 );
        out[13] = (uint8_t)( r.swRevision & 0xFF );
        out[14] = (uint8_t)( ( r.slot == Slot::B ? 1 : 0 ) | ( r.trial ? 2 : 0 ) | ( r.reverted ? 4 : 0 ) | ( r.fellBack ? 8 : 0 ) );
        return kVersionReportBytes;
    }

} // namespace system
} // namespace kick_mcu
