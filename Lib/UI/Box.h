//
// Created by Jake Rieger on 3/13/2024.
//

#pragma once

#include "Color.h"
#include "Element.h"
#include "XenAPI.h"
#include "Platform.h"
#include "Rect.h"

namespace Xen {
    class XEN_API Box final : public Element {
    public:
        explicit Box(const i64 zIndex, Element* child) : Element(zIndex) {
            this->Children.push_back(child);
        }
        Box(const Rect& size, const Color& color, i64 zIndex, Element* child);
        ~Box() override;

        void Draw() override;
        void UpdateSize(const Rect& size) override { Size = size; }

        Rect& GetSize() { return Size; }
        Color& GetFillColor() { return FillColor; }
        Color& GetStrokeColor() { return StrokeColor; }
        ID2D1SolidColorBrush* GetBrush() { return Brush; }

    private:
        Rect Size         = {};
        Color FillColor   = {};
        Color StrokeColor = {};

        ID2D1SolidColorBrush* Brush = nullptr;
    };
}  // namespace Xen