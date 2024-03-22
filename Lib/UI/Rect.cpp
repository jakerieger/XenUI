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

#include "Rect.h"
#include "Helpers.inl"

namespace Xen {
    Rect Rect::Largest = FromPoints(Offset(0.f, 0.f), Offset(MAX_SIZE_F, MAX_SIZE_F));
    Rect Rect::Zero    = {0, 0, 0, 0};

    Rect::Rect(const Offset& offset, const Size<f32>& size) {
        InternalRect.bottom = offset.Y + size.Height;
        InternalRect.left   = offset.X;
        InternalRect.right  = offset.X + size.Width;
        InternalRect.top    = offset.Y;
    }

    Rect::Rect(const f32 left, const f32 top, const f32 right, const f32 bottom) {
        InternalRect.bottom = bottom;
        InternalRect.left   = left;
        InternalRect.right  = right;
        InternalRect.top    = top;
    }

    Rect Rect::FromCenter(const Offset& center, const f32 width, const f32 height) {
        const auto left   = center.X - width / 2;
        const auto top    = center.Y - height / 2;
        const auto right  = center.X + width / 2;
        const auto bottom = center.Y + height / 2;
        return Rect {left, top, right, bottom};
    }

    Rect Rect::FromCircle(const Offset& center, const f32 radius) {
        return FromCenter(center, radius * 2, radius * 2);
    }

    Rect Rect::FromLTWH(const f32 left, const f32 top, const f32 width, const f32 height) {
        return Rect {left, top, left + width, top + height};
    }

    Rect Rect::FromPoints(const Offset& a, const Offset& b) {
        return Rect {std::min(a.X, b.X),
                     std::min(a.Y, b.Y),
                     std::max(a.X, b.X),
                     std::max(a.Y, b.Y)};
    }

    Rect Rect::Copy(const Rect& other) {
        return {other.Left(), other.Top(), other.Right(), other.Bottom()};
    }

    f32 Rect::Height() const { return InternalRect.bottom - InternalRect.top; }
    f32 Rect::Width() const { return InternalRect.right - InternalRect.left; }
    f32 Rect::Top() const { return InternalRect.top; }
    f32 Rect::Left() const { return InternalRect.left; }
    f32 Rect::Bottom() const { return InternalRect.bottom; }
    f32 Rect::Right() const { return InternalRect.right; }
    f32 Rect::LongestSide() const { return std::max(Width(), Height()); }
    f32 Rect::ShortestSide() const { return std::min(Width(), Height()); }

    Offset Rect::BottomCenter() const { return {Left() + Width() / 2.f, Bottom()}; }
    Offset Rect::BottomLeft() const { return {Left(), Bottom()}; }
    Offset Rect::BottomRight() const { return {Right(), Bottom()}; }
    Offset Rect::Center() const { return {Left() + Width() / 2.f, Top() + Height() / 2.f}; }
    Offset Rect::CenterLeft() const { return {Left(), Top() + Height() / 2.f}; }
    Offset Rect::CenterRight() const { return {Right(), Top() + Height() / 2.f}; }
    Offset Rect::TopCenter() const { return {Left() + Width() / 2.f, Top()}; }
    Offset Rect::TopLeft() const { return {Left(), Top()}; }
    Offset Rect::TopRight() const { return {Right(), Top()}; }

    bool Rect::Contains(const Offset& offset) const {
        return offset.X >= Left() && offset.X < Right() && offset.Y >= Top() && offset.Y < Bottom();
    }

    bool Rect::Overlaps(const Rect& other) const {
        if (Right() <= other.Left() || other.Right() <= Left()) {
            return false;
        }
        if (Bottom() <= other.Top() || other.Bottom() <= Top()) {
            return false;
        }
        return true;
    }

    Rect Rect::Shrink(const f32 delta) const {
        return {Left() + delta, Top() + delta, Right() - delta, Bottom() - delta};
    }

    Rect Rect::BoundingBox(const Rect& other) const {
        return {std::min(Left(), other.Left()),
                std::min(Top(), other.Top()),
                std::max(Right(), other.Right()),
                std::max(Bottom(), other.Bottom())};
    }

    Rect Rect::Grow(const f32 delta) const {
        return {Left() - delta, Top() - delta, Right() + delta, Bottom() + delta};
    }

    Rect Rect::Intersect(const Rect& other) const {
        return {std::max(Left(), other.Left()),
                std::max(Top(), other.Top()),
                std::min(Right(), other.Right()),
                std::min(Bottom(), other.Bottom())};
    }

    Rect Rect::Shift(const Offset& offset) const {
        return {Left() + offset.X, Top() + offset.Y, Right() + offset.X, Bottom() + offset.Y};
    }

    Rect Rect::Translate(const f32 translateX, const f32 translateY) const {
        return {Left() + translateX,
                Top() + translateY,
                Right() + translateX,
                Bottom() + translateY};
    }

    Rect Rect::Lerp(const Rect& a, const Rect& b, const f32 t) {
        return {Math::Lerp(a.Left(), b.Left(), t),
                Math::Lerp(a.Top(), b.Top(), t),
                Math::Lerp(a.Right(), b.Right(), t),
                Math::Lerp(a.Bottom(), b.Bottom(), t)};
    }
}  // namespace Xen