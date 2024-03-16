//
// Created by Jake Rieger on 3/13/2024.
//

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