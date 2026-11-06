/*******************************************************************************
 BEGIN_JUCE_MODULE_DECLARATION

  ID:                 PktMgr
  vendor:             MUSIC Tribe, MY
  version:            0.1.0
  name:               Control surface packet protocol
  description:        JUCE module wrapper around the in-tree ComProtocol/PktMgr sources
  website:
  license:            AGPLv3

  dependencies:

 END_JUCE_MODULE_DECLARATION
*******************************************************************************/

// The public ComProtocol/PktMgr folder has no JUCE module header. This wrapper
// gives it one without touching the vendor folder.
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include "PktMgr.hpp"
