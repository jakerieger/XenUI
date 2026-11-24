// Copyright 2026 Jake Rieger
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "XenUI/Core/Math.hpp"
#include "XenUI/Core/Matrix.hpp"
#include "XenUI/Graphics/GL/PathData.hpp"
#include "XenUI/Graphics/StrokeStyle.hpp"

#include <cstdint>
#include <vector>

// CPU geometry for shapes the GPU shader can't draw analytically: arbitrary paths, dashed strokes and unusual joins.
// Strokes become closed polygons whose union is the stroked area, and polygons become an exact-area coverage mask,
// which matches Direct2D's per-primitive antialiasing.

namespace XenUI {
    using Contour = std::vector<Vec2>;

    /// @brief An integer pixel rectangle.
    struct PixelBox {
        int32_t X      = 0;
        int32_t Y      = 0;
        int32_t Width  = 0;
        int32_t Height = 0;

        [[nodiscard]] bool IsEmpty() const { return Width <= 0 || Height <= 0; }
    };

    /// @brief The outline of Figures stroked with Stroke, as polygons wound the same way, so filling them with the
    /// non-zero rule gives the stroked area. Tolerance is how far round joins and caps may stray from true arcs, in
    /// the figures' units.
    [[nodiscard]] std::vector<Contour>
    StrokeFigures(const std::vector<PathData::Figure>& Figures, const StrokeStyle& Stroke, float Tolerance);

    /// @brief Maps every point through Transform.
    void TransformContours(std::vector<Contour>& Contours, const Matrix3x2& Transform);

    /// @brief Pixel-aligned bounds of Contours, or an empty box.
    [[nodiscard]] PixelBox GetContourBounds(const std::vector<Contour>& Contours);

    /// @brief Rasterizes the closed Contours into Region, writing one byte of exact area coverage per pixel (row
    /// major, Region.Width per row) into Mask.
    void RasterizeCoverage(const std::vector<Contour>& Contours,
                           FillRule Rule,
                           const PixelBox& Region,
                           std::vector<uint8_t>& Mask);
}  // namespace XenUI
