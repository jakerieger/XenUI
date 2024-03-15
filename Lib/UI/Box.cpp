//
// Created by Jake Rieger on 3/13/2024.
//

#include "Box.h"

namespace Xen {
    Box::Box(const Rect& shape, const Color& color, ID2D1RenderTarget* renderTarget) {
        Shape        = shape;
        FillColor    = color;
        StrokeColor  = color;
        RenderTarget = renderTarget;

        if (const auto hr =
              RenderTarget->CreateSolidColorBrush(FillColor.GetD2DColor(), &InternalColor);
            FAILED(hr)) {
            throw std::runtime_error("Could not create D2D color brush");
        }
    }

    Box::~Box() { SafeRelease(&InternalColor); }

    void Box::Draw() { RenderTarget->FillRectangle(Shape.GetD2DRect(), InternalColor); }
}  // namespace Xen