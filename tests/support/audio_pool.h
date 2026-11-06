#pragma once
// AudIO draws every Buffer from one process-wide MemoryPool. Initialise it once
// with enough blocks for any single test; Buffers return their blocks on destruction.

#include <mutex>

#include "MemoryPool.h"

namespace kicktest
{
    constexpr uint32_t kBlockSize = 480;   // 10 ms @ 48 kHz
    constexpr uint16_t kPoolBlocks = 512;

    inline void ensureAudioPool()
    {
        static std::once_flag once;
        static float*         storage = new float[kPoolBlocks * kBlockSize]; // lives for the process
        std::call_once( once, [] { AudIO::MemoryPool<float>::get().initialize( storage, kPoolBlocks, kBlockSize ); } );
    }
}
