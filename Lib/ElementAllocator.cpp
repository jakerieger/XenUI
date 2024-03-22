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