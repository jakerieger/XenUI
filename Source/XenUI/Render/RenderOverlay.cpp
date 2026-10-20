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

#include "XenUI/Render/RenderOverlay.hpp"

#include <algorithm>

namespace XenUI {
    Vec2 ComputePopupPosition(const Rect& Anchor, const Size& PopupSize, const Rect& Bounds, PopupPlacement Placement) {
        Vec2 Position;
        if (Placement == PopupPlacement::Below) {
            Position = {Anchor.Left(), Anchor.Bottom()};
            if (Position.Y + PopupSize.Height > Bounds.Bottom() && Anchor.Top() - PopupSize.Height >= Bounds.Top()) {
                Position.Y = Anchor.Top() - PopupSize.Height;
            }
        } else {
            Position = {Anchor.Right(), Anchor.Top()};
            if (Position.X + PopupSize.Width > Bounds.Right() && Anchor.Left() - PopupSize.Width >= Bounds.Left()) {
                Position.X = Anchor.Left() - PopupSize.Width;
            }
        }

        Position.X = std::clamp(Position.X, Bounds.Left(), std::max(Bounds.Left(), Bounds.Right() - PopupSize.Width));
        Position.Y = std::clamp(Position.Y, Bounds.Top(), std::max(Bounds.Top(), Bounds.Bottom() - PopupSize.Height));
        return Position;
    }

    void RenderAnchoredPopup::SetAnchor(const Rect& Anchor, PopupPlacement Placement) {
        if (Anchor == _Anchor && Placement == _Placement) { return; }
        _Anchor    = Anchor;
        _Placement = Placement;
        MarkNeedsLayout();
    }

    void RenderAnchoredPopup::PerformLayout() {
        const BoxConstraints& Constraints = GetConstraints();
        SetSize(Constraints.Constrain(Constraints.GetBiggest()));
        const Size& Bounds = GetSize();

        RenderBox* Child = GetFirstChild();
        if (Child == nullptr) { return; }

        Child->Layout(BoxConstraints::Loose(Bounds), true);
        Child->SetOffset(
          ComputePopupPosition(_Anchor, Child->GetSize(), {0.0f, 0.0f, Bounds.Width, Bounds.Height}, _Placement));
    }

    bool RenderIgnorePointer::HitTest(HitTestResult& Result, const Vec2& Position) {
        return !_Ignoring && RenderBox::HitTest(Result, Position);
    }
}  // namespace XenUI
