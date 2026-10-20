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
#include "XenUI/Core/Color.hpp"
#include "XenUI/Core/Math.hpp"
#include "XenUI/Input/Shortcuts.hpp"
#include "XenUI/Tree/Node.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace XenUI {
    class Layer;
    class WindowImpl;

    struct WindowDesc {
        std::string Title = "XenUI";
        /// @brief Initial client area size in device-independent pixels.
        Size ClientSize   = {1280.0f, 720.0f};
        Color Background  = Color::FromHex(0x1E1E22);
        bool Resizable    = true;
        bool DarkTitleBar = true;
        /// @brief Lets the content draw the title bar: the client area covers the whole window inside the resize
        /// borders. Mark draggable areas with WindowDragArea and add WindowControls for the caption buttons.
        bool CustomTitleBar = false;
    };

    struct FrameInfo {
        /// @brief Seconds since the App was created.
        double Time = 0.0;
        /// @brief Seconds since the previous frame of an uninterrupted animation, or 0 for the first frame.
        double DeltaTime = 0.0;
        uint64_t Index   = 0;
    };

    using FrameCallback = std::function<void(const FrameInfo&)>;

    class XENUI_API Window {
    public:
        explicit Window(const WindowDesc& Desc = {});
        ~Window();

        Window(const Window&)            = delete;
        Window& operator=(const Window&) = delete;
        Window(Window&&) noexcept;
        Window& operator=(Window&&) noexcept;

        void Show();
        void Maximize();

        /// @brief Requests the window to close, as if the user clicked the close button.
        void Close();

        void SetTitle(std::string_view Title);

        /// @brief Resizes the client area, in device-independent pixels, keeping the window's position.
        void SetClientSize(const Size& ClientSize);
        /// @brief Uses the dark or light system frame, to match the content's theme.
        void SetDarkTitleBar(bool Dark);
        /// @brief Restores the window if minimized and brings it to the front.
        void Activate();
        void SetBackground(const Color& NewBackground);

        /// @brief Shows a widget tree that fills the client area. Replaces any root layer set directly.
        void SetContent(const Node& Root);

        /// @brief Sets the layer tree drawn in the client area, in device-independent pixels from the top-left.
        /// Replaces any widget content.
        void SetRootLayer(std::shared_ptr<Layer> Root);
        [[nodiscard]] const std::shared_ptr<Layer>& GetRootLayer() const;

        /// @brief Schedules one frame at the next vertical blank. Layer changes request frames automatically.
        void RequestFrame();

        /// @brief Called at the start of every frame, before compositing. Call RequestFrame from the callback to
        /// keep animating; the window renders nothing and uses no CPU while no frames are requested.
        void SetFrameCallback(FrameCallback Callback);

        /// @brief Called once the window has been destroyed, by the user or by Close.
        void SetOnClosed(std::function<void()> Callback);

        /// @brief Window-wide key bindings, tried after the focused widgets decline a key.
        void SetShortcuts(std::vector<Shortcut> Shortcuts);

        /// @brief Briefly tints every repainted region. A debugging aid for finding unnecessary invalidation.
        void SetShowDamageRegions(bool Enabled);

        [[nodiscard]] bool IsOpen() const;

        /// @brief Client area size in device-independent pixels.
        [[nodiscard]] Size GetClientSize() const;

        /// @brief Ratio of physical pixels to device-independent pixels for the monitor the window is on.
        [[nodiscard]] float GetDpiScale() const;

        /// @brief Returns the underlying HWND.
        [[nodiscard]] void* GetNativeHandle() const;

    private:
        std::unique_ptr<WindowImpl> _Impl;
    };
}  // namespace XenUI
