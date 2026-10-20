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

#include "XenUI/Tree/WidgetArena.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <new>
#include <vector>

namespace XenUI {
    namespace {
        constexpr size_t kBlockSize          = 64 * 1024;
        constexpr size_t kDedicatedThreshold = kBlockSize / 4;
        constexpr size_t kMaxSpareBlocks     = 4;
        constexpr size_t kBlockAlignment     = alignof(std::max_align_t);
    }  // namespace

    class ArenaBlock {
    public:
        ArenaBlock(class ThreadArena& OwningArena, size_t BlockCapacity, bool IsDedicated)
            : Owner(OwningArena), Capacity(BlockCapacity), Dedicated(IsDedicated) {}

        [[nodiscard]] std::byte* GetData() { return reinterpret_cast<std::byte*>(this) + GetHeaderSize(); }

        static constexpr size_t GetHeaderSize() {
            return (sizeof(ArenaBlock) + kBlockAlignment - 1) / kBlockAlignment * kBlockAlignment;
        }

        class ThreadArena& Owner;
        size_t Capacity;
        size_t Used        = 0;
        uint32_t LiveCount = 0;
        bool Dedicated;
    };

    class ThreadArena {
    public:
        ThreadArena() = default;

        ~ThreadArena() {
            for (ArenaBlock* Block : _Spare) {
                FreeBlock(Block);
            }
            if (_Current != nullptr && _Current->LiveCount == 0) { FreeBlock(_Current); }
        }

        void* Allocate(size_t Size, size_t Alignment, ArenaBlock*& OutBlock) {
            if (Size + Alignment > kDedicatedThreshold) {
                ArenaBlock* Block = CreateBlock(Size + Alignment, true);
                OutBlock          = Block;
                return Carve(*Block, Size, Alignment);
            }

            if (_Current == nullptr || !Fits(*_Current, Size, Alignment)) { RetireCurrent(); }
            OutBlock = _Current;
            return Carve(*_Current, Size, Alignment);
        }

        void Release(ArenaBlock& Block) noexcept {
            --_LiveAllocations;
            if (--Block.LiveCount > 0 || &Block == _Current) { return; }
            Recycle(&Block);
        }

        [[nodiscard]] WidgetArenaStats GetStats() const { return {_LiveAllocations, _ActiveBlocks, _Spare.size()}; }

    private:
        static bool Fits(ArenaBlock& Block, size_t Size, size_t Alignment) {
            const size_t Offset = (Block.Used + Alignment - 1) / Alignment * Alignment;
            return Offset + Size <= Block.Capacity;
        }

        void* Carve(ArenaBlock& Block, size_t Size, size_t Alignment) {
            const size_t Offset = (Block.Used + Alignment - 1) / Alignment * Alignment;
            Block.Used          = Offset + Size;
            ++Block.LiveCount;
            ++_LiveAllocations;
            return Block.GetData() + Offset;
        }

        void RetireCurrent() {
            if (_Current != nullptr && _Current->LiveCount == 0) {
                _Current->Used = 0;
                return;
            }

            if (!_Spare.empty()) {
                _Current = _Spare.back();
                _Spare.pop_back();
                ++_ActiveBlocks;
                return;
            }
            _Current = CreateBlock(kBlockSize, false);
        }

        ArenaBlock* CreateBlock(size_t Capacity, bool Dedicated) {
            void* Memory = ::operator new(ArenaBlock::GetHeaderSize() + Capacity, std::align_val_t {kBlockAlignment});
            ++_ActiveBlocks;
            return new (Memory) ArenaBlock(*this, Capacity, Dedicated);
        }

        void Recycle(ArenaBlock* Block) noexcept {
            --_ActiveBlocks;
            if (Block->Dedicated || _Spare.size() >= kMaxSpareBlocks) {
                FreeBlock(Block);
                return;
            }
            Block->Used = 0;
            _Spare.push_back(Block);
        }

        static void FreeBlock(ArenaBlock* Block) noexcept {
            Block->~ArenaBlock();
            ::operator delete(Block, std::align_val_t {kBlockAlignment});
        }

        ArenaBlock* _Current = nullptr;
        std::vector<ArenaBlock*> _Spare;
        size_t _LiveAllocations = 0;
        size_t _ActiveBlocks    = 0;
    };

    namespace {
        ThreadArena& GetThreadArena() {
            thread_local ThreadArena Arena;
            return Arena;
        }
    }  // namespace

    void* WidgetArena::Allocate(size_t Size, size_t Alignment, ArenaBlock*& OutBlock) {
        return GetThreadArena().Allocate(Size, std::max(Alignment, alignof(void*)), OutBlock);
    }

    void WidgetArena::Release(ArenaBlock& Block) noexcept {
        Block.Owner.Release(Block);
    }

    WidgetArenaStats WidgetArena::GetStats() {
        return GetThreadArena().GetStats();
    }
}  // namespace XenUI
