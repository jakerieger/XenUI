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