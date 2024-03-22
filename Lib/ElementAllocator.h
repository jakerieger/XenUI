// Copyright (C) 2024 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

// Author: Jake Rieger
// Created: 3/21/2024.
//

#pragma once

#include "XenAPI.h"
#include "UI/Element.h"

namespace Xen {
    static constexpr size_t ELEMENT_COUNT =
      100;  // TODO: Make this user-configurable in some manner
    static constexpr size_t ELEMENT_SIZE = sizeof(Element);
    static constexpr size_t ARENA_SIZE   = ELEMENT_COUNT * ELEMENT_SIZE;

    /// @brief Implements a simple arena allocator used to manage the UI element tree
    class XEN_API ElementAllocator {
    public:
        ElementAllocator() = default;
        void* Allocate(size_t size);
        void Reset();
        void* GetArena();
        size_t GetUsed() const;

    private:
        char Arena[ARENA_SIZE] = {0};
        size_t Used            = 0;
    };
}  // namespace Xen