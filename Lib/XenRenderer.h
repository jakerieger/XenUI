//
// Created by Jake Rieger on 3/12/2024.
//

#pragma once

#include "Types.h"
#include "Platform.h"
#include "XenAPI.h"
#include "UI/Box.h"
#include "UI/Offset.h"
#include "UI/Rect.h"

#include <vector>

namespace Xen {
    class XEN_API AppWindow;

    // TODO: Refactor into namespace to allow app-wide access to the render target
    class XEN_API XenRenderer {
    public:
        XenRenderer() = default;
        ~XenRenderer();
        void Init();
        [[nodiscard]] HRESULT Render() const;
        void OnResize(u32 width, u32 height) const;
        void SetOwner(AppWindow* owner);
        void CheckHit(Offset<i64>& mousePos) const;

    private:
        HRESULT CreateDeviceResources();
        HRESULT CreateDeviceIndependentResources();
        void DiscardDeviceResources() const;

        ID2D1Factory* D2DFactory               = nullptr;
        ID2D1HwndRenderTarget* D2DRenderTarget = nullptr;
        AppWindow* OwningWindow                = nullptr;

        Box* testBox = nullptr;
    };
}  // namespace Xen