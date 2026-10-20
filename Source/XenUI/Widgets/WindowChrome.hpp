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
#include "XenUI/Reactive/Signal.hpp"
#include "XenUI/Render/RenderWindowChrome.hpp"
#include "XenUI/Tree/Widget.hpp"

#include <functional>
#include <memory>

namespace XenUI {
    /// @brief Window state and actions shared with title bar widgets in a window created with CustomTitleBar.
    /// Signals are tracked, so caption buttons restyle themselves on hover, press, maximize and activation.
    class XENUI_API WindowChrome {
    public:
        Signal<bool> Maximized = false;
        Signal<bool> Active    = true;
        /// @brief The caption button under the pointer, or Client for none.
        Signal<WindowRegion> Hovered = WindowRegion::Client;
        Signal<WindowRegion> Pressed = WindowRegion::Client;

        std::function<void()> Minimize;
        std::function<void()> ToggleMaximize;
        std::function<void()> Close;

        /// @brief The chrome of the window containing Context, or nullptr when it uses the system title bar.
        [[nodiscard]] static WindowChrome* Of(BuildContext& Context);
    };

    /// @internal Provided by WidgetHost so title bar widgets can find their window.
    struct WindowChromeHandle {
        WindowChrome* Chrome = nullptr;

        bool operator==(const WindowChromeHandle&) const = default;
    };

    /// @brief Makes its area drag the window, like a system title bar. Interactive widgets inside it still work.
    struct WindowDragArea {
        Node Child;

        [[nodiscard]] std::unique_ptr<RenderWindowDragArea> CreateRenderObject() const {
            return std::make_unique<RenderWindowDragArea>();
        }
        void UpdateRenderObject(RenderWindowDragArea& Target) const { (void)Target; }
    };

    struct RawCaptionButton {
        WindowRegion Region = WindowRegion::Close;
        Node Child;

        [[nodiscard]] std::unique_ptr<RenderCaptionButton> CreateRenderObject() const {
            auto Result = std::make_unique<RenderCaptionButton>();
            UpdateRenderObject(*Result);
            return Result;
        }

        void UpdateRenderObject(RenderCaptionButton& Target) const { Target.SetRegion(Region); }
    };

    /// @brief Minimize, maximize and close buttons in the Windows 11 style, for the right end of a custom title bar.
    /// Renders nothing in windows that use the system title bar.
    struct XENUI_API WindowControls {
        float ButtonWidth = 46.0f;
        float Height      = 32.0f;

        [[nodiscard]] Node Build(BuildContext& Context) const;
    };
}  // namespace XenUI
