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

#include "XenUI/Platform/Linux/WindowImpl.hpp"
#include "XenUI/AppContext.hpp"
#include "XenUI/Core/Error.hpp"
#include "XenUI/Graphics/DeviceLost.hpp"
#include "XenUI/Graphics/GL/SwapChain.hpp"
#include "XenUI/Platform/Linux/IconFile.hpp"
#include "XenUI/Platform/Linux/PopupManager.hpp"
#include "XenUI/Platform/Linux/SdlPlatform.hpp"
#include "XenUI/Widgets/WidgetHost.hpp"
#include "XenUI/Widgets/WindowChrome.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <string>
#include <utility>
#include <vector>

namespace XenUI {
    namespace {
        /// @brief Every live window, so the run loop can destroy closed popups once their events are handled.
        std::vector<WindowImpl*>& GetLiveWindows() {
            static std::vector<WindowImpl*> Windows;
            return Windows;
        }

        /// @brief Presents a window's GL drawable with the shared context.
        class SdlSurface final : public GLSurface {
        public:
            SdlSurface(SDL_Window* Window, SDL_GLContext Context) : _Window(Window), _Context(Context) {}

            void MakeCurrent() override { SDL_GL_MakeCurrent(_Window, _Context); }

            bool SwapBuffers() override { return SDL_GL_SwapWindow(_Window); }

        private:
            SDL_Window* _Window;
            SDL_GLContext _Context;
        };
    }  // namespace

    WindowImpl::WindowImpl(const WindowDesc& Desc)
        : WindowCore(Desc.Background, false), _Popups(std::make_unique<PopupManager>(*this)),
          _Resizable(Desc.Resizable) {
        SDL_WindowFlags Flags = SDL_WINDOW_HIDDEN;
        if (Desc.Resizable) { Flags |= SDL_WINDOW_RESIZABLE; }
        if (Desc.CustomTitleBar) {
            Flags |= SDL_WINDOW_BORDERLESS;
            _Chrome                 = std::make_unique<WindowChrome>();
            _Chrome->Minimize       = [this] { SDL_MinimizeWindow(_Window); };
            _Chrome->ToggleMaximize = [this] {
                if ((SDL_GetWindowFlags(_Window) & SDL_WINDOW_MAXIMIZED) != 0) {
                    SDL_RestoreWindow(_Window);
                } else {
                    SDL_MaximizeWindow(_Window);
                }
            };
            _Chrome->Close = [this] { Close(); };
        }
        const std::string Title(Desc.Title);
        CreateNativeWindow(Title.c_str(), Flags, nullptr, Desc.ClientSize);
        if (_Chrome) { SDL_SetWindowHitTest(_Window, &WindowImpl::HitTest, this); }
        // Typing reaches widgets as text events, as WM_CHAR does on Windows; the input method is told where the
        // caret is whenever a text field has focus.
        SDL_StartTextInput(_Window);
    }

    WindowImpl::WindowImpl(const PopupWindowOptions& Options) : WindowCore(Options.Background, true) {
        const SDL_WindowFlags Flags = SDL_WINDOW_HIDDEN | SDL_WINDOW_NOT_FOCUSABLE |
                                      (Options.Tooltip ? SDL_WINDOW_TOOLTIP : SDL_WINDOW_POPUP_MENU);
        CreateNativeWindow("", Flags, Options.Owner, {1.0f, 1.0f});
    }

    void WindowImpl::CreateNativeWindow(const char* Title,
                                        SDL_WindowFlags Flags,
                                        SDL_Window* Owner,
                                        const Size& ClientSize) {
        Sdl::EnsureVideo();
        Flags |= SDL_WINDOW_OPENGL | SDL_WINDOW_HIGH_PIXEL_DENSITY;
        const int Width  = std::max(1, static_cast<int>(std::lround(ClientSize.Width)));
        const int Height = std::max(1, static_cast<int>(std::lround(ClientSize.Height)));
        _Window          = Owner != nullptr ? SDL_CreatePopupWindow(Owner, 0, 0, Width, Height, Flags)
                                            : SDL_CreateWindow(Title, Width, Height, Flags);
        if (_Window == nullptr) { throw Error(std::format("Window: SDL couldn't create a window ({})", SDL_GetError())); }
        _Id = SDL_GetWindowID(_Window);
        Sdl::Attach(_Window, this);
        GetLiveWindows().push_back(this);
        AppContext::Get().OnWindowOpened(*this);

        try {
            // The window was made in window coordinates, which equal DIPs only when the display's scale is all
            // pixel density (Wayland); elsewhere (X11) scale it now that the window knows its display.
            if (Owner == nullptr) {
                const float Ratio = 1.0f / GetCoordinateScale();
                if (std::abs(Ratio - 1.0f) > 0.01f) {
                    SDL_SetWindowSize(_Window,
                                      static_cast<int>(std::lround(ClientSize.Width * Ratio)),
                                      static_cast<int>(std::lround(ClientSize.Height * Ratio)));
                }
                SDL_SetWindowPosition(_Window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
            }
            RecreateSwapChain();
        } catch (...) {
            DestroyNative();
            throw;
        }
    }

    WindowImpl::~WindowImpl() {
        DestroyNative();
        std::erase(GetLiveWindows(), this);

        // The content refers to the popup presenter and the chrome, which base members would otherwise outlive.
        _Chrome.reset();
        _Content.reset();
        _Popups.reset();
    }

    void WindowImpl::Show() {
        if (_Window == nullptr) { return; }
        SDL_ShowWindow(_Window);
        _Visible = true;
        _Compositor.AddFullDamage();
    }

    void WindowImpl::Hide() {
        if (_Window == nullptr) { return; }
        SDL_HideWindow(_Window);
        _Visible = false;
    }

    void WindowImpl::Maximize() {
        if (_Window != nullptr) { SDL_MaximizeWindow(_Window); }
    }

    void WindowImpl::Close() {
        // Like WM_CLOSE posted on Windows: ask once the current event has been handled.
        const SDL_WindowID Id = _Id;
        AppContext::Post([Id] {
            if (WindowImpl* Target = Sdl::FindWindow(SDL_GetWindowFromID(Id))) { Target->RequestClose(); }
        });
    }

    void WindowImpl::Destroy() {
        const SDL_WindowID Id = _Id;
        AppContext::Post([Id] {
            if (WindowImpl* Target = Sdl::FindWindow(SDL_GetWindowFromID(Id))) { Target->DestroyNative(); }
        });
    }

    void WindowImpl::RequestClose() {
        if (const std::function<bool()> Requested = _OnCloseRequested; Requested && !Requested()) { return; }
        DestroyNative();
    }

    void WindowImpl::DestroyNative() {
        if (_Window == nullptr) { return; }
        if (_Popups) {
            _Popups->DismissAll();
            _Popups->DestroyClosed(true);
        }
        CancelPointers();
        _SwapChain.reset();

        SDL_Window* Native = std::exchange(_Window, nullptr);
        Sdl::Attach(Native, nullptr);
        if (SDL_GL_GetCurrentWindow() == Native) { SDL_GL_MakeCurrent(nullptr, nullptr); }
        SDL_DestroyWindow(Native);
        _Visible = false;

        if (auto* Context = AppContext::TryGet()) { Context->OnWindowClosed(*this); }
        if (const std::function<void()> Closed = _OnClosed) { Closed(); }
    }

    void WindowImpl::DestroyClosedPopups() {
        for (WindowImpl* Window : std::vector<WindowImpl*>(GetLiveWindows())) {
            if (Window->_Popups) { Window->_Popups->DestroyClosed(false); }
        }
    }

    void WindowImpl::SetTitle(std::string_view Title) {
        if (_Window != nullptr) { SDL_SetWindowTitle(_Window, std::string(Title).c_str()); }
    }

    void WindowImpl::SetClientSize(const Size& ClientSize) {
        if (_Window == nullptr) { return; }
        const float Ratio = 1.0f / GetCoordinateScale();
        SDL_SetWindowSize(_Window,
                          std::max(1, static_cast<int>(std::lround(ClientSize.Width * Ratio))),
                          std::max(1, static_cast<int>(std::lround(ClientSize.Height * Ratio))));
    }

    void WindowImpl::SetDarkTitleBar(bool Dark) {
        (void)Dark;
    }

    void WindowImpl::SetIcon(int ResourceId) {
        (void)ResourceId;
    }

    void WindowImpl::SetIconFile(const std::filesystem::path& IconFile) {
        std::optional<IconPixels> Icon = LoadIconFile(IconFile);
        if (!Icon) { throw Error("Couldn't load the icon " + IconFile.string()); }
        if (_Window == nullptr) { return; }
        SDL_Surface* Surface = SDL_CreateSurfaceFrom(static_cast<int>(Icon->Width),
                                                     static_cast<int>(Icon->Height),
                                                     SDL_PIXELFORMAT_BGRA32,
                                                     Icon->Pixels.data(),
                                                     static_cast<int>(Icon->Width * 4));
        if (Surface == nullptr) { throw Error("Couldn't load the icon " + IconFile.string()); }
        SDL_SetWindowIcon(_Window, Surface);
        SDL_DestroySurface(Surface);
    }

    void WindowImpl::Activate() {
        if (_Window == nullptr) { return; }
        if ((SDL_GetWindowFlags(_Window) & SDL_WINDOW_MINIMIZED) != 0) { SDL_RestoreWindow(_Window); }
        SDL_RaiseWindow(_Window);
    }

    void WindowImpl::ConfigureContent(WidgetHost& Content) {
        Content.SetPopupPresenter(_Popups.get());
        Content.SetNativeParent(_Window);
        Content.SetWindowChrome(_Chrome.get());
    }

    void WindowImpl::PlacePopup(const Rect& Bounds) {
        if (_Window == nullptr) { return; }
        SDL_SetWindowPosition(_Window, static_cast<int>(std::lround(Bounds.X)), static_cast<int>(std::lround(Bounds.Y)));
        SDL_SetWindowSize(_Window,
                          std::max(1, static_cast<int>(std::lround(Bounds.Width))),
                          std::max(1, static_cast<int>(std::lround(Bounds.Height))));
        Show();
    }

    void WindowImpl::UpdatePopupSize() {
        const float Ratio  = 1.0f / GetCoordinateScale();
        const Size Content = _Content->GetLaidOutSize();
        const int Width    = std::max(1, static_cast<int>(std::ceil(Content.Width * Ratio)));
        const int Height   = std::max(1, static_cast<int>(std::ceil(Content.Height * Ratio)));
        int CurrentWidth   = 0;
        int CurrentHeight  = 0;
        SDL_GetWindowSize(_Window, &CurrentWidth, &CurrentHeight);
        if (Width == CurrentWidth && Height == CurrentHeight) { return; }

        _InContentUpdate = true;
        SDL_SetWindowSize(_Window, Width, Height);
        _InContentUpdate = false;
    }

    float WindowImpl::GetCoordinateScale() const {
        if (_Window == nullptr) { return 1.0f; }
        const float Density = SDL_GetWindowPixelDensity(_Window);
        const float Scale   = GetDpiScale();
        return Density > 0.0f && Scale > 0.0f ? Density / Scale : 1.0f;
    }

    Size WindowImpl::GetClientSize() const {
        if (_Window == nullptr) { return {}; }
        int Width  = 0;
        int Height = 0;
        SDL_GetWindowSizeInPixels(_Window, &Width, &Height);
        return Size {static_cast<float>(Width), static_cast<float>(Height)} / GetDpiScale();
    }

    float WindowImpl::GetDpiScale() const {
        if (_Window == nullptr) { return 1.0f; }
        const float Scale = SDL_GetWindowDisplayScale(_Window);
        return Scale > 0.0f ? Scale : 1.0f;
    }

    Vec2 WindowImpl::ToDips(float X, float Y) const {
        const float Scale = GetCoordinateScale();
        return {X * Scale, Y * Scale};
    }

    void WindowImpl::RenderComposition() {
        // Windows frames every window with a thin line (DWM draws it even without a system title bar). Linux
        // desktops don't frame borderless windows, so they vanish against similar content; draw the line ourselves
        // for custom title bars and popups, except when maximized, as Windows does.
        std::optional<Color> Border;
        const bool Maximized = _Window != nullptr && (SDL_GetWindowFlags(_Window) & SDL_WINDOW_MAXIMIZED) != 0;
        if ((_Chrome || _IsPopup) && !Maximized) {
            const float Luminance = 0.2126f * _Background.R + 0.7152f * _Background.G + 0.0722f * _Background.B;
            Border = Luminance < 0.5f ? Color {1.0f, 1.0f, 1.0f, 0.14f} : Color {0.0f, 0.0f, 0.0f, 0.22f};
        }
        _Compositor.SetFrameBorder(Border);
        _Compositor.Render(*_SwapChain, AppContext::Get().GetGraphicsDevice(), _Background);
    }

    void WindowImpl::ReleaseSwapChain() {
        _SwapChain.reset();
    }

    void WindowImpl::RecreateSwapChain() {
        _SwapChain.reset();
        if (_Window == nullptr) { return; }
        SDL_GLContext Context = Sdl::GetGLContext(_Window);
        SDL_GL_MakeCurrent(_Window, Context);
        // Top-level windows wait for vertical blank; popups shouldn't hold up their owner's frames.
        SDL_GL_SetSwapInterval(_IsPopup ? 0 : 1);

        int Width  = 0;
        int Height = 0;
        SDL_GetWindowSizeInPixels(_Window, &Width, &Height);
        _SwapChain = std::make_unique<SwapChain>(static_cast<uint32_t>(std::max(Width, 1)),
                                                 static_cast<uint32_t>(std::max(Height, 1)),
                                                 GetDpiScale(),
                                                 std::make_unique<SdlSurface>(_Window, Context));
        _Compositor.ResetBufferHistory();
    }

    void WindowImpl::OnResize() {
        if (!_SwapChain || _Window == nullptr) { return; }
        int Width  = 0;
        int Height = 0;
        SDL_GetWindowSizeInPixels(_Window, &Width, &Height);
        const auto PixelWidth  = static_cast<uint32_t>(std::max(Width, 1));
        const auto PixelHeight = static_cast<uint32_t>(std::max(Height, 1));
        if (PixelWidth == _SwapChain->GetWidth() && PixelHeight == _SwapChain->GetHeight()) { return; }

        _SwapChain->Resize(PixelWidth, PixelHeight);
        _Compositor.ResetBufferHistory();
        if (!_InContentUpdate) { RenderFrame(); }
    }

    void WindowImpl::HandleEvent(const SDL_Event& Event) {
        if (_Window == nullptr) { return; }
        ++_DispatchDepth;
        struct DepthGuard {
            uint32_t& Depth;
            ~DepthGuard() { --Depth; }
        } Guard {_DispatchDepth};

        if (HandleWindowEvent(Event)) { return; }
        switch (Event.type) {
            case SDL_EVENT_MOUSE_MOTION:
                HandleMouseMotion(Event.motion);
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                HandleMouseButton(Event.button);
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                HandleWheel(Event.wheel);
                break;
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
                HandleKey(Event.key);
                break;
            case SDL_EVENT_TEXT_INPUT:
                HandleText(Event.text);
                break;
            case SDL_EVENT_TEXT_EDITING:
                HandleComposition(Event.edit);
                break;
            case SDL_EVENT_DROP_BEGIN:
            case SDL_EVENT_DROP_FILE:
            case SDL_EVENT_DROP_TEXT:
            case SDL_EVENT_DROP_POSITION:
            case SDL_EVENT_DROP_COMPLETE:
                HandleDrop(Event);
                break;
            default:
                break;
        }
    }

    bool WindowImpl::HandleWindowEvent(const SDL_Event& Event) {
        switch (Event.type) {
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                RequestClose();
                return true;
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                if (_Popups) { _Popups->DismissAll(); }
                OnResize();
                return true;
            case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
                if (_SwapChain) { _SwapChain->SetDpiScale(GetDpiScale()); }
                _Compositor.AddFullDamage();
                OnResize();
                return true;
            case SDL_EVENT_WINDOW_MOVED:
                if (_Popups) { _Popups->DismissAll(); }
                return true;
            case SDL_EVENT_WINDOW_EXPOSED:
                _Compositor.AddFullDamage();
                RenderFrame();
                return true;
            case SDL_EVENT_WINDOW_SHOWN:
                _Visible = true;
                _Compositor.AddFullDamage();
                return true;
            case SDL_EVENT_WINDOW_HIDDEN:
                _Visible = false;
                return true;
            case SDL_EVENT_WINDOW_MINIMIZED:
                _Minimized = true;
                if (_Popups) { _Popups->DismissAll(); }
                return true;
            case SDL_EVENT_WINDOW_MAXIMIZED:
                _Minimized = false;
                if (_Chrome) { _Chrome->Maximized = true; }
                return true;
            case SDL_EVENT_WINDOW_RESTORED:
                _Minimized = false;
                if (_Chrome) { _Chrome->Maximized = false; }
                _Compositor.AddFullDamage();
                return true;
            case SDL_EVENT_WINDOW_FOCUS_GAINED:
                if (_Chrome) { _Chrome->Active = true; }
                EndCaptionGrab(false);
                return true;
            case SDL_EVENT_WINDOW_FOCUS_LOST:
                if (_Popups) { _Popups->DismissAll(); }
                // Pressing the caption hands the pointer to the compositor to move the window, and GNOME takes
                // keyboard focus for the move too. That isn't the window being deactivated, so don't dim the title
                // bar for it.
                if (IsCaptionGrabLikely()) {
                    BeginCaptionGrab();
                    return true;
                }
                if (_Chrome) { _Chrome->Active = false; }
                CancelPointers();
                return true;
            case SDL_EVENT_WINDOW_MOUSE_ENTER:
                _MouseInside = true;
                ApplyCursor();
                EndCaptionGrab(false);
                return true;
            case SDL_EVENT_WINDOW_MOUSE_LEAVE:
                if (IsCaptionGrabLikely()) { BeginCaptionGrab(); }
                _MouseInside = false;
                SetCaptionHover(WindowRegion::Client);
                if (_Chrome) { _Chrome->Pressed = WindowRegion::Client; }
                DispatchPointer(PointerPhase::Leave, MouseButton::None, {});
                return true;
            default:
                return false;
        }
    }

    void WindowImpl::OnContentUpdated() {
        if (_IsPopup && _Visible) { UpdatePopupSize(); }
        if (!_IsPopup) { PlaceInputMethod(); }
    }
}  // namespace XenUI
