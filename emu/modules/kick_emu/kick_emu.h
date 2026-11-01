/*******************************************************************************
 BEGIN_JUCE_MODULE_DECLARATION

  ID:                 kick_emu
  vendor:             Bombest
  version:            0.1.0
  name:               KICK emulator host
  description:        Virtual hardware, device root, control socket for the KICK desktop emulator
  website:
  license:            AGPLv3

  dependencies:       juce_core, juce_events, juce_gui_basics

 END_JUCE_MODULE_DECLARATION
*******************************************************************************/

/*
 * Runtime side of the emulator, linked into the Kick app in KICK_EMU builds.
 *
 * Environment:
 *   KICK_EMU_ROOT          device root (default ~/KickEmu)
 *   KICK_EMU_CONTROL_PORT  TCP control socket on 127.0.0.1 (default 50901)
 *   KICK_EMU_EVENT_PORT    UDP port the Surface listens on (default 50902)
 *
 * Control socket: one command per line, one reply line per command.
 *   ping                      -> ok
 *   snapshot <file.png>       -> ok <file>     screenshot of the Kick window
 *   tap <x> <y> [holdMs]      -> ok <component>  touch at window coordinates (default hold 40 ms,
 *                                                  a long press is a tap with holdMs above ~600)
 *   drag <x1> <y1> <x2> <y2> [holdMs] -> ok <component>
 *   quit                      -> ok            asks the app to quit
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once
#ifndef KICK_EMU_H_INCLUDE
    #define KICK_EMU_H_INCLUDE

    #include <juce_core/juce_core.h>
    #include <juce_events/juce_events.h>
    #include <juce_gui_basics/juce_gui_basics.h>

    #include "kick_emu_hal.h"

namespace kick_emu
{
    class Host
    {
    public:
        static Host& get();

        /// Starts the control socket. Window is the top-level Kick window.
        void start( juce::Component* Window );
        void stop();

        juce::Component* getWindow() const noexcept { return window.getComponent(); }

    private:
        Host() = default;
        class ControlServer;

        juce::Component::SafePointer<juce::Component> window;
        std::unique_ptr<ControlServer>                server;
    };
}

#endif
