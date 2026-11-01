
#include <thread>
#include "DawBus.h"
#include <algorithm>

namespace AudIO
{
    struct busIDs
    {
        std::vector<uint32_t> monoIns;
        std::vector<uint32_t> stereoIns;
        std::vector<uint32_t> monoOuts;
        std::vector<uint32_t> stereoOuts;
        std::vector<uint32_t> sendReturns;
        std::vector<uint32_t> submixes;
        std::vector<uint32_t> aux;
        std::vector<uint32_t> objects;
        std::vector<uint32_t> others;

        std::vector<uint32_t> ins;
        std::vector<uint32_t> outs;
    };

    static busIDs gBuses;

    DawRouting::DawRouting( Design& Design_, BusType Type_, BusWidth Width_ )
        : design_( Design_ )
        , type_( Type_ )
        , width_( Width_ ) {}

    DawRouting::~DawRouting()
    {
        removeFromSends();
    }

    int DawRouting::getNumIns() { return (int)gBuses.ins.size(); }
    int DawRouting::getNumMonoIns() { return (int)gBuses.monoIns.size(); }
    int DawRouting::getNumStereoIns() { return (int)gBuses.stereoIns.size(); }
    int DawRouting::getNumOuts() { return (int)gBuses.outs.size(); }
    int DawRouting::getNumMonoOuts() { return (int)gBuses.monoOuts.size(); }
    int DawRouting::getNumStereoOuts() { return (int)gBuses.stereoOuts.size(); }
    int DawRouting::getNumSends() { return (int)gBuses.sendReturns.size(); }
    int DawRouting::getNumReturns() { return (int)gBuses.sendReturns.size(); }
    int DawRouting::getNumSubmixes() { return (int)gBuses.submixes.size(); }
    int DawRouting::getNumAuxes() { return (int)gBuses.aux.size(); }
    int DawRouting::getNumObjects() { return (int)gBuses.objects.size(); }
    int DawRouting::getNumOthers() { return (int)gBuses.others.size(); }

    // Default Bus...
    std::vector<uint32_t>& DawRouting::getBusIDs( BusType Type )
    {
        static std::vector<uint32_t> _emptyVectorIds;
        switch( Type )
        {
            case BusType::In: return gBuses.ins; break;
            case BusType::Out: return gBuses.outs; break;
            case BusType::SendReturn: return gBuses.sendReturns; break;
            case BusType::Submix: return gBuses.submixes; break;
            case BusType::Aux: return gBuses.aux; break;
            case BusType::Object: return gBuses.objects; break;
        }
        return _emptyVectorIds;
    }

    uint32_t DawRouting::getBusID( BusType Type, uint32_t Position )
    {
        auto& idVector = getBusIDs( Type );
        return ( Position < idVector.size() ) ? idVector[Position] : 0;
    }

    uint32_t DawRouting::getDefaultStereoInID() { return getStereoInID( 0 ); }
    uint32_t DawRouting::getDefaultMonoInID() { return getMonoInID( 0 ); }
    uint32_t DawRouting::getStereoInID( uint32_t Position ) { return ( Position < gBuses.stereoIns.size() ) ? gBuses.stereoIns[Position] : 0; }
    uint32_t DawRouting::getMonoInID( uint32_t Position ) { return ( Position < gBuses.monoIns.size() ) ? gBuses.monoIns[Position] : 0; }

    uint32_t DawRouting::getDefaultStereoOutID() { return getStereoOutID( 0 ); }
    uint32_t DawRouting::getDefaultMonoOutID() { return getMonoOutID( 0 ); }
    uint32_t DawRouting::getStereoOutID( uint32_t Position ) { return ( Position < gBuses.stereoOuts.size() ) ? gBuses.stereoOuts[Position] : 0; }
    uint32_t DawRouting::getMonoOutID( uint32_t Position ) { return ( Position < gBuses.monoOuts.size() ) ? gBuses.monoOuts[Position] : 0; }

    uint32_t DawRouting::getSendReturnID( uint32_t Position ) { return getBusID( BusType::SendReturn, Position ); }
    uint32_t DawRouting::getSubmixID( uint32_t Position ) { return getBusID( BusType::Submix, Position ); }
    uint32_t DawRouting::getAuxID( uint32_t Position ) { return getBusID( BusType::Aux, Position ); }
    uint32_t DawRouting::getInID( uint32_t Position ) { return getBusID( BusType::In, Position ); }
    uint32_t DawRouting::getOutID( uint32_t Position ) { return getBusID( BusType::Out, Position ); }

    // Connection API's
    bool DawRouting::connectInputTo( uint32_t ToId )
    {
        if( BusType::Object == type_ || BusType::Other == type_ )
        {
            auto retVal = design_.patch( ToId, id_ );
            // Allow time for audio design to propagate changes in patch info...
            std::this_thread::sleep_for( std::chrono::milliseconds( 20 ) );
            return retVal;
        }
        return false;
    }

    bool DawRouting::connectOutputTo( uint32_t ToId, int position, float Gain )
    {
        uint32_t connectIndex = ( 0 > position ) ? (uint32_t)connectedTo_.size() : (uint32_t)position;
        if( connectIndex < connectedTo_.size() )
        {
            if( connectedTo_[connectIndex] != ToId )
            {
                design_.removeFromBus( connectedTo_[connectIndex], id_ );
                connectedTo_[connectIndex] = 0;
                // Allow time for audio design to propagate changes in patch info...
                std::this_thread::sleep_for( std::chrono::milliseconds( 20 ) );
            }
        }
        else
        {
            connectedTo_.resize( ( connectIndex + 1 ), 0 );
        }
        auto retVal = design_.sendToBus( ToId, id_, Gain );
        // Allow time for audio design to propagate changes in patch info...
        std::this_thread::sleep_for( std::chrono::milliseconds( 20 ) );
        if( retVal )
            connectedTo_[connectIndex] = ToId;
        return retVal;
    }

    bool DawRouting::isConnectedTo( uint32_t BusId )
    {
        return std::find( connectedTo_.begin(), connectedTo_.end(), BusId ) != connectedTo_.end();
    }

    const uint32_t DawRouting::getConnectedTo( uint32_t position ) const
    {
        return ( position < connectedTo_.size() ) ? connectedTo_[position] : 0;
    }

    //
    // Helper functions...
    //
    bool DawRouting::isInBus( const uint32_t BusId ) const
    {
        auto isMono   = std::find( gBuses.monoIns.begin(), gBuses.monoIns.end(), BusId ) != gBuses.monoIns.end();
        auto isStereo = std::find( gBuses.stereoIns.begin(), gBuses.stereoIns.end(), BusId ) != gBuses.stereoIns.end();
        return ( isMono || isStereo );
    }

    bool DawRouting::isOutBus( const uint32_t BusId ) const
    {
        auto isMono   = std::find( gBuses.monoOuts.begin(), gBuses.monoOuts.end(), BusId ) != gBuses.monoOuts.end();
        auto isStereo = std::find( gBuses.stereoOuts.begin(), gBuses.stereoOuts.end(), BusId ) != gBuses.stereoOuts.end();
        return ( isMono || isStereo );
    }

    bool DawRouting::isSendBus( const uint32_t BusId ) const
    {
        return std::find( gBuses.sendReturns.begin(), gBuses.sendReturns.end(), BusId ) != gBuses.sendReturns.end();
    }
    bool DawRouting::isReturnsBus( const uint32_t BusId ) const
    {
        return isSendBus( BusId );
    }
    bool DawRouting::isAuxBus( const uint32_t BusId ) const
    {
        return std::find( gBuses.aux.begin(), gBuses.aux.end(), BusId ) != gBuses.aux.end();
    }
    bool DawRouting::isSubmixBus( const uint32_t BusId ) const
    {
        return std::find( gBuses.submixes.begin(), gBuses.submixes.end(), BusId ) != gBuses.submixes.end();
    }

    bool DawRouting::isRoutingToSendAllowed( const uint32_t BusId )
    {
        auto isMonoOut    = std::find( gBuses.monoOuts.begin(), gBuses.monoOuts.end(), BusId ) != gBuses.monoOuts.end();
        auto isStereoOut  = std::find( gBuses.stereoOuts.begin(), gBuses.stereoOuts.end(), BusId ) != gBuses.stereoOuts.end();
        auto isSendReturn = std::find( gBuses.sendReturns.begin(), gBuses.sendReturns.end(), BusId ) != gBuses.sendReturns.end();

        // Output or Send/Return can not be routed to any send bus.
        return !( isMonoOut || isStereoOut || isSendReturn );
    }

    void DawRouting::initAudioRouting( bool RouteToSend )
    {
        if( !initialized_ )
        {
            // Configure this bus to downmix STEREO audio to MONO channel (Left or Right).
            switch( width_ )
            {
                case BusWidth::Mono_L:
                    design_.setParameter( id_, BusOrChain::kParamDownMixToLeft, 1.f );
                    break;
                case BusWidth::Mono_R:
                    design_.setParameter( id_, BusOrChain::kParamDownMixToRight, 1.f );
                    break;
                default:
                    design_.setParameter( id_, BusOrChain::kParamStereo, 1.f );
                    break;
            }

            // Create default routing...
            switch( type_ )
            {
                case BusType::In:
                    gBuses.ins.push_back( id_ );
                    if( isStereo() )
                        gBuses.stereoIns.push_back( id_ );
                    else
                        gBuses.monoIns.push_back( id_ );
                    break;
                case BusType::Out:
                    gBuses.outs.push_back( id_ );
                    if( isStereo() )
                        gBuses.stereoOuts.push_back( id_ );
                    else
                        gBuses.monoOuts.push_back( id_ );
                    break;
                case BusType::SendReturn:
                    connectOutputTo( DawBus::getDefaultStereoOutID(), 0, 1.f );
                    gBuses.sendReturns.push_back( id_ );
                    break;
                case BusType::Submix:
                    connectOutputTo( DawBus::getDefaultStereoOutID(), 0, 1.f );
                    gBuses.submixes.push_back( id_ );
                    if( RouteToSend )
                        routeToSends();
                    break;
                case BusType::Aux:
                    connectOutputTo( DawBus::getDefaultStereoOutID(), 0, 1.f );
                    gBuses.aux.push_back( id_ );
                    break;
                case BusType::Object:
                    connectOutputTo( DawBus::getDefaultStereoOutID(), 0, 1.f );
                    gBuses.objects.push_back( id_ );
                    if( RouteToSend )
                        routeToSends();
                    break;
                default:
                    connectOutputTo( DawBus::getDefaultStereoOutID(), 0, 1.f );
                    gBuses.others.push_back( id_ );
                    break;
            }

            initialized_ = true;
        }
    }

    //
    // Protected functions...
    //

    void DawRouting::routeToSends()
    {
        for( auto sendBus : gBuses.sendReturns )
        {
            design_.sendToBus( sendBus, id_, 0.f );
        }
        // Allow time for audio design to propagate changes in patch info...
        std::this_thread::sleep_for( std::chrono::milliseconds( 20 ) );
    }

    void DawRouting::removeFromSends()
    {
        for( auto sendBus : gBuses.sendReturns )
        {
            design_.removeFromBus( sendBus, id_ );
        }
        // Allow time for audio design to propagate changes in patch info...
        std::this_thread::sleep_for( std::chrono::milliseconds( 20 ) );
    }

    void DawRouting::resetRouting()
    {
        auto max = (int)gBuses.sendReturns.size();
        for( auto i = 0; i < max; ++i )
        {
            design_.setBusInputGain( gBuses.sendReturns[i], id_, 0.f );
        }

        switch( type_ )
        {
            default:
            case BusType::SendReturn:
            case BusType::Submix:
            case BusType::Aux:
            case BusType::Object:
                connectOutputTo( DawBus::getDefaultStereoOutID(), 0, 1.f );
                break;
            case BusType::In:
            case BusType::Out:
                break;
        }
    }

    // Send Gain API's
    bool DawRouting::setGainForSend( int SendPosition, float Gain )
    {
        if( utils::isPositiveAndBelow( SendPosition, (int)gBuses.sendReturns.size() ) )
        {
            auto gain = utils::clamp( Gain, 0.f, 1.f );
            design_.setBusInputGain( gBuses.sendReturns[SendPosition], id_, gain );
            return true;
        }
        return false;
    }

    void DawRouting::setGainsForSend( const std::vector<float>& Gains )
    {
        auto max = (int)utils::min( Gains.size(), gBuses.sendReturns.size() );
        for( auto SendPosition = 0; SendPosition < max; ++SendPosition )
        {
            design_.setBusInputGain( gBuses.sendReturns[SendPosition], id_, Gains[SendPosition] );
        }
    }

    float DawRouting::getGainForSends( int SendPosition ) const
    {
        if( utils::isPositiveAndBelow( SendPosition, (int)gBuses.sendReturns.size() ) )
        {
            return design_.getBusInputGain( gBuses.sendReturns[SendPosition], id_ );
        }
        return 0.f;
    }

    std::vector<float> DawRouting::getGainForSends() const
    {
        std::vector<float> gains;
        for( auto sendBus : gBuses.sendReturns )
        {
            gains.push_back( design_.getBusInputGain( sendBus, id_ ) );
        }
        return gains;
    }

};
