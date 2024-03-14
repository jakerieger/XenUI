//
// Created by Jake Rieger on 3/12/2024.
//

#pragma once

#include "Types.h"
#include "Platform.h"
#include "XenAPI.h"
#include "UI/Offset.h"

#include <vector>

namespace Xen {
    class XEN_API AppWindow;

    class XEN_API XenRenderer {
    public:
        XenRenderer() = default;
        ~XenRenderer();
        void Init();
        [[nodiscard]] HRESULT Render();
        void OnResize(u32 width, u32 height) const;
        void SetOwner(AppWindow* owner);
        void CheckHit(const Offset<i64>& mousePos) const;

    private:
        HRESULT CreateDeviceResources();
        HRESULT CreateDeviceIndependentResources();
        void DiscardDeviceResources() const;

        ID2D1Factory* D2DFactory               = nullptr;
        ID2D1HwndRenderTarget* D2DRenderTarget = nullptr;
        AppWindow* OwningWindow                = nullptr;

        // TODO: For testing purposes only, will be removed during API refactoring
        // Brush management will eventually become the job of widgets and not the renderer
        std::vector<ID2D1SolidColorBrush*> SolidColorBrushes = {};
        D2D1_RECT_F testRect                                 = {};
    };
}  // namespace Xen