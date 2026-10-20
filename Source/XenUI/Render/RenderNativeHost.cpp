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

#include <Windows.h>

#include <cmath>
#include <mutex>

namespace XenUI {
    namespace {
        constexpr wchar_t kContainerClassName[] = L"XenUI.NativeHost";

        LRESULT CALLBACK ContainerProc(HWND Hwnd, UINT Message, WPARAM WParam, LPARAM LParam) {
            switch (Message) {
                case WM_ERASEBKGND:
                    return 1;
                case WM_PAINT: {
                    PAINTSTRUCT Paint;
                    BeginPaint(Hwnd, &Paint);
                    EndPaint(Hwnd, &Paint);
                    return 0;
                }
                default:
                    return DefWindowProcW(Hwnd, Message, WParam, LParam);
            }
        }

        void RegisterContainerClass() {
            static std::once_flag Registered;
            std::call_once(Registered, [] {
                const WNDCLASSEXW Class {
                  .cbSize        = sizeof(WNDCLASSEXW),
                  .lpfnWndProc   = ContainerProc,
                  .hInstance     = GetModuleHandleW(nullptr),
                  .hCursor       = LoadCursorW(nullptr, IDC_ARROW),
                  .lpszClassName = kContainerClassName,
                };
                RegisterClassExW(&Class);
            });
        }

        RECT ToPixels(const Rect& Area, float Scale) {
            return {static_cast<LONG>(std::lround(Area.X * Scale)),
                    static_cast<LONG>(std::lround(Area.Y * Scale)),
                    static_cast<LONG>(std::lround((Area.X + Area.Width) * Scale)),
                    static_cast<LONG>(std::lround((Area.Y + Area.Height) * Scale))};
        }
    }  // namespace

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
        if (_Container == nullptr && GetOwner() != nullptr && GetOwner()->GetNativeParent() != nullptr) {
            RegisterContainerClass();
            _Parent    = GetOwner()->GetNativeParent();
            _Container = CreateWindowExW(0,
                                         kContainerClassName,
                                         L"",
                                         WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                                         0,
                                         0,
                                         1,
                                         1,
                                         static_cast<HWND>(_Parent),
                                         nullptr,
                                         GetModuleHandleW(nullptr),
                                         nullptr);
            if (_Container != nullptr && _Callbacks.OnAttach) { _Callbacks.OnAttach(_Container); }
        }
        if (_Container != nullptr) {
            GetLayer()->SetNativeAnchor(GetSize(),
                                        [this](const std::optional<NativePlacement>& Placement) { Place(Placement); });
        }
    }

    void RenderNativeHost::Place(const std::optional<NativePlacement>& Placement) {
        const auto Container = static_cast<HWND>(_Container);
        if (Container == nullptr) { return; }
        if (!Placement) {
            ShowWindow(Container, SW_HIDE);
            return;
        }

        const float Scale  = static_cast<float>(GetDpiForWindow(static_cast<HWND>(_Parent))) / USER_DEFAULT_SCREEN_DPI;
        const RECT Bounds  = ToPixels(Placement->Bounds, Scale);
        const RECT Visible = ToPixels(Placement->Visible, Scale);
        const int Width    = Bounds.right - Bounds.left;
        const int Height   = Bounds.bottom - Bounds.top;
        SetWindowPos(Container,
                     nullptr,
                     Bounds.left,
                     Bounds.top,
                     Width,
                     Height,
                     SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);

        const bool Clipped = Visible.left != Bounds.left || Visible.top != Bounds.top ||
                             Visible.right != Bounds.right || Visible.bottom != Bounds.bottom;
        HRGN Region = Clipped ? CreateRectRgn(Visible.left - Bounds.left,
                                              Visible.top - Bounds.top,
                                              Visible.right - Bounds.left,
                                              Visible.bottom - Bounds.top)
                              : nullptr;
        SetWindowRgn(Container, Region, TRUE);

        if (Width != _PixelWidth || Height != _PixelHeight) {
            _PixelWidth  = Width;
            _PixelHeight = Height;
            if (_Callbacks.OnResize) { _Callbacks.OnResize(Width, Height); }
        }
    }

    void RenderNativeHost::OnDetached() {
        DestroyContainer();
        RenderBox::OnDetached();
    }

    void RenderNativeHost::DestroyContainer() {
        if (_Container == nullptr) { return; }
        GetLayer()->ClearNativeAnchor();
        const NativeHandle Container = std::exchange(_Container, nullptr);
        if (_Callbacks.OnDetach) { _Callbacks.OnDetach(Container); }
        if (IsWindow(static_cast<HWND>(Container))) { DestroyWindow(static_cast<HWND>(Container)); }
        _PixelWidth  = 0;
        _PixelHeight = 0;
    }
}  // namespace XenUI
