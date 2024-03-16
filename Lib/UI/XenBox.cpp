//
// Created by Jake Rieger on 3/13/2024.
//

#include "XenBox.h"
#include "XenRenderer.h"

namespace Xen {
    XenBox::XenBox(const Rect& size, const Color& color) {
        Size        = size;
        FillColor   = color;
        StrokeColor = color;

        if (const auto hr =
              Renderer::GetRenderTarget()->CreateSolidColorBrush(FillColor.GetD2DColor(), &Brush);
            FAILED(hr)) {
            throw std::runtime_error("Could not create D2D color brush");
        }
    }

    XenBox::~XenBox() { SafeRelease(&Brush); }

    void XenBox::Draw() { Renderer::GetRenderTarget()->FillRectangle(Size.GetD2DRect(), Brush); }
}  // namespace Xen