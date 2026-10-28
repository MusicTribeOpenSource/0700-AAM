#pragma once

#include <atomic>
#include "IProcessable.h"
#include "RampedParameter.h"
#include "ObjectFactory.h"

namespace AudIO
{
    // One counter for the whole program. As a namespace-scope `static` every source file got its
    // own copy, so objects created in different files shared ids; a project's program could then
    // take the id of the audio input port and route the microphone straight to the main output.
    inline std::atomic<uint32_t> audioObjectId{ 1 };

    class Object : public IProcessable
    {
    public:
        using Ptr = Object*;

        Object( uint32_t ninputs, uint32_t noutputs, uint32_t nparam )
            : _numInputs( ninputs )
            , _numOutputs( noutputs )
            , _numParams(nparam)
        {
            _parameters.resize( nparam );
        }

        virtual ~Object() = default;

        inline uint32_t     getId() const { return _id; }
        virtual const char* getType() const { return ""; };

        virtual void               setName( const std::string& Name ) { _name = Name; }
        virtual const std::string& getName() const { return _name; }

        inline uint32_t inputCount() const { return _numInputs; }
        inline uint32_t outputCount() const { return _numOutputs; }

        inline uint32_t sampleRate() const { return _sampleRate; }
        inline uint32_t blockSize() const { return _blockSize; }
        inline float    blockRate() const { return (float)_blockSize / (float)_sampleRate; }

        virtual void prepareToPlay( uint32_t samplerate, uint32_t blocksize )
        {
            _blockSize  = blocksize;
            _sampleRate = samplerate;
        }

        virtual void stop() {}

        double      getSampleRate() const { return (double)_sampleRate; }
        virtual int getNumParameters() { return (int)_numParams; }

        virtual const std::vector<AudIO::ParamInfo>& getParameters() { return _parameters; };

        virtual float getParameter( int /*index*/ ) { return 0.f; }; // get the parameter value at the specified index
        virtual void  setParameter( int /*index*/, float /*value*/ ){}; // set the parameter at index to value
        virtual void  getParameterLabel( int /*index*/, char* /*text*/ ){}; // label for the parameter (eg dB)
        virtual void  getParameterName( int /*index*/, char* /*text*/ ){}; // name of the parameter
        virtual void  getParameterDisplay( int /*index*/, char* /*text*/ ){}; // text description of the current value

        virtual float getMin( uint32_t index ) const
        {
            index;
            return 0.f;
        }
        virtual float getMax( uint32_t index ) const
        {
            index;
            return 0.f;
        }

        virtual bool canResize() const { return false; }
        virtual bool resize( uint32_t ninputs, uint32_t noutputs )
        {
            ninputs;
            noutputs;
            return false;
        };

    protected:
        uint32_t                      _numInputs  = 0; ///< number of audio inputs
        uint32_t                      _numOutputs = 0; ///< number of audio outputs
        uint32_t                      _numParams  = 0; ///< all programs are assumed to have numParams parameters
        std::vector<AudIO::ParamInfo> _parameters;

    private:
        const uint32_t _id{ ++audioObjectId };
        std::string    _name;
        uint32_t       _sampleRate{ 48000 }; // Default sample rate...
        uint32_t       _blockSize{ 1 };
    };
} // Namespace AudIO
