// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <limits>
// Released engine defaults. These are software bus counts, not a claim about
// physical hardware; a board image may override them through compile settings.
#ifndef AUDIO_SAMPLE_RATE
#define AUDIO_SAMPLE_RATE 48000
#endif
#ifndef AUDIO_BLOCK_SIZE
#define AUDIO_BLOCK_SIZE 480
#endif
#ifndef AUDIO_NUM_INPUTS
#define AUDIO_NUM_INPUTS 2
#endif
#ifndef AUDIO_NUM_OUTPUTS
#define AUDIO_NUM_OUTPUTS 2
#endif
#ifndef AUDIO_NUM_SEND_RETURNS
#define AUDIO_NUM_SEND_RETURNS 4
#endif
#ifndef AUDIO_NUM_SUBMIX
#define AUDIO_NUM_SUBMIX 4
#endif
#ifndef AUDIO_NUM_AUX
#define AUDIO_NUM_AUX 0
#endif

// API ranges required by the released UI. MIDI uses its standard wire range;
// gain/pan match AudIO::Panner's documented linear ranges.
inline constexpr int UNKNOWN_VALUE = std::numeric_limits<int>::min();
namespace MIDI {
inline constexpr int MIN = 0, MAX = 127, NUMERIC_MAX = 128, STEP = 1;
inline constexpr int DEFAULT = 0, LEVEL_DEFAULT = 100, NOTE_DEFAULT = 60, MIDDLE_C = 60;
inline constexpr float SCALER = 1.f / 127.f;
}
namespace MIDI_CH { inline constexpr int MIN = 1, MAX = 16, DEFAULT = 1; }
namespace POLYPHONY { inline constexpr int MIN = 1, MAX = 64, DEFAULT = 16, STEP = 1; }
namespace GAIN { inline constexpr float MIN = 0, MAX = 1, DEFAULT = 1, STEP = .01f, ROOT_2 = .7071067811865475f; }
namespace PAN { inline constexpr float MIN = -1, MAX = 1, DEFAULT = 0, STEP = .01f; }

namespace DB { inline constexpr float MIN = -100, MAX = 0; }
