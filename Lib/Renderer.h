//
// Created by Jake Rieger on 3/12/2024.
//

#pragma once

#include "Types.h"
#include "Platform.h"
#include "XenAPI.h"
#include "UI/Offset.h"

namespace Xen {
    class XEN_API XenApp;
}

namespace Xen::Renderer {
    void Init();
    int Render();
    void Shutdown();
    void OnResize(u32 width, u32 height);
    void CheckHit(const Offset& mousePos);
    void CheckOverlap(const Offset& mousePos);
    void SetOwner(XenApp* owner);
    Offset GetRenderTargetCenter();
    u64 GetTicks();

    ID2D1Factory* GetD2DFactory();
    IDWriteFactory* GetDWFactory();
    ID2D1HwndRenderTarget* GetRenderTarget();
    XenApp* GetOwner();
}  // namespace Xen::Renderer