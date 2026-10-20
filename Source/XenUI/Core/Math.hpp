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

#include <algorithm>
#include <limits>

namespace XenUI {
    inline constexpr float kUnbounded = std::numeric_limits<float>::infinity();

    struct Vec2 {
        float X = 0.0f;
        float Y = 0.0f;

        constexpr Vec2 operator+(const Vec2& Other) const { return {X + Other.X, Y + Other.Y}; }
        constexpr Vec2 operator-(const Vec2& Other) const { return {X - Other.X, Y - Other.Y}; }
        constexpr Vec2 operator*(float Scalar) const { return {X * Scalar, Y * Scalar}; }
        constexpr Vec2 operator/(float Scalar) const { return {X / Scalar, Y / Scalar}; }
        constexpr Vec2 operator-() const { return {-X, -Y}; }

        constexpr Vec2& operator+=(const Vec2& Other) { return *this = *this + Other; }
        constexpr Vec2& operator-=(const Vec2& Other) { return *this = *this - Other; }
        constexpr Vec2& operator*=(float Scalar) { return *this = *this * Scalar; }
        constexpr Vec2& operator/=(float Scalar) { return *this = *this / Scalar; }

        constexpr bool operator==(const Vec2&) const = default;
    };

    struct Size {
        float Width  = 0.0f;
        float Height = 0.0f;

        [[nodiscard]] constexpr bool IsEmpty() const { return Width <= 0.0f || Height <= 0.0f; }

        constexpr Size operator*(float Scalar) const { return {Width * Scalar, Height * Scalar}; }
        constexpr Size operator/(float Scalar) const { return {Width / Scalar, Height / Scalar}; }

        constexpr bool operator==(const Size&) const = default;
    };

    struct Rect {
        float X      = 0.0f;
        float Y      = 0.0f;
        float Width  = 0.0f;
        float Height = 0.0f;

        static constexpr Rect FromEdges(float LeftEdge, float TopEdge, float RightEdge, float BottomEdge) {
            return {LeftEdge, TopEdge, RightEdge - LeftEdge, BottomEdge - TopEdge};
        }

        static constexpr Rect FromOriginSize(const Vec2& Position, const XenUI::Size& Dimensions) {
            return {Position.X, Position.Y, Dimensions.Width, Dimensions.Height};
        }

        [[nodiscard]] constexpr float Left() const { return X; }
        [[nodiscard]] constexpr float Top() const { return Y; }
        [[nodiscard]] constexpr float Right() const { return X + Width; }
        [[nodiscard]] constexpr float Bottom() const { return Y + Height; }
        [[nodiscard]] constexpr Vec2 Origin() const { return {X, Y}; }
        [[nodiscard]] constexpr XenUI::Size GetSize() const { return {Width, Height}; }
        [[nodiscard]] constexpr Vec2 Center() const { return {X + Width * 0.5f, Y + Height * 0.5f}; }
        [[nodiscard]] constexpr bool IsEmpty() const { return Width <= 0.0f || Height <= 0.0f; }

        [[nodiscard]] constexpr bool Contains(const Vec2& Point) const {
            return Point.X >= X && Point.X < Right() && Point.Y >= Y && Point.Y < Bottom();
        }

        [[nodiscard]] constexpr bool Intersects(const Rect& Other) const {
            return X < Other.Right() && Other.X < Right() && Y < Other.Bottom() && Other.Y < Bottom();
        }

        [[nodiscard]] constexpr bool ContainsRect(const Rect& Other) const {
            return Other.X >= X && Other.Y >= Y && Other.Right() <= Right() && Other.Bottom() <= Bottom();
        }

        /// @brief Smallest rectangle containing both. Empty rectangles are ignored.
        [[nodiscard]] constexpr Rect Union(const Rect& Other) const {
            if (IsEmpty()) { return Other; }
            if (Other.IsEmpty()) { return *this; }
            return FromEdges(std::min(X, Other.X),
                             std::min(Y, Other.Y),
                             std::max(Right(), Other.Right()),
                             std::max(Bottom(), Other.Bottom()));
        }

        /// @brief Overlapping area, or an empty rectangle when they do not overlap.
        [[nodiscard]] constexpr Rect Intersection(const Rect& Other) const {
            const float NewLeft   = std::max(X, Other.X);
            const float NewTop    = std::max(Y, Other.Y);
            const float NewRight  = std::min(Right(), Other.Right());
            const float NewBottom = std::min(Bottom(), Other.Bottom());
            if (NewRight <= NewLeft || NewBottom <= NewTop) { return {}; }
            return FromEdges(NewLeft, NewTop, NewRight, NewBottom);
        }

        [[nodiscard]] constexpr Rect Inflated(float Amount) const {
            return {X - Amount, Y - Amount, Width + Amount * 2.0f, Height + Amount * 2.0f};
        }

        [[nodiscard]] constexpr Rect Translated(const Vec2& Delta) const {
            return {X + Delta.X, Y + Delta.Y, Width, Height};
        }

        [[nodiscard]] constexpr float Area() const { return IsEmpty() ? 0.0f : Width * Height; }

        constexpr bool operator==(const Rect&) const = default;
    };
}  // namespace XenUI
