//
// Created by Jake Rieger on 3/12/2024.
//

#pragma once

#include "XenAPI.h"
#include "Platform.h"
#include "Types.h"
#include "UI/Offset.h"
#include "UI/Size.h"
#include <string>
#include <iostream>

namespace Xen {
    class XEN_API AppWindow {
    public:
        AppWindow(const Size<u32>& dimensions, str className, str title, int windowIcon);
        void Init();
        void Run() const;
        void Shutdown();

        [[nodiscard]] HWND GetHandle() const { return Handle; }
        [[nodiscard]] Size<u32> GetDimensions() const { return Dimensions; }
        Offset& GetCursorPosition() { return CursorPosition; }
        [[nodiscard]] Offset GetWindowCenter() const {
            return {static_cast<f32>(Dimensions.Width) / 2.f,
                    static_cast<f32>(Dimensions.Height) / 2.f};
        }

        void Quit() const noexcept;
        void Minimize() noexcept;
        void ToggleMaximize() noexcept;

    private:
        void MainLoop() const;

        static LRESULT CALLBACK MessageHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

        LRESULT OnResize(UINT width, UINT height);
        LRESULT OnPaint() const;
        LRESULT OnKeyUp();
        LRESULT OnKeyDown();
        LRESULT OnLeftMouseButtonDown();
        LRESULT OnLeftMouseButtonUp();
        LRESULT OnRightMouseButtonDown();
        LRESULT OnRightMouseButtonUp();
        LRESULT OnMouseMove(UINT xPos, UINT yPos);

        Size<u32> Dimensions;
        str ClassName;
        str Title;
        HWND Handle        = nullptr;
        HINSTANCE Instance = nullptr;
        Offset CursorPosition;
        int WindowIcon = 0;
    };
}  // namespace Xen