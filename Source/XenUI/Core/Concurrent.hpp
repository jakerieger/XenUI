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

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>
#include <optional>
#include <span>
#include <type_traits>

#pragma warning(push)
#pragma warning(disable : 4324)

namespace XenUI {
    inline constexpr size_t kCacheLineSize = 64;

    /// @brief Wait-free single-producer, single-consumer FIFO of fixed capacity, for handing data from a real-time
    /// thread (such as audio) to the UI without locks or allocation. One thread may push, one other thread may pop.
    /// @tparam Capacity Number of slots; must be a power of two. One slot is always kept free.
    template<class T, size_t Capacity>
        requires(Capacity >= 2 && (Capacity & (Capacity - 1)) == 0 && std::is_trivially_copyable_v<T>)
    class SpscQueue {
    public:
        static constexpr size_t kUsableCapacity = Capacity - 1;

        /// @brief Producer side. Returns false when the queue is full.
        bool Push(const T& Value) {
            const size_t Tail = _Tail.load(std::memory_order_relaxed);
            const size_t Next = (Tail + 1) & kMask;
            if (Next == _HeadCache) {
                _HeadCache = _Head.load(std::memory_order_acquire);
                if (Next == _HeadCache) { return false; }
            }
            _Slots[Tail] = Value;
            _Tail.store(Next, std::memory_order_release);
            return true;
        }

        /// @brief Producer side. Pushes as many values as fit and returns how many were pushed.
        size_t PushRange(std::span<const T> Values) {
            size_t Pushed = 0;
            while (Pushed < Values.size() && Push(Values[Pushed])) {
                ++Pushed;
            }
            return Pushed;
        }

        /// @brief Consumer side.
        std::optional<T> Pop() {
            const size_t Head = _Head.load(std::memory_order_relaxed);
            if (Head == _TailCache) {
                _TailCache = _Tail.load(std::memory_order_acquire);
                if (Head == _TailCache) { return std::nullopt; }
            }
            T Value = _Slots[Head];
            _Head.store((Head + 1) & kMask, std::memory_order_release);
            return Value;
        }

        /// @brief Consumer side. Pops up to Destination.size() values and returns how many were popped.
        size_t PopRange(std::span<T> Destination) {
            size_t Popped = 0;
            while (Popped < Destination.size()) {
                const std::optional<T> Value = Pop();
                if (!Value) { break; }
                Destination[Popped++] = *Value;
            }
            return Popped;
        }

        /// @brief Approximate number of queued values; exact only on the consumer thread when the producer is idle.
        [[nodiscard]] size_t GetSize() const {
            return (_Tail.load(std::memory_order_acquire) - _Head.load(std::memory_order_acquire)) & kMask;
        }

    private:
        static constexpr size_t kMask = Capacity - 1;

        alignas(kCacheLineSize) std::atomic<size_t> _Head {0};
        size_t _TailCache = 0;
        alignas(kCacheLineSize) std::atomic<size_t> _Tail {0};
        size_t _HeadCache = 0;
        alignas(kCacheLineSize) std::array<T, Capacity> _Slots {};
    };

    /// @brief Hands the latest value from one producer thread to one consumer thread without locks. The producer
    /// writes into a private slot and publishes it; the consumer always sees the most recent complete value, and
    /// intermediate values may be skipped. Suited to meter levels, transport position and analysis snapshots.
    template<class T>
        requires std::is_default_constructible_v<T>
    class TripleBuffer {
    public:
        /// @brief Producer side: the slot to fill before Publish.
        [[nodiscard]] T& GetWriteSlot() { return _Slots[_Write]; }

        /// @brief Producer side: makes the write slot the latest value.
        void Publish() {
            const uint8_t Previous =
              _Shared.exchange(static_cast<uint8_t>(_Write | kFreshFlag), std::memory_order_acq_rel);
            _Write = Previous & kIndexMask;
        }

        void Publish(const T& Value) {
            GetWriteSlot() = Value;
            Publish();
        }

        /// @brief Consumer side: takes the latest published value if there is a new one. Returns whether it changed.
        bool Acquire() {
            if ((_Shared.load(std::memory_order_relaxed) & kFreshFlag) == 0) { return false; }
            const uint8_t Previous = _Shared.exchange(_Read, std::memory_order_acq_rel);
            _Read                  = Previous & kIndexMask;
            return true;
        }

        /// @brief Consumer side: the most recently acquired value.
        [[nodiscard]] const T& Read() const { return _Slots[_Read]; }

    private:
        static constexpr uint8_t kIndexMask = 0x03;
        static constexpr uint8_t kFreshFlag = 0x04;

        std::array<T, 3> _Slots {};
        alignas(kCacheLineSize) std::atomic<uint8_t> _Shared {1};
        alignas(kCacheLineSize) uint8_t _Write = 0;
        alignas(kCacheLineSize) uint8_t _Read  = 2;
    };
}  // namespace XenUI

#pragma warning(pop)
