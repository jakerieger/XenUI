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
