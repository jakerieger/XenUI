//
// Created by Jake Rieger on 3/13/2024.
//

#pragma once

#include "Color.h"
#include "Element.h"
#include "Interactive.h"
#include "XenAPI.h"
#include "Platform.h"
#include "Rect.h"

#include <functional>

namespace Xen {
    class XEN_API Box final : public Element,
                              public Interactive {
    public:
        Box(const Rect& size,
            const Color& color,
            i64 zIndex,
            Element* child,
            bool rounded                    = false,
            std::function<void()> onPressed = ActionNone);
        ~Box() override;

        void Draw() override;

        Color& GetFillColor() { return FillColor; }
        Color& GetStrokeColor() { return StrokeColor; }
        ID2D1SolidColorBrush* GetBrush() { return Brush; }

    private:
        Color FillColor   = {};
        Color StrokeColor = {};
        bool Rounded;

        ID2D1SolidColorBrush* Brush = nullptr;
    };
}  // namespace Xen