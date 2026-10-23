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

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace XenUI {
    /// @brief How long one frame took and how much work it did. Times are in milliseconds.
    struct FrameSample {
        /// @brief When the frame started, in seconds on the timer clock.
        double Time = 0.0;
        /// @brief The window's frame callback.
        float CallbackMs = 0.0f;
        /// @brief Rebuilding dirty elements.
        float BuildMs = 0.0f;
        /// @brief Laying out dirty render objects.
        float LayoutMs = 0.0f;
        /// @brief Recording dirty repaint boundaries into display lists.
        float PaintMs = 0.0f;
        /// @brief Drawing the damaged region of the layer tree.
        float DrawMs = 0.0f;
        /// @brief Presenting the swap chain.
        float PresentMs = 0.0f;
        /// @brief Waiting for the swap chain to accept a frame, which paces rendering to the display.
        float WaitMs = 0.0f;
        /// @brief Elements rebuilt.
        uint32_t Rebuilt = 0;
        /// @brief Render objects laid out.
        uint32_t LaidOut = 0;
        /// @brief Repaint boundaries painted.
        uint32_t Repainted = 0;
        /// @brief Fraction of the window redrawn, from 0 to 1.
        float Damage = 0.0f;

        /// @brief Time spent working, excluding the wait for the display.
        [[nodiscard]] float GetWorkMs() const { return CallbackMs + BuildMs + LayoutMs + PaintMs + DrawMs + PresentMs; }
    };

    /// @brief The most recent frames of one window, for diagnostics such as the frame stats overlay. UI thread only.
    class XENUI_API FrameStatistics {
    public:
        static constexpr size_t kCapacity = 240;

        void Record(const FrameSample& Sample);

        /// @brief Up to Count most recent frames, oldest first.
        [[nodiscard]] std::vector<FrameSample> GetRecent(size_t Count = kCapacity) const;
        /// @brief Frames that started at or after Start, in seconds on the timer clock.
        [[nodiscard]] size_t CountFramesSince(double Start) const;
        /// @brief Frames recorded since the window opened.
        [[nodiscard]] uint64_t GetTotalFrames() const { return _Total; }

    private:
        std::array<FrameSample, kCapacity> _Samples {};
        size_t _Next    = 0;
        size_t _Count   = 0;
        uint64_t _Total = 0;
    };

    /// @internal Work counted while a frame is built, laid out and painted. One set per thread.
    struct FrameCounters {
        uint32_t Rebuilt   = 0;
        uint32_t LaidOut   = 0;
        uint32_t Repainted = 0;

        /// @internal
        [[nodiscard]] XENUI_API static FrameCounters& Current();
    };
}  // namespace XenUI
