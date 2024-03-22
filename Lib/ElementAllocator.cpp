// Author: Jake Rieger
// Created: 3/21/2024.
//

#include "ElementAllocator.h"

#include <iostream>

namespace Xen {
    void* ElementAllocator::Allocate(const size_t size) {
        if (Used + size > ARENA_SIZE) {
            std::cerr << "(FATAL) Arena allocator out of memory!\nArena size: " << ARENA_SIZE
                      << " Bytes\nUsed: " << Used << " Bytes\n"
                      << "Tried to allocate: " << size << " Bytes\n";
            exit(1);
        }

        void* ptr = &Arena[Used];
        Used += size;
        return ptr;
    }

    void ElementAllocator::Reset() { Used = 0; }

    void* ElementAllocator::GetArena() { return &Arena[0]; }

    size_t ElementAllocator::GetUsed() const { return Used; }
}  // namespace Xen