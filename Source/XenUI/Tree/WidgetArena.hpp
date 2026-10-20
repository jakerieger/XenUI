// Copyright 2026 Jake Rieger
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "XenUI/Api.hpp"

#include <cstddef>

namespace XenUI {
    class ArenaBlock;

    struct WidgetArenaStats {
        size_t LiveAllocations = 0;
        size_t ActiveBlocks    = 0;
        size_t SpareBlocks     = 0;
    };

    /// @brief Bump allocator for widget descriptions, one per thread. Allocation is a pointer increment; each block
    /// counts its live allocations and is recycled as a whole once the last one is released. Since most widgets die
    /// together when their subtree rebuilds, this behaves like a frame arena while keeping exact lifetimes for
    /// descriptions that outlive a frame (for example, a child passed through a component's properties).
    class XENUI_API WidgetArena {
    public:
        static void* Allocate(size_t Size, size_t Alignment, ArenaBlock*& OutBlock);
        static void Release(ArenaBlock& Block) noexcept;
        [[nodiscard]] static WidgetArenaStats GetStats();
    };
}  // namespace XenUI
