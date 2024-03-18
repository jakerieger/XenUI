//
// Created by Jake Rieger on 3/13/2024.
//

#include "Box.h"
#include "Renderer.h"

namespace Xen {
    Box::Box(const Rect& size, const Color& color, const i64 zIndex, Element* child)
        : Element(zIndex) {
        this->Children.push_back(child);
        Size        = size;
        FillColor   = color;
        StrokeColor = color;

        if (const auto hr =
              Renderer::GetRenderTarget()->CreateSolidColorBrush(FillColor.GetD2DColor(), &Brush);
            FAILED(hr)) {
            throw std::runtime_error("Could not create D2D color brush");
        }
    }

    Box::~Box() { SafeRelease(&Brush); }

    void Box::Draw() { Renderer::GetRenderTarget()->FillRectangle(Size.GetD2DRect(), Brush); }
}  // namespace Xen