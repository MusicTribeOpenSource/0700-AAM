/*******************************************************************************
 The block below describes the properties of this module, and is read by
 the Projucer to automatically generate project code that uses it.

 BEGIN_JUCE_MODULE_DECLARATION

  ID:                 gesture
  vendor:             MUSIC Tribe, MY
  version:            0.1.0
  name:               Kick gesture control
  description:        Receives kick-gesture events over local IPC, maps them to panel input, and talks to kick-camera
  website:
  license:            AGPLv3
  dependencies:       juce_core, juce_events

 END_JUCE_MODULE_DECLARATION

*******************************************************************************/

#pragma once
#ifndef GESTURE_H_INCLUDE
    #define GESTURE_H_INCLUDE

    #include <juce_core/juce_core.h>
    #include <juce_events/juce_events.h>

    #include "contracts/gesture/gesture_event.h"
    #include "contracts/gesture/gesture_ipc_keys.h"
    #include "contracts/gesture/gesture_ipc_message_types.h"
    #include "contracts/gesture/gesture_signals.h"

    #include "core/backoff.h"
    #include "core/camera_status.h"
    #include "core/gesture_gate.h"
    #include "core/gesture_mapping.h"
    #include "core/gesture_router.h"
    #include "core/gesture_smoother.h"

    #include "ipc/gesture_decode.h"
    #include "ipc/gesture_ipc_server.h"
    #include "camera/camera_service.h"
#endif
