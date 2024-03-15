//
// Created by Jake Rieger on 3/13/2024.
//

#pragma once

#include "Color.h"
#include "Types.h"
#include "XenAPI.h"
#include "Platform.h"
#include "Rect.h"

namespace Xen {
    class XEN_API Box {
    public:
        Box() = default;
        Box(const Rect& shape, const Color& color, ID2D1RenderTarget* renderTarget);
        ~Box();

        void Draw();
        void UpdateShape(const Rect& shape) { Shape = shape; }

        Rect& GetShape() { return Shape; }
        Color& GetFillColor() { return FillColor; }
        Color& GetStrokeColor() { return StrokeColor; }

    private:
        Rect Shape        = {};
        Color FillColor   = {};
        Color StrokeColor = {};

        ID2D1SolidColorBrush* InternalColor = nullptr;
        ID2D1RenderTarget* RenderTarget     = nullptr;
    };
}  // namespace Xen