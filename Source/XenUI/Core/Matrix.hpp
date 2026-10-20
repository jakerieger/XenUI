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

#include <algorithm>
#include <cmath>
#include <optional>

namespace XenUI {
    /// @brief 2D affine transform using the row-vector convention: `A * B` applies A first, then B.
    struct Matrix3x2 {
        float M11 = 1.0f;
        float M12 = 0.0f;
        float M21 = 0.0f;
        float M22 = 1.0f;
        float Dx  = 0.0f;
        float Dy  = 0.0f;

        static constexpr Matrix3x2 Identity() { return {}; }

        static constexpr Matrix3x2 Translation(const Vec2& Offset) {
            return {1.0f, 0.0f, 0.0f, 1.0f, Offset.X, Offset.Y};
        }

        static constexpr Matrix3x2 Translation(float OffsetX, float OffsetY) {
            return {1.0f, 0.0f, 0.0f, 1.0f, OffsetX, OffsetY};
        }

        static constexpr Matrix3x2 Scaling(float ScaleX, float ScaleY, const Vec2& Center = {}) {
            return {ScaleX, 0.0f, 0.0f, ScaleY, Center.X - ScaleX * Center.X, Center.Y - ScaleY * Center.Y};
        }

        /// @brief Positive angles rotate clockwise on screen (y axis pointing down).
        static Matrix3x2 Rotation(float Radians, const Vec2& Center = {}) {
            const float Cos = std::cos(Radians);
            const float Sin = std::sin(Radians);
            return {
              Cos,
              Sin,
              -Sin,
              Cos,
              Center.X - Cos * Center.X + Sin * Center.Y,
              Center.Y - Sin * Center.X - Cos * Center.Y,
            };
        }

        constexpr Matrix3x2 operator*(const Matrix3x2& Next) const {
            return {
              M11 * Next.M11 + M12 * Next.M21,
              M11 * Next.M12 + M12 * Next.M22,
              M21 * Next.M11 + M22 * Next.M21,
              M21 * Next.M12 + M22 * Next.M22,
              Dx * Next.M11 + Dy * Next.M21 + Next.Dx,
              Dx * Next.M12 + Dy * Next.M22 + Next.Dy,
            };
        }

        [[nodiscard]] constexpr Vec2 TransformPoint(const Vec2& Point) const {
            return {Point.X * M11 + Point.Y * M21 + Dx, Point.X * M12 + Point.Y * M22 + Dy};
        }

        /// @brief Returns the axis-aligned bounding box of the transformed rectangle.
        [[nodiscard]] constexpr Rect TransformRect(const Rect& Bounds) const {
            if (IsTranslation()) { return {Bounds.X + Dx, Bounds.Y + Dy, Bounds.Width, Bounds.Height}; }

            const Vec2 TopLeft     = TransformPoint({Bounds.Left(), Bounds.Top()});
            const Vec2 TopRight    = TransformPoint({Bounds.Right(), Bounds.Top()});
            const Vec2 BottomLeft  = TransformPoint({Bounds.Left(), Bounds.Bottom()});
            const Vec2 BottomRight = TransformPoint({Bounds.Right(), Bounds.Bottom()});
            return Rect::FromEdges(std::min({TopLeft.X, TopRight.X, BottomLeft.X, BottomRight.X}),
                                   std::min({TopLeft.Y, TopRight.Y, BottomLeft.Y, BottomRight.Y}),
                                   std::max({TopLeft.X, TopRight.X, BottomLeft.X, BottomRight.X}),
                                   std::max({TopLeft.Y, TopRight.Y, BottomLeft.Y, BottomRight.Y}));
        }

        [[nodiscard]] constexpr bool IsIdentity() const { return *this == Matrix3x2 {}; }

        [[nodiscard]] constexpr bool IsTranslation() const {
            return M11 == 1.0f && M12 == 0.0f && M21 == 0.0f && M22 == 1.0f;
        }

        [[nodiscard]] constexpr float Determinant() const { return M11 * M22 - M12 * M21; }

        [[nodiscard]] constexpr std::optional<Matrix3x2> Inverted() const {
            const float Det = Determinant();
            if (Det == 0.0f) { return std::nullopt; }

            const float InverseDet = 1.0f / Det;
            return Matrix3x2 {
              M22 * InverseDet,
              -M12 * InverseDet,
              -M21 * InverseDet,
              M11 * InverseDet,
              (M21 * Dy - M22 * Dx) * InverseDet,
              (M12 * Dx - M11 * Dy) * InverseDet,
            };
        }

        /// @brief Largest factor by which this transform stretches any unit vector's axis.
        [[nodiscard]] float GetMaxScale() const {
            return std::sqrt(std::max(M11 * M11 + M12 * M12, M21 * M21 + M22 * M22));
        }

        constexpr bool operator==(const Matrix3x2&) const = default;
    };
}  // namespace XenUI
