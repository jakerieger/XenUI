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

#include "XenUI/Render/RenderNativeHost.hpp"

#include <cstdio>
#include <mutex>

// SDL can't embed native child windows, so on Linux a native host only reserves its space and never calls
// OnAttach. Content that needs a native control has to be built from XenUI widgets there.

namespace XenUI {
    RenderNativeHost::~RenderNativeHost() {
        DestroyContainer();
    }

    void RenderNativeHost::SetPreferredSize(std::optional<float> Width, std::optional<float> Height) {
        if (Width == _Width && Height == _Height) { return; }
        _Width  = Width;
        _Height = Height;
        MarkNeedsLayout();
    }

    void RenderNativeHost::PerformLayout() {
        const BoxConstraints& Constraints = GetConstraints();
        const float Width  = _Width.value_or(Constraints.HasBoundedWidth() ? Constraints.MaxWidth : 0.0f);
        const float Height = _Height.value_or(Constraints.HasBoundedHeight() ? Constraints.MaxHeight : 0.0f);
        SetSize(Constraints.Constrain({Width, Height}));
    }

    bool RenderNativeHost::HitTestSelf(const Vec2& Position) const {
        (void)Position;
        return true;
    }

    void RenderNativeHost::Paint(PaintContext& Context) {
        (void)Context;
        static std::once_flag Warned;
        std::call_once(Warned, [] {
            std::fputs("XenUI: NativeHost has no native child windows on Linux; its area stays empty.\n", stderr);
        });
    }

    void RenderNativeHost::Place(const std::optional<NativePlacement>& Placement) {
        (void)Placement;
    }

    void RenderNativeHost::OnDetached() {
        DestroyContainer();
        RenderBox::OnDetached();
    }

    void RenderNativeHost::DestroyContainer() {
        _Container   = nullptr;
        _Parent      = nullptr;
        _PixelWidth  = 0;
        _PixelHeight = 0;
    }
}  // namespace XenUI
