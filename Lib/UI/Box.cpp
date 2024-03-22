// Copyright (C) 2024 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "Box.h"
#include "Renderer.h"

namespace Xen {
    Box::Box(const Rect& size,
             const Color& color,
             const i64 zIndex,
             Element* child,
             const bool rounded,
             std::function<void()> onPressed)
        : Element(zIndex, size), Rounded(rounded) {
        this->OnPressed = std::move(onPressed);
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

    void Box::Draw() {
        if (Rounded) {
            D2D1_ROUNDED_RECT roundedRect;
            roundedRect.rect    = Size.GetD2DRect();
            roundedRect.radiusX = 8.f;
            roundedRect.radiusY = 8.f;
            Renderer::GetRenderTarget()->FillRoundedRectangle(roundedRect, Brush);
            return;
        }

        Renderer::GetRenderTarget()->FillRectangle(Size.GetD2DRect(), Brush);
    }
}  // namespace Xen