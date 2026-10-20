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

#include "XenUI/Core/SlabPool.hpp"

#include <array>
#include <new>

namespace XenUI {
    namespace {
        constexpr size_t kGranularity   = 16;
        constexpr size_t kMaxPooledSize = 1024;
        constexpr size_t kClassCount    = kMaxPooledSize / kGranularity;
        constexpr size_t kChunkSize     = 64 * 1024;

        struct FreeSlot {
            FreeSlot* Next;
        };

        struct ThreadPools {
            std::array<FreeSlot*, kClassCount> FreeLists {};
        };

        thread_local ThreadPools Pools;

        size_t GetSizeClass(size_t Size) {
            return (Size + kGranularity - 1) / kGranularity - 1;
        }

        void Refill(size_t SizeClass) {
            const size_t SlotSize = (SizeClass + 1) * kGranularity;
            auto* Chunk           = static_cast<std::byte*>(::operator new(kChunkSize));
            FreeSlot* Head        = Pools.FreeLists[SizeClass];
            for (size_t Offset = 0; Offset + SlotSize <= kChunkSize; Offset += SlotSize) {
                auto* Slot = reinterpret_cast<FreeSlot*>(Chunk + Offset);
                Slot->Next = Head;
                Head       = Slot;
            }
            Pools.FreeLists[SizeClass] = Head;
        }
    }  // namespace

    void* SlabPool::Allocate(size_t Size) {
        if (Size == 0 || Size > kMaxPooledSize) { return ::operator new(Size); }

        const size_t SizeClass = GetSizeClass(Size);
        if (Pools.FreeLists[SizeClass] == nullptr) { Refill(SizeClass); }

        FreeSlot* Slot             = Pools.FreeLists[SizeClass];
        Pools.FreeLists[SizeClass] = Slot->Next;
        return Slot;
    }

    void SlabPool::Free(void* Pointer, size_t Size) noexcept {
        if (Pointer == nullptr) { return; }
        if (Size == 0 || Size > kMaxPooledSize) {
            ::operator delete(Pointer);
            return;
        }

        const size_t SizeClass     = GetSizeClass(Size);
        auto* Slot                 = static_cast<FreeSlot*>(Pointer);
        Slot->Next                 = Pools.FreeLists[SizeClass];
        Pools.FreeLists[SizeClass] = Slot;
    }
}  // namespace XenUI
