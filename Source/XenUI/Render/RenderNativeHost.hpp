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
#include "XenUI/Graphics/Layer.hpp"
#include "XenUI/Render/RenderBox.hpp"

#include <functional>
#include <optional>

namespace XenUI {
    /// @brief A platform window handle (an HWND on Windows) as an opaque pointer.
    using NativeHandle = void*;

    /// @brief Hosts a native child window inside the widget tree. The container window is created on first paint
    /// and kept at this box's place on screen: moved, resized, clipped by enclosing scroll views and clips, and
    /// hidden when the box is not shown. Native windows draw above XenUI content in the same window, so overlays
    /// drawn inside the window (not popups or tooltips, which are separate windows) appear beneath them.
    class XENUI_API RenderNativeHost final : public RenderBox {
    public:
        struct Callbacks {
            /// @brief The container exists; create or attach native content as its child.
            std::function<void(NativeHandle Container)> OnAttach;
            /// @brief The container is about to be destroyed; detach native content.
            std::function<void(NativeHandle Container)> OnDetach;
            /// @brief The container's size in physical pixels changed.
            std::function<void(int Width, int Height)> OnResize;
        };

        ~RenderNativeHost() override;

        void SetCallbacks(Callbacks NewCallbacks) { _Callbacks = std::move(NewCallbacks); }
        void SetPreferredSize(std::optional<float> Width, std::optional<float> Height);

        [[nodiscard]] NativeHandle GetContainer() const { return _Container; }
        [[nodiscard]] bool IsRepaintBoundary() const override { return true; }
        void Paint(PaintContext& Context) override;

    protected:
        void PerformLayout() override;
        void OnDetached() override;
        [[nodiscard]] bool HitTestSelf(const Vec2& Position) const override;

    private:
        void Place(const std::optional<NativePlacement>& Placement);
        void DestroyContainer();

        Callbacks _Callbacks;
        std::optional<float> _Width;
        std::optional<float> _Height;
        NativeHandle _Container = nullptr;
        NativeHandle _Parent    = nullptr;
        int _PixelWidth         = 0;
        int _PixelHeight        = 0;
    };
}  // namespace XenUI
