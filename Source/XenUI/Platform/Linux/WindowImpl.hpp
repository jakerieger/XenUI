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

#include <SDL3/SDL.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string_view>

namespace XenUI {
    class PopupManager;
    class SwapChain;
    class TextInputClient;
    class WidgetHost;
    class WindowChrome;

    struct PopupWindowOptions {
        SDL_Window* Owner = nullptr;
        bool Tooltip      = false;
        Color Background;
    };

    /// @brief An SDL3 window drawn with OpenGL, on top of the shared WindowCore. The Linux twin of
    /// Platform/Win32/WindowImpl: events arrive through HandleEvent from the run loop instead of a window procedure.
    class WindowImpl final : public WindowCore {
    public:
        explicit WindowImpl(const WindowDesc& Desc);
        /// @brief Creates a hidden popup of Options.Owner that never takes keyboard focus. Place it with PlacePopup.
        explicit WindowImpl(const PopupWindowOptions& Options);
        ~WindowImpl() override;

        WindowImpl(const WindowImpl&)            = delete;
        WindowImpl& operator=(const WindowImpl&) = delete;

        void Show();
        void Maximize();
        void Close();
        void Destroy();
        void Activate();
        void SetTitle(std::string_view Title);
        void SetClientSize(const Size& ClientSize);
        /// @brief No effect: the desktop draws title bars in its own theme.
        void SetDarkTitleBar(bool Dark);
        /// @brief No effect: Linux executables have no icon resources. Use SetIconFile.
        void SetIcon(int ResourceId);
        void SetIconFile(const std::filesystem::path& IconFile);
        [[nodiscard]] void* GetNativeHandle() const { return _Window; }
        [[nodiscard]] SDL_Window* GetSdlWindow() const { return _Window; }

        void Hide();
        /// @brief Moves and resizes the popup to Bounds, in its owner's window coordinates, and shows it.
        void PlacePopup(const Rect& Bounds);
        /// @brief True while one of this window's events is being handled, including any modal loop it started.
        [[nodiscard]] bool IsDispatching() const { return _DispatchDepth > 0; }

        /// @brief Handles one event addressed to this window.
        void HandleEvent(const SDL_Event& Event);
        /// @brief Destroys popups closed since the last call whose windows aren't handling an event.
        static void DestroyClosedPopups();

        void SetCursor(CursorShape Shape) override;
        void BeginRelativePointer() override;
        void EndRelativePointer() override;

        /// @brief Pixels per window coordinate over pixels per DIP: multiply window coordinates by it for DIPs.
        [[nodiscard]] float GetCoordinateScale() const;

        [[nodiscard]] bool IsOpen() const override { return _Window != nullptr; }
        [[nodiscard]] Size GetClientSize() const override;
        [[nodiscard]] float GetDpiScale() const override;

        void ReleaseSwapChain() override;
        void RecreateSwapChain() override;

    protected:
        [[nodiscard]] bool HasRenderTarget() const override { return _SwapChain != nullptr && _Visible; }
        void RenderComposition() override;
        void ConfigureContent(WidgetHost& Content) override;
        void OnContentUpdated() override;

    private:
        friend class PopupManager;

        void CreateNativeWindow(const char* Title, SDL_WindowFlags Flags, SDL_Window* Owner, const Size& ClientSize);
        void RequestClose();
        void DestroyNative();
        void OnResize();
        void UpdatePopupSize();

        bool HandleWindowEvent(const SDL_Event& Event);
        void HandleMouseMotion(const SDL_MouseMotionEvent& Event);
        void HandleMouseButton(const SDL_MouseButtonEvent& Event);
        void HandleWheel(const SDL_MouseWheelEvent& Event);
        void HandleKey(const SDL_KeyboardEvent& Event);
        void HandleText(const SDL_TextInputEvent& Event);
        void HandleComposition(const SDL_TextEditingEvent& Event);
        void HandleDrop(const SDL_Event& Event);
        /// @brief Drives the caption buttons of a custom title bar. Returns true when it consumed the event.
        bool HandleCaptionButton(const Vec2& Position, bool Pressed, bool Released);
        /// @brief The compositor took the pointer (and often keyboard focus) to move the window: SDL handed it the
        /// press on the caption without telling us. Pairs of these make a double-click.
        void BeginCaptionGrab();
        /// @brief The pointer is back after a caption grab. Applies a double-click's maximize toggle. CheckFocus
        /// dims the title bar if focus never came back, for a focus loss that wasn't a grab after all.
        void EndCaptionGrab(bool CheckFocus);
        [[nodiscard]] bool IsCaptionGrabLikely() const;
        void SetCaptionHover(WindowRegion Region);
        static SDL_HitTestResult HitTest(SDL_Window* Window, const SDL_Point* Point, void* Data);

        void DispatchPointer(PointerPhase Phase, MouseButton Button, const Vec2& Position);
        [[nodiscard]] TextInputClient* GetTextInput() const;
        void PlaceInputMethod();
        void CancelPointers();
        void ApplyCursor() const;
        [[nodiscard]] Vec2 ToDips(float X, float Y) const;

        SDL_Window* _Window = nullptr;
        SDL_WindowID _Id    = 0;
        std::unique_ptr<SwapChain> _SwapChain;
        std::unique_ptr<PopupManager> _Popups;
        std::unique_ptr<WindowChrome> _Chrome;
        bool _Resizable       = true;
        bool _Visible         = false;
        bool _InContentUpdate = false;
        uint32_t _DispatchDepth = 0;

        CursorShape _Cursor      = CursorShape::Arrow;
        MouseButton _HeldButtons = MouseButton::None;
        bool _MouseInside        = false;
        bool _RelativeMode       = false;
        Vec2 _RelativeAnchor;

        /// @brief Data of an external drag over the window, collected as SDL reports it.
        struct ExternalDrop {
            bool Active = false;
            DragData Data;
            Vec2 Position;
        };
        ExternalDrop _Drop;

        /// @brief Where the pointer last was, and whether that was the draggable caption.
        Vec2 _LastPointer;
        bool _PointerOverCaption = false;
        /// @brief A caption press the compositor took, inferred from the events that follow it.
        bool _InCaptionGrab        = false;
        double _CaptionGrabStarted = -1.0;
        /// @brief The first press of a possible double-click on the caption.
        double _CaptionClickTime = -1.0;
        Vec2 _CaptionClickPosition;
        bool _ToggleAfterGrab = false;
        std::optional<Rect> _InputMethodArea;
    };
}  // namespace XenUI
