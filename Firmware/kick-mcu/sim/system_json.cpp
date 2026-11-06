// SPDX-License-Identifier: AGPL-3.0-or-later
#include "system_json.h"

#include <sstream>

namespace kick_mcu
{
namespace sim
{
    namespace
    {
        const char* stateName( system::BootState s )
        {
            switch( s )
            {
                case system::BootState::Confirmed: return "Confirmed";
                case system::BootState::TrialPending: return "TrialPending";
                case system::BootState::TrialBooted: return "TrialBooted";
            }
            return "unknown";
        }

        std::string versionText( const system::VersionNo& v ) { return std::to_string( v.major ) + "." + std::to_string( v.minor ) + "." + std::to_string( v.bug ); }
    } // namespace

    std::string bootReportJson( unsigned index, const system::BootDecision& d )
    {
        std::ostringstream o;
        o << "{\"boot\":" << index << ",\"bootable\":" << ( d.bootable ? "true" : "false" );
        if( d.bootable )
        {
            o << ",\"slot\":\"" << system::slotName( d.slot ) << "\",\"trial\":" << ( d.trial ? "true" : "false" ) << ",\"reverted\":" << ( d.reverted ? "true" : "false" )
              << ",\"fellBack\":" << ( d.fellBack ? "true" : "false" ) << ",\"recordMissing\":" << ( d.recordMissing ? "true" : "false" ) << ",\"state\":\"" << stateName( d.record.state )
              << "\",\"status\":\"" << system::imageStatusName( d.image.status ) << "\",\"version\":\"" << versionText( d.image.header.version ) << "\"";
        }
        o << "}\n";
        return o.str();
    }

    std::string flashInspectJson( bsp::IFlash& flash, const system::FlashLayout& layout )
    {
        std::ostringstream o;
        system::BootRecord r;
        o << "{\"record\":";
        if( system::BootControl( flash, layout ).load( r ) )
            o << "{\"seq\":" << r.seq << ",\"state\":\"" << stateName( r.state ) << "\",\"good\":\"" << system::slotName( r.good ) << "\",\"trial\":\"" << system::slotName( r.trial ) << "\"}";
        else
            o << "null";
        for( unsigned i = 0; i < 2; ++i )
        {
            const system::Slot       s = (system::Slot)i;
            const system::ImageCheck c = system::verifyImage( flash, layout.baseOf( s ), layout.slotBytes );
            o << ",\"" << system::slotName( s ) << "\":{\"status\":\"" << system::imageStatusName( c.status ) << "\"";
            if( c.status == system::ImageStatus::Ok )
                o << ",\"version\":\"" << versionText( c.header.version ) << "\",\"imageBytes\":" << c.header.imageBytes;
            o << "}";
        }
        o << "}\n";
        return o.str();
    }

} // namespace sim
} // namespace kick_mcu
