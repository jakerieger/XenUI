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

#include "XenUI/Platform/WindowImpl.hpp"
#include "XenUI/Platform/Accessibility.hpp"
#include "XenUI/Platform/SystemDragDrop.hpp"
#include "XenUI/Widgets/WindowChrome.hpp"
#include "XenUI/Widgets/Theme.hpp"
#include "XenUI/Core/Animation.hpp"
#include "XenUI/Core/Timer.hpp"

#include <windowsx.h>

#include "XenUI/AppContext.hpp"
#include "XenUI/Graphics/DeviceLost.hpp"
#include "XenUI/Graphics/SwapChain.hpp"
#include "XenUI/Platform/HResult.hpp"
#include "XenUI/Platform/PopupManager.hpp"
#include "XenUI/Platform/Utf8.hpp"
#include "XenUI/Widgets/WidgetHost.hpp"

#include <dwmapi.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <mutex>
#include <utility>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace XenUI {
    namespace {
        constexpr wchar_t kWindowClassName[] = L"XenUI.Window";
        constexpr wchar_t kPopupClassName[]  = L"XenUI.Popup";

        HINSTANCE GetModuleInstance() {
            return reinterpret_cast<HINSTANCE>(&__ImageBase);
        }

        float DpiToScale(UINT Dpi) {
            return static_cast<float>(Dpi) / static_cast<float>(USER_DEFAULT_SCREEN_DPI);
        }

        void
        RegisterWindowClass(std::once_flag& Registered, const wchar_t* ClassName, UINT ClassStyle, WNDPROC WindowProc) {
            std::call_once(Registered, [ClassName, ClassStyle, WindowProc] {
                const WNDCLASSEXW WindowClass {
                  .cbSize        = sizeof(WNDCLASSEXW),
                  .style         = ClassStyle,
                  .lpfnWndProc   = WindowProc,
                  .hInstance     = GetModuleInstance(),
                  .hCursor       = LoadCursorW(nullptr, IDC_ARROW),
                  .lpszClassName = ClassName,
                };
                if (RegisterClassExW(&WindowClass) == 0) { ThrowLastError("RegisterClassExW"); }
            });
        }
    }  // namespace

    WindowImpl::WindowImpl(const WindowDesc& Desc)
        : _Background(Desc.Background), _Compositor(*this), _Popups(std::make_unique<PopupManager>(*this)) {
        static std::once_flag Registered;
        RegisterWindowClass(Registered, kWindowClassName, 0, &WindowImpl::WindowProc);

        _Style = (Desc.Resizable ? WS_OVERLAPPEDWINDOW : (WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX))) |
                 WS_CLIPCHILDREN;
        if (Desc.CustomTitleBar) {
            _Chrome                 = std::make_unique<WindowChrome>();
            _Chrome->Minimize       = [this] { ShowWindow(_Hwnd, SW_MINIMIZE); };
            _Chrome->ToggleMaximize = [this] { ShowWindow(_Hwnd, IsZoomed(_Hwnd) ? SW_RESTORE : SW_MAXIMIZE); };
            _Chrome->Close          = [this] { Close(); };
        }
        CreateNativeWindow(kWindowClassName, Widen(Desc.Title), 0, nullptr, {CW_USEDEFAULT, CW_USEDEFAULT}, [&] {
            if (Desc.DarkTitleBar) {
                const BOOL Enabled = TRUE;
                DwmSetWindowAttribute(_Hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &Enabled, sizeof(Enabled));
            }
            CenterOnMonitor(Desc.ClientSize);
        });
        _DropRegistration = std::make_unique<SystemDropRegistration>(*this);
        if (_Chrome) {
            SetWindowPos(_Hwnd,
                         nullptr,
                         0,
                         0,
                         0,
                         0,
                         SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }

    WindowImpl::WindowImpl(const PopupWindowOptions& Options)
        : _Background(Options.Background), _Compositor(*this), _IsPopup(true), _ClickThrough(Options.ClickThrough) {
        static std::once_flag Registered;
        RegisterWindowClass(Registered, kPopupClassName, CS_DROPSHADOW, &WindowImpl::WindowProc);

        _Style = WS_POPUP;
        CreateNativeWindow(kPopupClassName,
                           L"",
                           WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                           Options.Owner,
                           Options.Position,
                           [this] {
                               const DWM_WINDOW_CORNER_PREFERENCE Corners = DWMWCP_ROUNDSMALL;
                               DwmSetWindowAttribute(_Hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &Corners, sizeof(Corners));
                           });
    }

    void WindowImpl::CreateNativeWindow(const wchar_t* ClassName,
                                        const std::wstring& Title,
                                        DWORD ExStyle,
                                        HWND Owner,
                                        POINT Position,
                                        const std::function<void()>& Configure) {
        const bool DefaultPlacement = Position.x == CW_USEDEFAULT;
        CreateWindowExW(ExStyle,
                        ClassName,
                        Title.c_str(),
                        _Style,
                        Position.x,
                        Position.y,
                        DefaultPlacement ? CW_USEDEFAULT : 1,
                        DefaultPlacement ? CW_USEDEFAULT : 1,
                        Owner,
                        nullptr,
                        GetModuleInstance(),
                        this);
        if (_Hwnd == nullptr) { ThrowLastError("CreateWindowExW"); }

        AppContext::Get().OnWindowOpened(*this);

        try {
            _Dpi = GetDpiForWindow(_Hwnd);
            Configure();
            RecreateSwapChain();
        } catch (...) {
            DestroyWindow(_Hwnd);
            throw;
        }
    }

    WindowImpl::~WindowImpl() {
        if (_Hwnd != nullptr) { DestroyWindow(_Hwnd); }
    }

    void WindowImpl::Show() {
        ShowWindow(_Hwnd, SW_SHOW);
    }

    void WindowImpl::Maximize() {
        ShowWindow(_Hwnd, SW_MAXIMIZE);
    }

    void WindowImpl::Close() {
        if (_Hwnd != nullptr) { PostMessageW(_Hwnd, WM_CLOSE, 0, 0); }
    }

    void WindowImpl::SetTitle(std::string_view Title) {
        SetWindowTextW(_Hwnd, Widen(Title).c_str());
    }

    void WindowImpl::SetBackground(const Color& NewBackground) {
        _Background = NewBackground;
        _Compositor.AddFullDamage();
    }

    void WindowImpl::SetContent(const Node& Root) {
        if (!_Content) {
            _Content = std::make_unique<WidgetHost>();
            _Content->SetRecordsFrames(false);
            _Content->SetOnUpdateRequested([this] { RequestFrame(); });
            _Content->GetInput().SetHost(this);
            if (_IsPopup) {
                _Content->SetSizeToContent(_PopupMaximum);
            } else {
                _Content->SetPopupPresenter(_Popups.get());
                _Content->SetNativeParent(_Hwnd);
                _Content->SetWindowChrome(_Chrome.get());
            }
        }
        _Compositor.SetRoot(_Content->GetRootLayer());
        _Content->SetRoot(Root);
    }

    void WindowImpl::SetRootLayer(std::shared_ptr<Layer> Root) {
        _Content.reset();
        _Compositor.SetRoot(std::move(Root));
    }

    void WindowImpl::Hide() {
        if (_Hwnd != nullptr) { ShowWindow(_Hwnd, SW_HIDE); }
    }

    Size WindowImpl::MeasurePopup(const Size& Maximum) {
        _PopupMaximum = Maximum;
        if (!_Content) { return {}; }
        _Content->SetSizeToContent(Maximum);
        _Content->Update();
        return _Content->GetLaidOutSize();
    }

    void WindowImpl::PlacePopup(const RECT& Bounds) {
        SetWindowPos(_Hwnd,
                     nullptr,
                     Bounds.left,
                     Bounds.top,
                     Bounds.right - Bounds.left,
                     Bounds.bottom - Bounds.top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        ShowWindow(_Hwnd, SW_SHOWNOACTIVATE);
    }

    void WindowImpl::UpdatePopupSize() {
        const float Scale  = GetDpiScale();
        const Size Content = _Content->GetLaidOutSize();
        const LONG Width   = std::lround(std::ceil(Content.Width * Scale));
        const LONG Height  = std::lround(std::ceil(Content.Height * Scale));
        RECT Client {};
        GetClientRect(_Hwnd, &Client);
        if (Width == Client.right && Height == Client.bottom) { return; }

        _InContentUpdate = true;
        SetWindowPos(_Hwnd, nullptr, 0, 0, Width, Height, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        _InContentUpdate = false;
    }

    void WindowImpl::SetFrameCallback(FrameCallback Callback) {
        _FrameCallback = std::move(Callback);
        RequestFrame();
    }

    void WindowImpl::SetShowDamageRegions(bool Enabled) {
        _Compositor.SetShowDamage(Enabled);
    }

    Size WindowImpl::GetClientSize() const {
        RECT Client {};
        GetClientRect(_Hwnd, &Client);
        const Size Physical {static_cast<float>(Client.right - Client.left),
                             static_cast<float>(Client.bottom - Client.top)};
        return Physical / GetDpiScale();
    }

    float WindowImpl::GetDpiScale() const {
        return DpiToScale(_Dpi);
    }

    bool WindowImpl::HasPendingFrame() const {
        return _SwapChain && !_Minimized && (_FrameRequested || _Compositor.HasPendingDamage());
    }

    HANDLE WindowImpl::GetFrameWaitable() const {
        return _SwapChain ? _SwapChain->GetFrameWaitable() : nullptr;
    }

    void WindowImpl::MarkFrameReady() {
        if (_SwapChain) { _SwapChain->MarkFrameReady(); }
    }

    void WindowImpl::RenderFrame() {
        if (!_SwapChain) { return; }

        const double Started  = TimerQueue::Get().Now();
        const auto CallbackAt = std::chrono::steady_clock::now();
        RunFrameCallback();
        const float CallbackMs =
          std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - CallbackAt).count();
        UpdateContent();
        try {
            _Compositor.Render(*_SwapChain, AppContext::Get().GetGraphicsDevice(), _Background);
        } catch (const DeviceLostError&) {
            AppContext::Get().HandleDeviceLost();
            return;
        }

        if (_Content) {
            const UpdateTimings& Update        = _Content->GetLastUpdate();
            const Compositor::Timings& Drawing = _Compositor.GetLastTimings();
            _Content->GetFrameStatistics().Record({
              .Time       = Started,
              .CallbackMs = CallbackMs,
              .BuildMs    = Update.BuildMs,
              .LayoutMs   = Update.LayoutMs,
              .PaintMs    = Update.PaintMs,
              .DrawMs     = Drawing.DrawMs,
              .PresentMs  = Drawing.PresentMs,
              .WaitMs     = Drawing.WaitMs,
              .Rebuilt    = Update.Rebuilt,
              .LaidOut    = Update.LaidOut,
              .Repainted  = Update.Repainted,
              .Damage     = Drawing.Damage,
            });
        }
    }

    void WindowImpl::ReleaseSwapChain() {
        _SwapChain.reset();
    }

    void WindowImpl::RecreateSwapChain() {
        _SwapChain.reset();
        _SwapChain =
          std::make_unique<SwapChain>(AppContext::Get().GetGraphicsDevice(), _Hwnd, static_cast<float>(_Dpi));
        _Compositor.ResetBufferHistory();
    }

    LRESULT CALLBACK WindowImpl::WindowProc(HWND Hwnd, UINT Message, WPARAM WParam, LPARAM LParam) {
        if (Message == WM_NCCREATE) {
            const auto* CreateStruct = reinterpret_cast<const CREATESTRUCTW*>(LParam);
            auto* Creator            = static_cast<WindowImpl*>(CreateStruct->lpCreateParams);
            Creator->_Hwnd           = Hwnd;
            SetWindowLongPtrW(Hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(Creator));
        }

        auto* Self = reinterpret_cast<WindowImpl*>(GetWindowLongPtrW(Hwnd, GWLP_USERDATA));
        if (Self == nullptr) { return DefWindowProcW(Hwnd, Message, WParam, LParam); }

        try {
            return Self->HandleMessage(Message, WParam, LParam);
        } catch (...) {
            if (auto* Context = AppContext::TryGet()) { Context->ReportException(std::current_exception()); }
            return DefWindowProcW(Hwnd, Message, WParam, LParam);
        }
    }

    LRESULT WindowImpl::HandleMessage(UINT Message, WPARAM WParam, LPARAM LParam) {
        if (const std::optional<LRESULT> Result = HandleFrameMessage(Message, WParam, LParam)) { return *Result; }
        if (const std::optional<LRESULT> Result = HandleInputMessage(Message, WParam, LParam)) { return *Result; }

        switch (Message) {
            case WM_SIZE:
                _Minimized = WParam == SIZE_MINIMIZED;
                if (_Chrome) { _Chrome->Maximized = WParam == SIZE_MAXIMIZED; }
                if (_Popups) { _Popups->DismissAll(); }
                if (!_Minimized) { OnResize(LOWORD(LParam), HIWORD(LParam)); }
                return 0;
            case WM_MOVE:
                if (_Popups) { _Popups->DismissAll(); }
                return 0;
            case WM_ACTIVATE:
                if (LOWORD(WParam) == WA_INACTIVE && _Popups) { _Popups->DismissAll(); }
                if (_Chrome) { _Chrome->Active = LOWORD(WParam) != WA_INACTIVE; }
                return DefWindowProcW(_Hwnd, Message, WParam, LParam);
            case WM_ACTIVATEAPP:
                if (WParam == FALSE && _Popups) { _Popups->DismissAll(); }
                return 0;
            case WM_MOUSEACTIVATE:
                return _IsPopup ? MA_NOACTIVATE : DefWindowProcW(_Hwnd, Message, WParam, LParam);
            case WM_NCHITTEST:
                return _ClickThrough ? HTTRANSPARENT : DefWindowProcW(_Hwnd, Message, WParam, LParam);
            case kDestroyPopupsMessage:
                if (_Popups) { _Popups->DestroyClosed(); }
                return 0;
            case WM_PAINT:
                _Compositor.AddFullDamage();
                RenderFrame();
                ValidateRect(_Hwnd, nullptr);
                return 0;
            case WM_ERASEBKGND:
                return 1;
            case WM_SETTINGCHANGE:
                if (LParam != 0 &&
                    std::wstring_view(reinterpret_cast<const wchar_t*>(LParam)) == L"ImmersiveColorSet") {
                    RefreshSystemAppearance();
                }
                if (WParam == SPI_SETCLIENTAREAANIMATION) { AnimationTicker::RefreshSystemSetting(); }
                return DefWindowProcW(_Hwnd, Message, WParam, LParam);
            case WM_GETOBJECT: {
                if (!_Accessibility) { _Accessibility = std::make_unique<AccessibilityBridge>(*this); }
                if (const std::optional<LRESULT> Result = _Accessibility->HandleGetObject(WParam, LParam)) {
                    return *Result;
                }
                return DefWindowProcW(_Hwnd, Message, WParam, LParam);
            }
            case WM_DPICHANGED:
                OnDpiChanged(HIWORD(WParam), *reinterpret_cast<const RECT*>(LParam));
                return 0;
            case WM_DESTROY:
                OnDestroy();
                return 0;
            case WM_NCDESTROY: {
                const HWND Hwnd = std::exchange(_Hwnd, nullptr);
                SetWindowLongPtrW(Hwnd, GWLP_USERDATA, 0);
                return DefWindowProcW(Hwnd, Message, WParam, LParam);
            }
            default:
                return DefWindowProcW(_Hwnd, Message, WParam, LParam);
        }
    }

    int WindowImpl::GetFrameThickness() const {
        return GetSystemMetricsForDpi(SM_CXSIZEFRAME, _Dpi) + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, _Dpi);
    }

    RECT WindowImpl::GetFrameRect(const Size& ClientSize) const {
        const Size Physical = ClientSize * GetDpiScale();
        RECT Frame {0, 0, std::lround(Physical.Width), std::lround(Physical.Height)};
        if (_Chrome) {
            const int Thickness = (_Style & WS_THICKFRAME) != 0 ? GetFrameThickness() : 0;
            Frame.left -= Thickness;
            Frame.right += Thickness;
            Frame.bottom += Thickness;
            return Frame;
        }
        AdjustWindowRectExForDpi(&Frame, _Style, FALSE, 0, _Dpi);
        return Frame;
    }

    void WindowImpl::SetCaptionHover(WindowRegion Region) {
        if (_Chrome && _Chrome->Hovered.Peek() != Region) { _Chrome->Hovered = Region; }
    }

    std::optional<LRESULT> WindowImpl::HandleFrameMessage(UINT Message, WPARAM WParam, LPARAM LParam) {
        if (!_Chrome) { return std::nullopt; }
        const auto ToRegion = [](WPARAM Hit) {
            switch (Hit) {
                case HTMINBUTTON:
                    return WindowRegion::Minimize;
                case HTMAXBUTTON:
                    return WindowRegion::Maximize;
                case HTCLOSE:
                    return WindowRegion::Close;
                default:
                    return WindowRegion::Client;
            }
        };

        switch (Message) {
            case WM_NCCALCSIZE: {
                if (WParam == FALSE) { return std::nullopt; }
                RECT& Client        = reinterpret_cast<NCCALCSIZE_PARAMS*>(LParam)->rgrc[0];
                const int Thickness = (_Style & WS_THICKFRAME) != 0 ? GetFrameThickness() : 0;
                Client.left += Thickness;
                Client.right -= Thickness;
                Client.bottom -= Thickness;
                if (IsZoomed(_Hwnd)) { Client.top += GetFrameThickness(); }
                return 0;
            }
            case WM_NCHITTEST: {
                const LRESULT System = DefWindowProcW(_Hwnd, Message, WParam, LParam);
                if (System != HTCLIENT) { return System; }
                POINT Point {GET_X_LPARAM(LParam), GET_Y_LPARAM(LParam)};
                ScreenToClient(_Hwnd, &Point);
                const int Thickness = GetFrameThickness();
                if (!IsZoomed(_Hwnd) && (_Style & WS_THICKFRAME) != 0 && Point.y < Thickness) {
                    RECT Bounds;
                    GetClientRect(_Hwnd, &Bounds);
                    if (Point.x < Thickness * 2) { return HTTOPLEFT; }
                    if (Point.x >= Bounds.right - Thickness * 2) { return HTTOPRIGHT; }
                    return HTTOP;
                }
                InputDispatcher* Input = GetInput();
                if (Input == nullptr) { return HTCLIENT; }
                const float Scale = GetDpiScale();
                switch (Input->HitTestWindowRegion(
                  {static_cast<float>(Point.x) / Scale, static_cast<float>(Point.y) / Scale})) {
                    case WindowRegion::Caption:
                        return HTCAPTION;
                    case WindowRegion::Minimize:
                        return HTMINBUTTON;
                    case WindowRegion::Maximize:
                        return (_Style & WS_MAXIMIZEBOX) != 0 ? HTMAXBUTTON : HTCLIENT;
                    case WindowRegion::Close:
                        return HTCLOSE;
                    default:
                        return HTCLIENT;
                }
            }
            case WM_NCMOUSEMOVE:
                SetCaptionHover(ToRegion(WParam));
                if (!_TrackingNonClient) {
                    TRACKMOUSEEVENT Tracking {.cbSize    = sizeof(TRACKMOUSEEVENT),
                                              .dwFlags   = TME_LEAVE | TME_NONCLIENT,
                                              .hwndTrack = _Hwnd};
                    TrackMouseEvent(&Tracking);
                    _TrackingNonClient = true;
                }
                return ToRegion(WParam) != WindowRegion::Client ? std::optional<LRESULT>(0) : std::nullopt;
            case WM_NCMOUSELEAVE:
                _TrackingNonClient = false;
                SetCaptionHover(WindowRegion::Client);
                _Chrome->Pressed = WindowRegion::Client;
                return std::nullopt;
            case WM_MOUSEMOVE:
                SetCaptionHover(WindowRegion::Client);
                return std::nullopt;
            case WM_NCLBUTTONDOWN:
            case WM_NCLBUTTONDBLCLK:
                if (ToRegion(WParam) == WindowRegion::Client) { return std::nullopt; }
                if (_Popups) { _Popups->DismissMenus(); }
                _Chrome->Pressed = ToRegion(WParam);
                return 0;
            case WM_NCLBUTTONUP: {
                const WindowRegion Released = ToRegion(WParam);
                if (Released == WindowRegion::Client) { return std::nullopt; }
                const bool Clicked = _Chrome->Pressed.Peek() == Released;
                _Chrome->Pressed   = WindowRegion::Client;
                if (Clicked) {
                    const std::function<void()> Action = Released == WindowRegion::Minimize   ? _Chrome->Minimize
                                                         : Released == WindowRegion::Maximize ? _Chrome->ToggleMaximize
                                                                                              : _Chrome->Close;
                    if (Action) { Action(); }
                }
                return 0;
            }
            default:
                return std::nullopt;
        }
    }

    void WindowImpl::SetClientSize(const Size& ClientSize) {
        if (_Hwnd == nullptr) { return; }
        const RECT Frame = GetFrameRect(ClientSize);
        SetWindowPos(_Hwnd,
                     nullptr,
                     0,
                     0,
                     Frame.right - Frame.left,
                     Frame.bottom - Frame.top,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    void WindowImpl::SetDarkTitleBar(bool Dark) {
        if (_Hwnd == nullptr) { return; }
        const BOOL Enabled = Dark ? TRUE : FALSE;
        DwmSetWindowAttribute(_Hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &Enabled, sizeof(Enabled));
    }

    void WindowImpl::Activate() {
        if (_Hwnd == nullptr) { return; }
        if (IsIconic(_Hwnd)) { ShowWindow(_Hwnd, SW_RESTORE); }
        SetForegroundWindow(_Hwnd);
    }

    void WindowImpl::CenterOnMonitor(const Size& ClientSize) {
        const RECT Frame      = GetFrameRect(ClientSize);
        const int FrameWidth  = Frame.right - Frame.left;
        const int FrameHeight = Frame.bottom - Frame.top;

        MONITORINFO MonitorInfo {.cbSize = sizeof(MONITORINFO)};
        GetMonitorInfoW(MonitorFromWindow(_Hwnd, MONITOR_DEFAULTTONEAREST), &MonitorInfo);
        const RECT& WorkArea = MonitorInfo.rcWork;

        const int X = std::max(WorkArea.left, WorkArea.left + (WorkArea.right - WorkArea.left - FrameWidth) / 2);
        const int Y = std::max(WorkArea.top, WorkArea.top + (WorkArea.bottom - WorkArea.top - FrameHeight) / 2);
        SetWindowPos(_Hwnd, nullptr, X, Y, FrameWidth, FrameHeight, SWP_NOZORDER | SWP_NOACTIVATE);
    }

    void WindowImpl::OnResize(uint32_t Width, uint32_t Height) {
        if (!_SwapChain) { return; }
        if (Width == _SwapChain->GetWidth() && Height == _SwapChain->GetHeight()) { return; }

        try {
            _SwapChain->Resize(Width, Height);
        } catch (const DeviceLostError&) {
            AppContext::Get().HandleDeviceLost();
            return;
        }
        _Compositor.ResetBufferHistory();
        if (!_InContentUpdate) { RenderFrame(); }
    }

    void WindowImpl::OnDpiChanged(UINT Dpi, const RECT& SuggestedRect) {
        _Dpi = Dpi;
        if (_SwapChain) {
            _SwapChain->SetDpi(static_cast<float>(Dpi));
            _Compositor.AddFullDamage();
        }
        SetWindowPos(_Hwnd,
                     nullptr,
                     SuggestedRect.left,
                     SuggestedRect.top,
                     SuggestedRect.right - SuggestedRect.left,
                     SuggestedRect.bottom - SuggestedRect.top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }

    void WindowImpl::OnDestroy() {
        if (_Accessibility) { _Accessibility->Disconnect(); }
        _DropRegistration.reset();
        CancelPointers();
        _SwapChain.reset();
        if (auto* Context = AppContext::TryGet()) { Context->OnWindowClosed(*this); }
        if (const std::function<void()> Closed = _OnClosed) { Closed(); }
    }

    void WindowImpl::UpdateContent() {
        if (!_Content) { return; }
        if (_IsPopup) {
            _Content->SetSizeToContent(_PopupMaximum);
            _Content->Update();
            if (IsWindowVisible(_Hwnd)) { UpdatePopupSize(); }
        } else {
            _Content->SetViewportSize(GetClientSize());
            _Content->Update();
        }
        if (_Accessibility) { _Accessibility->ContentUpdated(); }
    }

    void WindowImpl::RunFrameCallback() {
        _FrameRequested = false;
        if (!_FrameCallback) { return; }

        const double Now = AppContext::Get().GetTime();
        const FrameInfo Info {
          .Time      = Now,
          .DeltaTime = _Animating ? Now - _LastFrameTime : 0.0,
          .Index     = _FrameIndex++,
        };
        _FrameCallback(Info);

        _Animating     = _FrameRequested;
        _LastFrameTime = Now;
    }
}  // namespace XenUI
