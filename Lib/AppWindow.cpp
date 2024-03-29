// Copyright (C) 2024 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "AppWindow.h"
#include "Renderer.h"
#include "UI/Size.h"

#include <cassert>
#include <utility>

namespace Xen {
    AppWindow::AppWindow(const Size<u32>& dimensions,
                         str className,
                         str title,
                         const int windowIcon)
        : Dimensions(dimensions), ClassName(std::move(className)), Title(std::move(title)),
          WindowIcon(windowIcon) {}

    void AppWindow::Run() const { MainLoop(); }

    void AppWindow::Quit() const noexcept { ::PostQuitMessage(0); }

    void AppWindow::Minimize() noexcept {}

    void AppWindow::ToggleMaximize() noexcept {}

    void AppWindow::Init() {
        Instance      = ::GetModuleHandle(nullptr);
        HICON appIcon = ::LoadIcon(Instance, MAKEINTRESOURCE(WindowIcon));

        WNDCLASSEXA wndClass;
        wndClass.style         = CS_HREDRAW | CS_VREDRAW;
        wndClass.lpfnWndProc   = AppWindow::MessageHandler;
        wndClass.cbClsExtra    = 0;
        wndClass.cbWndExtra    = 0;
        wndClass.hInstance     = Instance;
        wndClass.hIcon         = appIcon;
        wndClass.hIconSm       = wndClass.hIcon;
        wndClass.hCursor       = ::LoadCursor(nullptr, IDC_ARROW);
        wndClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wndClass.lpszMenuName  = nullptr;
        wndClass.lpszClassName = ClassName.c_str();
        wndClass.cbSize        = sizeof(WNDCLASSEXW);
        wndClass.cbWndExtra    = sizeof(AppWindow);

        static ATOM registerResult = ::RegisterClassExA(&wndClass);
        assert(registerResult > 0 && "Window class registration failed");

        const u32 scrWidth  = ::GetSystemMetrics(SM_CXSCREEN);
        const u32 scrHeight = ::GetSystemMetrics(SM_CYSCREEN);
        const u32 posX      = (scrWidth - Dimensions.Width) / 2;
        const u32 posY      = (scrHeight - Dimensions.Height) / 2;

        Handle = ::CreateWindowExA(NULL,
                                   ClassName.c_str(),
                                   Title.c_str(),
                                   WS_OVERLAPPEDWINDOW,
                                   static_cast<i32>(posX),
                                   static_cast<i32>(posY),
                                   static_cast<i32>(Dimensions.Width),
                                   static_cast<i32>(Dimensions.Height),
                                   nullptr,
                                   nullptr,
                                   Instance,
                                   this);
        ::SetWindowLongPtr(Handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        assert(Handle != nullptr);
    }

    void AppWindow::MainLoop() const {
        ::ShowWindow(Handle, SW_SHOW);
        ::UpdateWindow(Handle);

        MSG msg = {nullptr};

        while (msg.message != WM_QUIT) {
            if (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                ::TranslateMessage(&msg);
                ::DispatchMessage(&msg);
            }
        }
    }

    void AppWindow::Shutdown() {
        ::DestroyWindow(Handle);
        Handle = nullptr;
    }

    LRESULT AppWindow::MessageHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        const LONG_PTR userData = ::GetWindowLongPtr(hwnd, GWLP_USERDATA);
        const auto appWindow    = reinterpret_cast<AppWindow*>(userData);

        if (!appWindow) {
            return DefWindowProc(hwnd, msg, wParam, lParam);
        }

        switch (msg) {
            case WM_DESTROY:
            case WM_CLOSE:
                ::PostQuitMessage(0);
                return 0;
            case WM_SIZE: {
                const UINT width  = LOWORD(lParam);
                const UINT height = HIWORD(lParam);
                return appWindow->OnResize(width, height);
            }
            case WM_DISPLAYCHANGE: {
                ::InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            case WM_PAINT: {
                return appWindow->OnPaint();
            }
            case WM_KEYDOWN:
                return appWindow->OnKeyDown();
            case WM_KEYUP:
                return appWindow->OnKeyUp();
            case WM_LBUTTONDOWN:
                return appWindow->OnLeftMouseButtonDown();
            case WM_LBUTTONUP:
                return appWindow->OnLeftMouseButtonUp();
            case WM_RBUTTONDOWN:
                return appWindow->OnRightMouseButtonDown();
            case WM_RBUTTONUP:
                return appWindow->OnRightMouseButtonUp();
            case WM_MOUSEMOVE: {
                const auto xPos = GET_X_LPARAM(lParam);
                const auto yPos = GET_Y_LPARAM(lParam);
                return appWindow->OnMouseMove(xPos, yPos);
            }
            default:
                return DefWindowProc(hwnd, msg, wParam, lParam);
        }
    }

    LRESULT AppWindow::OnResize(UINT width, UINT height) {
        Dimensions.Width  = width;
        Dimensions.Height = height;
        Renderer::OnResize(width, height);
        return 0;
    }

    LRESULT AppWindow::OnPaint() const {
        ::ValidateRect(Handle, nullptr);
        return Renderer::Render();
    }

    LRESULT AppWindow::OnKeyUp() { return 0; }

    LRESULT AppWindow::OnKeyDown() { return 0; }

    LRESULT AppWindow::OnLeftMouseButtonDown() {
        Renderer::CheckHit(CursorPosition);
        return 0;
    }

    LRESULT AppWindow::OnLeftMouseButtonUp() { return 0; }

    LRESULT AppWindow::OnRightMouseButtonDown() { return 0; }

    LRESULT AppWindow::OnRightMouseButtonUp() { return 0; }

    LRESULT AppWindow::OnMouseMove(const UINT xPos, const UINT yPos) {
        CursorPosition.X = static_cast<f32>(xPos);
        CursorPosition.Y = static_cast<f32>(yPos);
        Renderer::CheckOverlap(CursorPosition);
        return 0;
    }
}  // namespace Xen