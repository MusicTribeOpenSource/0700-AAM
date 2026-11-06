// Mirrors AudIO/AudIO/AudIO.cpp, the product's single-translation-unit build.
// Several AudIO functions are declared in headers but defined `inline` in their
// .cpp (DesignImpl::process, LFO::nextSample_internal, ...), so they only link
// when everything lands in one TU. JUCE-only and device sources are left out.

#include "Core/Design.cpp"
#include "Core/DesignImpl.cpp"
#include "Core/Chain.cpp"
#include "Core/Bus.cpp"
#include "Core/MeterEngine.cpp"

#include "Dsp/lfo.cpp"

#include "Objects/ADSR.cpp"
#include "Objects/LevelMeter.cpp"
#include "Objects/Mixer.cpp"
#include "Objects/SineWave.cpp"
#include "Objects/VolumeAndPan.cpp"
