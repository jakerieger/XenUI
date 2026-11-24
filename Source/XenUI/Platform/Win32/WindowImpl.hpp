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

#include "XenUI/Platform/WindowCore.hpp"

#include <Windows.h>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <variant>
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

    /// @brief A Win32 window: the HWND, its swap chain, message handling and input translation, on top of the
    /// shared WindowCore.
    class WindowImpl final : public WindowCore {
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
        void Destroy();
        void SetTitle(std::string_view Title);
        void SetClientSize(const Size& ClientSize);
        void Activate();
        void SetDarkTitleBar(bool Dark);
        void SetIcon(int ResourceId);
        void SetIconFile(const std::filesystem::path& IconFile);

        void Hide();
        /// @brief Moves and resizes the popup to Bounds in screen pixels and shows it without activating it.
        void PlacePopup(const RECT& Bounds);
        /// @brief True while one of this window's messages is being handled, including any nested message loop it
        /// started (a modal dialog, or a shell call that pumps messages). The window must not be destroyed then.
        [[nodiscard]] bool IsDispatching() const { return _DispatchDepth > 0; }
        /// @brief Asks the owner to destroy closed popups again once this window has finished its current message.
        void NotifyOwnerWhenIdle() { _NotifyOwnerWhenIdle = true; }

        void SetCursor(CursorShape Shape) override;
        void BeginRelativePointer() override;
        void EndRelativePointer() override;
        std::optional<DropEffect> PerformSystemDrag(const DragData& Data, DropEffect Allowed) override;

        /// @brief Converts a point in screen pixels to client device-independent pixels.
        [[nodiscard]] Vec2 ScreenToDips(POINT Screen) const;

        [[nodiscard]] bool IsOpen() const override { return _Hwnd != nullptr; }
        [[nodiscard]] Size GetClientSize() const override;
        [[nodiscard]] float GetDpiScale() const override;
        [[nodiscard]] HWND GetHwnd() const { return _Hwnd; }
        [[nodiscard]] void* GetNativeHandle() const { return _Hwnd; }

        [[nodiscard]] HANDLE GetFrameWaitable() const;
        void MarkFrameReady();

        void ReleaseSwapChain() override;
        void RecreateSwapChain() override;

    protected:
        [[nodiscard]] bool HasRenderTarget() const override { return _SwapChain != nullptr; }
        void RenderComposition() override;
        void ConfigureContent(WidgetHost& Content) override;
        void OnContentUpdated() override;

    private:
        static LRESULT CALLBACK WindowProc(HWND Hwnd, UINT Message, WPARAM WParam, LPARAM LParam);

        LRESULT HandleMessage(UINT Message, WPARAM WParam, LPARAM LParam);
        void CenterOnMonitor(const Size& ClientSize);
        void OnResize(uint32_t Width, uint32_t Height);
        void OnDpiChanged(UINT Dpi, const RECT& SuggestedRect);
        void OnDestroy();
        /// @brief Loads the icon source at the window's DPI and hands it to the window. Returns false if it failed.
        bool ApplyIcon();
        void ReleaseIcons();

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
        std::unique_ptr<SwapChain> _SwapChain;
        std::unique_ptr<PopupManager> _Popups;
        std::unique_ptr<SystemDropRegistration> _DropRegistration;
        std::unique_ptr<WindowChrome> _Chrome;
        std::unique_ptr<AccessibilityBridge> _Accessibility;
        bool _TrackingNonClient   = false;
        bool _ClickThrough        = false;
        bool _InContentUpdate     = false;
        bool _NotifyOwnerWhenIdle = false;
        uint32_t _DispatchDepth   = 0;

        /// @brief A resource ID in the executable (0 for its first icon) or an .ico file.
        std::variant<int, std::filesystem::path> _IconSource = 0;
        HICON _LargeIcon                                     = nullptr;
        HICON _SmallIcon                                     = nullptr;

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
