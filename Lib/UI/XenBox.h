//
// Created by Jake Rieger on 3/13/2024.
//

#pragma once

#include "Color.h"
#include "XenAPI.h"
#include "Platform.h"
#include "Rect.h"

namespace Xen {
    class XEN_API XenBox {
    public:
        XenBox() = default;
        XenBox(const Rect& shape, const Color& color);
        ~XenBox();

        void Draw();
        void UpdateShape(const Rect& shape) { Shape = shape; }

        Rect& GetShape() { return Shape; }
        Color& GetFillColor() { return FillColor; }
        Color& GetStrokeColor() { return StrokeColor; }
        ID2D1SolidColorBrush* GetBrush() { return Brush; }

    private:
        Rect Shape        = {};
        Color FillColor   = {};
        Color StrokeColor = {};

        ID2D1SolidColorBrush* Brush = nullptr;
    };
}  // namespace Xen