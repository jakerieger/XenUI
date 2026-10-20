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

#include "XenUI/Graphics/Compositor.hpp"
#include "XenUI/Input/InputDispatcher.hpp"
#include "XenUI/Input/Shortcuts.hpp"
#include "XenUI/Platform/Window.hpp"

#include <Windows.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace XenUI {
    class AccessibilityBridge;
    class TextInputClient;
    class PopupManager;
    class SystemDropRegistration;
    class WindowChrome;
    class SwapChain;
    class WidgetHost;

    struct PopupWindowOptions {
        HWND Owner = nullptr;
        /// @brief Initial top-left corner in screen pixels; it decides which monitor's DPI the popup uses.
        POINT Position    = {};
        bool ClickThrough = false;
        Color Background;
    };

    class WindowImpl final : public InputHost {
    public:
        explicit WindowImpl(const WindowDesc& Desc);
        /// @brief Creates a hidden, non-activating popup owned by Options.Owner and sized to its content.
        explicit WindowImpl(const PopupWindowOptions& Options);
        ~WindowImpl() override;

        WindowImpl(const WindowImpl&)            = delete;
        WindowImpl& operator=(const WindowImpl&) = delete;

        void Show();
        void Maximize();
        void Close();
        void SetTitle(std::string_view Title);
        void SetClientSize(const Size& ClientSize);
        void Activate();
        void SetDarkTitleBar(bool Dark);
        void SetBackground(const Color& NewBackground);
        void SetContent(const Node& Root);
        void SetRootLayer(std::shared_ptr<Layer> Root);
        [[nodiscard]] const std::shared_ptr<Layer>& GetRootLayer() const { return _Compositor.GetRoot(); }
        void RequestFrame() { _FrameRequested = true; }
        void SetFrameCallback(FrameCallback Callback);
        void SetShowDamageRegions(bool Enabled);
        void SetShortcuts(std::vector<Shortcut> Shortcuts) { _Shortcuts.SetShortcuts(std::move(Shortcuts)); }
        void SetOnClosed(std::function<void()> Callback) { _OnClosed = std::move(Callback); }

        void Hide();
        /// @brief Lays the popup's content out at its natural size, up to Maximum, and returns that size.
        [[nodiscard]] Size MeasurePopup(const Size& Maximum);
        /// @brief Moves and resizes the popup to Bounds in screen pixels and shows it without activating it.
        void PlacePopup(const RECT& Bounds);
        [[nodiscard]] WidgetHost* GetContentHost() const { return _Content.get(); }

        void SetCursor(CursorShape Shape) override;
        void BeginRelativePointer() override;
        void EndRelativePointer() override;
        std::optional<DropEffect> PerformSystemDrag(const DragData& Data, DropEffect Allowed) override;

        [[nodiscard]] InputDispatcher* GetInput() const;
        /// @brief Converts a point in screen pixels to client device-independent pixels.
        [[nodiscard]] Vec2 ScreenToDips(POINT Screen) const;

        [[nodiscard]] bool IsOpen() const { return _Hwnd != nullptr; }
        [[nodiscard]] Size GetClientSize() const;
        [[nodiscard]] float GetDpiScale() const;
        [[nodiscard]] HWND GetHwnd() const { return _Hwnd; }

        /// @brief True when a frame was requested and the window is in a state where it can be shown.
        [[nodiscard]] bool HasPendingFrame() const;
        [[nodiscard]] HANDLE GetFrameWaitable() const;
        void MarkFrameReady();

        /// @brief Runs the frame callback, then composites and presents. Recovers from device loss.
        void RenderFrame();

        void ReleaseSwapChain();
        void RecreateSwapChain();

    private:
        static LRESULT CALLBACK WindowProc(HWND Hwnd, UINT Message, WPARAM WParam, LPARAM LParam);

        LRESULT HandleMessage(UINT Message, WPARAM WParam, LPARAM LParam);
        void CenterOnMonitor(const Size& ClientSize);
        void OnResize(uint32_t Width, uint32_t Height);
        void OnDpiChanged(UINT Dpi, const RECT& SuggestedRect);
        void OnDestroy();
        void RunFrameCallback();
        void UpdateContent();

        std::optional<LRESULT> HandleInputMessage(UINT Message, WPARAM WParam, LPARAM LParam);
        std::optional<LRESULT> HandleFrameMessage(UINT Message, WPARAM WParam, LPARAM LParam);
        [[nodiscard]] int GetFrameThickness() const;
        [[nodiscard]] RECT GetFrameRect(const Size& ClientSize) const;
        void SetCaptionHover(WindowRegion Region);
        void DispatchMouse(PointerPhase Phase, MouseButton Button, WPARAM WParam, const Vec2& Position);
        void DispatchWheel(WPARAM WParam, LPARAM LParam, bool Horizontal);
        bool DispatchKey(WPARAM WParam, LPARAM LParam, bool Pressed);
        void DispatchCharacter(WPARAM WParam);
        [[nodiscard]] TextInputClient* GetTextInput() const;
        void HandleComposition(TextInputClient& Client, LPARAM Flags);
        void PlaceInputMethod(const TextInputClient& Client) const;
        void HandleRawInput(LPARAM LParam);
        void CreateNativeWindow(const wchar_t* ClassName,
                                const std::wstring& Title,
                                DWORD ExStyle,
                                HWND Owner,
                                POINT Position,
                                const std::function<void()>& Configure);
        void UpdatePopupSize();
        void CancelPointers();
        void ApplyCursor() const;
        [[nodiscard]] Vec2 ClientToDips(LPARAM LParam) const;

        HWND _Hwnd   = nullptr;
        DWORD _Style = 0;
        UINT _Dpi    = USER_DEFAULT_SCREEN_DPI;
        Color _Background;
        std::unique_ptr<SwapChain> _SwapChain;
        Compositor _Compositor;
        std::unique_ptr<PopupManager> _Popups;
        std::unique_ptr<WidgetHost> _Content;
        std::unique_ptr<SystemDropRegistration> _DropRegistration;
        std::unique_ptr<WindowChrome> _Chrome;
        std::unique_ptr<AccessibilityBridge> _Accessibility;
        bool _TrackingNonClient = false;
        bool _IsPopup           = false;
        bool _ClickThrough      = false;
        bool _InContentUpdate   = false;
        Size _PopupMaximum;

        FrameCallback _FrameCallback;
        std::function<void()> _OnClosed;
        bool _FrameRequested  = false;
        bool _Minimized       = false;
        bool _Animating       = false;
        double _LastFrameTime = 0.0;
        uint64_t _FrameIndex  = 0;

        ShortcutMatcher _Shortcuts;
        CursorShape _Cursor      = CursorShape::Arrow;
        MouseButton _HeldButtons = MouseButton::None;
        POINT _RelativeAnchor    = {};
        std::optional<POINT> _LastAbsoluteRaw;
        wchar_t _HighSurrogate  = 0;
        bool _TrackingMouse     = false;
        bool _RelativeMode      = false;
        bool _ReleasingCapture  = false;
        bool _LastSysKeyHandled = false;
    };
}  // namespace XenUI
