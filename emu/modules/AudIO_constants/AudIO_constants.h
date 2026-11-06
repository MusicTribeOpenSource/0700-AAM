/*******************************************************************************
 BEGIN_JUCE_MODULE_DECLARATION

  ID:                 AudIO_constants
  vendor:             Bombest
  version:            0.1.0
  name:               AudIO constants (emulator stand-in)
  description:        Audio engine constants the private AudIO repo used to provide
  website:
  license:            AGPLv3

  dependencies:

 END_JUCE_MODULE_DECLARATION
*******************************************************************************/

/*
 * Stand-in for the private AudIO_constants module, which is not part of the
 * public 0700-AAM release. Only the macros Kick actually uses are defined.
 *
 * The values are my best reading of the hardware, not copied from the vendor:
 * - 48 kHz matches rootfs_overlay/etc/asound.conf.
 * - The block size must stay at or below BLOCK_SIZE (480) in AudIO/Daw/DawEngine.cpp.
 * - I/O is one stereo pair each way. The routing UI offers "Out-1,2", "Out-1"
 *   and "Out-2" (kick_MidiControlTabs.cpp), the two mono halves of one pair.
 *   It has to be: DawBusManager (AudIO/Daw/DawBus.cpp) creates its output
 *   devices with the *input* count, so more outputs than inputs crashes.
 * - Send/return, submix and aux counts are guesses. Override any of them with
 *   -DAUDIO_NUM_SUBMIX=... etc. once a unit can be measured.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#ifndef AUDIO_SAMPLE_RATE
    #define AUDIO_SAMPLE_RATE ( 48000 )
#endif

#ifndef AUDIO_BLOCK_SIZE
    #define AUDIO_BLOCK_SIZE ( 256 )
#endif

#ifndef AUDIO_NUM_INPUTS
    #define AUDIO_NUM_INPUTS ( 2 )
#endif

#ifndef AUDIO_NUM_OUTPUTS
    #define AUDIO_NUM_OUTPUTS ( 2 )
#endif

#ifndef AUDIO_NUM_SEND_RETURNS
    #define AUDIO_NUM_SEND_RETURNS ( 4 )
#endif

#ifndef AUDIO_NUM_SUBMIX
    #define AUDIO_NUM_SUBMIX ( 8 )
#endif

#ifndef AUDIO_NUM_AUX
    #define AUDIO_NUM_AUX ( 0 )
#endif

/*
 * Parameter ranges. Kick uses these namespaces without defining them; the other
 * ranges (BPM, BAR, SEMI, FINE, ...) live in Kick/modules/common/source/kick_Constants.h
 * in the same style. Values below come from how the code uses each one:
 *
 * - PAN feeds AudIO::Panner directly, and Panner documents [-1, 1] (AudIO/Dsp/pan.h).
 * - GAIN is linear. GainConstrainer casts MIN/MAX to int, kick_KnobGrid.cpp steps it
 *   by 0.01, and kick_settings.cpp defaults a gain to ROOT_2, so the range tops out at 2.
 * - MIDI is the 0..127 value range; SCALER turns a value into 0..1 (kick_common_utils.cpp
 *   comments it as "MidiLevel / 127"). NUMERIC_MAX bounds 128-entry pad arrays.
 * - LEVEL_DEFAULT, POLYPHONY and MIDI_CH limits are educated guesses.
 */

namespace MIDI
{
    static constexpr int   MIN{ 0 };
    static constexpr int   MAX{ 127 };
    static constexpr int   NUMERIC_MAX{ 128 };
    static constexpr int   STEP{ 1 };
    static constexpr int   DEFAULT{ 0 };
    static constexpr int   LEVEL_DEFAULT{ 100 };
    static constexpr int   MIDDLE_C{ 60 };
    static constexpr int   NOTE_DEFAULT{ 60 };
    static constexpr float SCALER{ 1.0f / 127.0f };
}

namespace MIDI_CH
{
    static constexpr int MIN{ 1 };
    static constexpr int MAX{ 16 };
    static constexpr int DEFAULT{ 1 };
}

namespace POLYPHONY
{
    static constexpr int MIN{ 1 };
    static constexpr int MAX{ 32 };
    static constexpr int STEP{ 1 };
    static constexpr int DEFAULT{ 16 };
}

namespace GAIN
{
    static constexpr double MIN{ 0.0 };
    static constexpr double MAX{ 2.0 };
    static constexpr double STEP{ 0.01 };
    static constexpr double DEFAULT{ 1.0 };
    static constexpr double ROOT_2{ 1.4142135623730951 };
}

namespace PAN
{
    static constexpr double MIN{ -1.0 };
    static constexpr double MAX{ 1.0 };
    static constexpr double STEP{ 0.01 };
    static constexpr double DEFAULT{ 0.0 };
}

namespace DB
{
    static constexpr float MIN{ -96.0f };
}

/// Placeholder stored in a juce::Value before a UI attachment is bound (kick_attachments.h).
#ifndef UNKNOWN_VALUE
    #define UNKNOWN_VALUE ( -1 )
#endif
