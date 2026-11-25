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

#include "XenUI/Core/Color.hpp"
#include "XenUI/Core/Math.hpp"
#include "XenUI/Graphics/Compositor.hpp"
#include "XenUI/Input/InputDispatcher.hpp"
#include "XenUI/Input/Shortcuts.hpp"
#include "XenUI/Platform/Window.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace XenUI {
    class Layer;
    class WidgetHost;

    /// @brief The platform-neutral half of a window: its content, compositor, frame callback and shortcuts.
    ///
    /// Each platform derives its WindowImpl from this and supplies the native window, the swap chain and input
    /// translation through the hooks below. Derived classes must reset _Content before their own members that the
    /// content refers to (popup presenters, chrome) are destroyed, since base members are destroyed last.
    class WindowCore : public InputHost {
    public:
        ~WindowCore() override;

        WindowCore(const WindowCore&)            = delete;
        WindowCore& operator=(const WindowCore&) = delete;

        void SetBackground(const Color& NewBackground);
        void SetContent(const Node& Root);
        void SetRootLayer(std::shared_ptr<Layer> Root);
        [[nodiscard]] const std::shared_ptr<Layer>& GetRootLayer() const { return _Compositor.GetRoot(); }
        void RequestFrame() { _FrameRequested = true; }
        void SetFrameCallback(FrameCallback Callback);
        void SetShowDamageRegions(bool Enabled);
        void SetShortcuts(std::vector<Shortcut> Shortcuts) { _Shortcuts.SetShortcuts(std::move(Shortcuts)); }
        void SetOnClosed(std::function<void()> Callback) { _OnClosed = std::move(Callback); }
        void SetOnCloseRequested(std::function<bool()> Callback) { _OnCloseRequested = std::move(Callback); }

        /// @brief Lays the popup's content out at its natural size, up to Maximum, and returns that size.
        [[nodiscard]] Size MeasurePopup(const Size& Maximum);
        [[nodiscard]] WidgetHost* GetContentHost() const { return _Content.get(); }
        [[nodiscard]] InputDispatcher* GetInput() const;

        /// @brief Whether the window wants a frame: it has something to draw into, isn't minimized, and either a
        /// frame was requested or the compositor has damage.
        [[nodiscard]] bool HasPendingFrame() const;
        /// @brief Runs the frame callback, updates the content and composites it. Recovers from device loss.
        void RenderFrame();

        [[nodiscard]] virtual bool IsOpen() const        = 0;
        [[nodiscard]] virtual Size GetClientSize() const = 0;
        [[nodiscard]] virtual float GetDpiScale() const  = 0;
        virtual void ReleaseSwapChain()                  = 0;
        virtual void RecreateSwapChain()                 = 0;

    protected:
        WindowCore(const Color& Background, bool IsPopup);

        /// @brief Whether a swap chain (or other render target) exists to draw into.
        [[nodiscard]] virtual bool HasRenderTarget() const = 0;
        /// @brief Composites the layer tree into the render target. May throw DeviceLostError.
        virtual void RenderComposition() = 0;
        /// @brief Connects new content of a top-level window to the platform: popup presenter, native parent and
        /// title bar chrome. Not called for popups.
        virtual void ConfigureContent(WidgetHost& Content) = 0;
        /// @brief Runs after every content update, for popup resizing and accessibility.
        virtual void OnContentUpdated() = 0;
        /// @brief The tree actually shown for Root. Platforms that draw a title bar themselves wrap Root in one.
        [[nodiscard]] virtual Node WrapContent(const Node& Root) { return Root; }

        void UpdateContent();
        void RunFrameCallback();

        Color _Background;
        Compositor _Compositor;
        std::unique_ptr<WidgetHost> _Content;
        ShortcutMatcher _Shortcuts;
        Size _PopupMaximum;
        bool _IsPopup   = false;
        bool _Minimized = false;

        FrameCallback _FrameCallback;
        std::function<void()> _OnClosed;
        std::function<bool()> _OnCloseRequested;

    private:
        bool _FrameRequested  = false;
        bool _Animating       = false;
        double _LastFrameTime = 0.0;
        uint64_t _FrameIndex  = 0;
    };
}  // namespace XenUI
