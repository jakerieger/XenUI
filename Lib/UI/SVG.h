//
// Created by conta on 3/21/2024.
//

#pragma once

#include "Element.h"
#include "Interactive.h"
#include "XenAPI.h"
#include "Platform.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#define NANOSVG_ALL_COLOR_KEYWORDS
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"

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
        NSVGimage* SVGData = nullptr;
        std::vector<Geometry> ShapeData;
        Offset Position;
    };
}  // namespace Xen
