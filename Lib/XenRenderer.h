//
// Created by Jake Rieger on 3/12/2024.
//

#pragma once

#include "Types.h"
#include "Platform.h"
#include "XenAPI.h"
#include "UI/Offset.h"

namespace Xen {
    class XEN_API AppWindow;
}

namespace Xen::Renderer {
    void Init();
    int Render();
    void Shutdown();
    void OnResize(u32 width, u32 height);
    void CheckHit(Offset& mousePos);
    void CheckOverlap(Offset& mousePos);
    void SetOwningWindow(AppWindow* owner);
    Offset GetRenderTargetCenter();

    ID2D1Factory* GetD2DFactory();
    IDWriteFactory* GetDWFactory();
    ID2D1HwndRenderTarget* GetRenderTarget();
    AppWindow* GetOwningWindow();
}  // namespace Xen::Renderer