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

#include "XenUI/Render/RenderNativeHost.hpp"
#include "XenUI/Tree/Widget.hpp"

#include <functional>
#include <memory>
#include <optional>

namespace XenUI {
    /// @brief Embeds a native window, such as a plugin editor or a Win32 control, at this point of the layout.
    /// OnAttach receives the container window to create native content in; OnResize reports its size in physical
    /// pixels. The container follows layout, scrolling and clipping automatically. See RenderNativeHost for how
    /// native windows stack with XenUI content.
    struct NativeHost {
        std::function<void(NativeHandle Container)> OnAttach;
        std::function<void(NativeHandle Container)> OnDetach;
        std::function<void(int Width, int Height)> OnResize;
        std::optional<float> Width;
        std::optional<float> Height;

        [[nodiscard]] std::unique_ptr<RenderNativeHost> CreateRenderObject() const {
            auto Result = std::make_unique<RenderNativeHost>();
            UpdateRenderObject(*Result);
            return Result;
        }

        void UpdateRenderObject(RenderNativeHost& Target) const {
            Target.SetCallbacks({OnAttach, OnDetach, OnResize});
            Target.SetPreferredSize(Width, Height);
        }
    };
}  // namespace XenUI
