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

#include "Offset.h"
#include "Types.h"
#include "XenAPI.h"
#include "Platform.h"
#include "Size.h"

namespace Xen {
    class XEN_API Rect {
    public:
        Rect() = default;
        Rect(const Offset& offset, const Size<f32>& size);
        Rect(f32 left, f32 top, f32 right, f32 bottom);

        static Rect FromCenter(const Offset& center, f32 width, f32 height);
        static Rect FromCircle(const Offset& center, f32 radius);
        static Rect FromLTWH(f32 left, f32 top, f32 width, f32 height);
        static Rect FromPoints(const Offset& a, const Offset& b);
        static Rect Copy(const Rect& other);

        [[nodiscard]] f32 Height() const;
        [[nodiscard]] f32 Width() const;
        [[nodiscard]] f32 Top() const;
        [[nodiscard]] f32 Left() const;
        [[nodiscard]] f32 Bottom() const;
        [[nodiscard]] f32 Right() const;
        [[nodiscard]] f32 LongestSide() const;
        [[nodiscard]] f32 ShortestSide() const;

        [[nodiscard]] Offset BottomCenter() const;
        [[nodiscard]] Offset BottomLeft() const;
        [[nodiscard]] Offset BottomRight() const;
        [[nodiscard]] Offset Center() const;
        [[nodiscard]] Offset CenterLeft() const;
        [[nodiscard]] Offset CenterRight() const;
        [[nodiscard]] Offset TopCenter() const;
        [[nodiscard]] Offset TopLeft() const;
        [[nodiscard]] Offset TopRight() const;

        [[nodiscard]] bool Contains(const Offset& offset) const;
        [[nodiscard]] bool Overlaps(const Rect& other) const;

        [[nodiscard]] Rect Shrink(f32 delta) const;
        [[nodiscard]] Rect BoundingBox(const Rect& other) const;
        [[nodiscard]] Rect Grow(f32 delta) const;
        [[nodiscard]] Rect Intersect(const Rect& other) const;
        [[nodiscard]] Rect Shift(const Offset& offset) const;
        [[nodiscard]] Rect Translate(f32 translateX, f32 translateY) const;

        static Rect Largest;
        static Rect Zero;
        static Rect Lerp(const Rect& a, const Rect& b, f32 t);

        [[nodiscard]] D2D1_RECT_F& GetD2DRect() { return InternalRect; }

    private:
        D2D1_RECT_F InternalRect = {};
    };
}  // namespace Xen