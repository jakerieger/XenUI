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
#include "XenUI/Render/RenderBox.hpp"

#include <cstdint>

namespace XenUI {
    enum class PopupPlacement : uint8_t {
        /// @brief Below the anchor, flipping above it when there is no room.
        Below,
        /// @brief To the right of the anchor, flipping to its left when there is no room. For submenus.
        Right,
    };

    /// @brief Top-left position for a popup of PopupSize next to Anchor, flipped to the other side of the anchor
    /// when it would not fit and clamped inside Bounds. Any consistent units work.
    [[nodiscard]] XENUI_API Vec2 ComputePopupPosition(const Rect& Anchor,
                                                      const Size& PopupSize,
                                                      const Rect& Bounds,
                                                      PopupPlacement Placement);

    /// @brief Fills its parent and places its child next to an anchor rectangle given in its own coordinates,
    /// keeping the child inside its bounds. Only the child is hit-testable.
    class XENUI_API RenderAnchoredPopup final : public RenderBox {
    public:
        void SetAnchor(const Rect& Anchor, PopupPlacement Placement);

    protected:
        void PerformLayout() override;

    private:
        Rect _Anchor;
        PopupPlacement _Placement = PopupPlacement::Below;
    };

    /// @brief Makes its subtree invisible to hit testing.
    class XENUI_API RenderIgnorePointer final : public RenderBox {
    public:
        void SetIgnoring(bool Ignoring) { _Ignoring = Ignoring; }
        bool HitTest(HitTestResult& Result, const Vec2& Position) override;

    private:
        bool _Ignoring = true;
    };
}  // namespace XenUI
