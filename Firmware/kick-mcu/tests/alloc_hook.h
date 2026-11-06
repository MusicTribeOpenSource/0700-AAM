// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Counts calls to global operator new while armed. alloc_hook.cpp replaces operator new for the
// whole test binary; the count only moves between arm() and disarm().
#pragma once

#include <cstddef>

namespace alloc_hook
{
void   arm();
size_t disarm(); ///< stops counting and returns allocations seen since arm()
} // namespace alloc_hook
