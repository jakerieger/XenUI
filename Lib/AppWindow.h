//
// Created by Jake Rieger on 3/12/2024.
//

#pragma once

#include "XenAPI.h"
#include "Platform.h"
#include "Types.h"
#include "XenRenderer.h"
#include "UI/Offset.h"
#include "UI/Size.h"
#include <string>
#include <iostream>

namespace Xen {
    class XEN_API AppWindow {
    public:
        AppWindow(const Size<u32>& dimensions, str className, str title);
        void Run();

        [[nodiscard]] HWND GetHandle() const { return Handle; }
        [[nodiscard]] Size<u32> GetDimensions() const { return Dimensions; }
        Offset<i64>& GetCursorPosition() { return CursorPosition; }

    private:
        void Init();
        void MainLoop() const;
        void Shutdown();

        static LRESULT CALLBACK MessageHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

        LRESULT OnResize(UINT width, UINT height);
        LRESULT OnPaint() const;
        LRESULT OnKeyUp();
        LRESULT OnKeyDown();
        LRESULT OnLeftMouseButtonDown() const;
        LRESULT OnLeftMouseButtonUp();
        LRESULT OnRightMouseButtonDown();
        LRESULT OnRightMouseButtonUp();
        LRESULT OnMouseMove(UINT xPos, UINT yPos);

        Size<u32> Dimensions;
        str ClassName;
        str Title;
        HWND Handle                  = nullptr;
        HINSTANCE Instance           = nullptr;
        Unique<XenRenderer> Renderer = nullptr;
        Offset<i64> CursorPosition;
    };
}  // namespace Xen