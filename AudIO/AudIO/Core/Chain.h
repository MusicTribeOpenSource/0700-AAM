#pragma once

#include <unordered_map>
#include <list>

#include "IChain.h"
#include "ObjectMeta.h"
#include "insertMeta.h"
#include "../Objects/LevelMeter.h"

namespace AudIO
{
    class Chain : public IChain
        , public Object
    {
    public:
        Chain( bool stereo = true, uint32_t capacity = 8 );
        virtual ~Chain();

        void initializeChain( objectMeta* meta );

        bool     changeCapacity( uint32_t newcapacity ) override;
        uint32_t createInsert( const char* type ) override;
        uint32_t createInsert( std::unique_ptr<Object> obj ) override;
        Object*  getObject( uint32_t id ) override;
        bool     insertObject( uint32_t id, uint32_t pos ) override;
        bool     removeInsertById( uint32_t id ) override;
        bool     removeInsertByPos( uint32_t pos ) override;
        bool     deleteInsert( uint32_t id ) override;

        void  setParameter( int index, float value ) override;
        float getParameter( int index ) override;

        std::pair<float, float> getLevelAndPan() const;
        void                    setLevelAndPan( float Gain, float Pan );

        void enableInputMetering( bool Status ) override { inMeterEnabled.store( Status ); }
        void enableOutputMetering( bool Status ) override { outMeterEnabled.store( Status ); }

        MeterData& readInMeter() override;
        MeterData& readOutMeter() override;

        void prepareToPlay( uint32_t samplerate, uint32_t blocksize ) override;
        void process( Buffer<float>& Buffer ) override;

    private:
        void patchChainInputToFirstInput();
        void patchLastOutputToChainOutput();
        bool existsInObjects( uint32_t id );
        bool existsInInserts( uint32_t id );
        bool removeInsertByIterator( std::list<insertMeta>::iterator it );

    private:
        bool m_bypass{ false };
        bool m_mute{ false };
        bool m_stereo{ true };

        std::atomic<bool> inMeterEnabled{ false };
        std::atomic<bool> outMeterEnabled{ false };

        RampedParameter<float> m_bypassGain{ 1.0f };
        RampedParameter<float> m_muteGain{ 1.0f };
        RampedParameter<float> m_gain{ 1.0f };
        RampedParameter<float> m_pan{ 0.f };

        std::atomic<bool> m_downmixStereoToMono{ false };
        std::atomic<bool> m_downmixToLeft{ true };

        std::list<insertMeta>                                 _inserts;
        std::unordered_map<uint32_t, std::unique_ptr<Object>> _objects;
        objectMeta*                                           _chainmeta = nullptr;

        LevelMeter metering;
        MeterData  inMeter;
        MeterData  outMeter;
    };
} // Namespace AudIO
