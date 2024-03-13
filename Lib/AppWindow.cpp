//
// Created by conta on 3/12/2024.
//

#include "AppWindow.h"
#include "UI/Size.h"

#include <cassert>
#include <utility>

namespace Xen {
    AppWindow::AppWindow(const Size<u32>& dimensions, str className, str title)
        : Dimensions(dimensions), ClassName(std::move(className)), Title(std::move(title)) {}

    void AppWindow::Run() {
        Init();
        MainLoop();
        Shutdown();
    }

    void AppWindow::Init() {
        Instance = ::GetModuleHandle(nullptr);

        WNDCLASSEXA wndClass;
        wndClass.style         = CS_HREDRAW | CS_VREDRAW;
        wndClass.lpfnWndProc   = AppWindow::MessageHandler;
        wndClass.cbClsExtra    = 0;
        wndClass.cbWndExtra    = 0;
        wndClass.hInstance     = Instance;
        wndClass.hIcon         = ::LoadIcon(nullptr, IDI_WINLOGO);
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

        Renderer = MakeUnique<XenRenderer>();
        Renderer->SetOwner(this);
        Renderer->Init();
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
            case WM_MOUSEMOVE:
                return appWindow->OnMouseMove();
            default:
                return DefWindowProc(hwnd, msg, wParam, lParam);
        }
    }

    LRESULT AppWindow::OnResize(UINT width, UINT height) {
        Dimensions.Width  = width;
        Dimensions.Height = height;
        Renderer->OnResize(width, height);
        return 0;
    }

    LRESULT AppWindow::OnPaint() const {
        ::ValidateRect(Handle, nullptr);
        return Renderer->Render();
    }

    LRESULT AppWindow::OnKeyUp() {
        return 0;
    }

    LRESULT AppWindow::OnKeyDown() {
        return 0;
    }

    LRESULT AppWindow::OnLeftMouseButtonDown() {
        return 0;
    }

    LRESULT AppWindow::OnLeftMouseButtonUp() {
        return 0;
    }

    LRESULT AppWindow::OnRightMouseButtonDown() {
        return 0;
    }

    LRESULT AppWindow::OnRightMouseButtonUp() {
        return 0;
    }

    LRESULT AppWindow::OnMouseMove() {
        return 0;
    }
}  // namespace Xen