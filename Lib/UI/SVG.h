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

// Author: Jake Rieger
// Created: 3/21/2024.
//

#pragma once

#include "Element.h"
#include "Interactive.h"
#include "XenAPI.h"
#include "Platform.h"

namespace Xen {
    struct XEN_API Geometry {
        ID2D1SolidColorBrush* Brush = nullptr;
        ID2D1GeometrySink* Sink     = nullptr;
        ID2D1PathGeometry* Path     = nullptr;
    };

    class XEN_API SVG final : public Element {
    public:
        SVG(i64 zIndex, const str& fileName, const Offset& position, Element* child);
        ~SVG() override;

        void Draw() override;

    private:
        void ParseSVGToD2DGeometry();
        void Update();
        std::vector<Geometry> ShapeData;
        Offset Position;
    };
}  // namespace Xen
