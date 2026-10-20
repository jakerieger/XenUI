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
#include "XenUI/Graphics/LiveCanvas.hpp"
#include "XenUI/Reactive/Reactive.hpp"
#include "XenUI/Render/RenderBox.hpp"

#include <optional>

namespace XenUI {
    /// @brief Hosts a LiveCanvas in its own layer. Redraws every frame when continuous; otherwise when a signal read
    /// by the drawer changes, or on Invalidate. Never relays out or repaints anything else.
    class XENUI_API RenderLiveView final : public RenderBox,
                                           public Observer {
    public:
        void SetDrawer(LiveDrawer Drawer, bool Continuous);
        void SetPreferredSize(std::optional<float> Width, std::optional<float> Height);
        void Invalidate();

        [[nodiscard]] bool IsRepaintBoundary() const override { return true; }
        void Paint(PaintContext& Context) override;
        void OnDependencyChanged() override { Invalidate(); }

    protected:
        void PerformLayout() override;
        [[nodiscard]] bool HitTestSelf(const Vec2& Position) const override;

    private:
        LiveDrawer _Drawer;
        bool _Continuous = false;
        std::optional<float> _PreferredWidth;
        std::optional<float> _PreferredHeight;
    };
}  // namespace XenUI
