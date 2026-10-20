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

#include "XenUI/Render/RenderLive.hpp"

namespace XenUI {
    void RenderLiveView::SetDrawer(LiveDrawer Drawer, bool Continuous) {
        _Drawer     = std::move(Drawer);
        _Continuous = Continuous;
        MarkNeedsPaint();
    }

    void RenderLiveView::SetPreferredSize(std::optional<float> Width, std::optional<float> Height) {
        if (Width == _PreferredWidth && Height == _PreferredHeight) { return; }
        _PreferredWidth  = Width;
        _PreferredHeight = Height;
        MarkNeedsLayout();
    }

    void RenderLiveView::Invalidate() {
        GetLayer()->InvalidateLive();
    }

    void RenderLiveView::Paint(PaintContext& Context) {
        const std::weak_ptr<void> Alive = GetLifetimeToken();
        GetLayer()->SetLiveContent(
          GetSize(),
          [this, Alive, Drawer = _Drawer](LiveCanvas& Canvas) {
              if (Alive.expired() || !Drawer) { return; }
              TrackingScope Scope(*this);
              Drawer(Canvas);
          },
          _Continuous);
        RenderBox::Paint(Context);
    }

    void RenderLiveView::PerformLayout() {
        const BoxConstraints& Constraints = GetConstraints();
        const Size Biggest                = Constraints.GetBiggest();
        SetSize(
          Constraints.Constrain({_PreferredWidth.value_or(Biggest.Width), _PreferredHeight.value_or(Biggest.Height)}));
    }

    bool RenderLiveView::HitTestSelf(const Vec2& Position) const {
        (void)Position;
        return true;
    }
}  // namespace XenUI
