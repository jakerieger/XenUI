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