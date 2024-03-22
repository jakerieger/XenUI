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

#include "Helpers.inl"
#include "XenAPI.h"
#include "Types.h"

namespace Xen {
    class XEN_API Offset {
    public:
        Offset() = default;
        Offset(const f32 x, const f32 y) : X(x), Y(y) {}

        bool operator==(const Offset&) const;
        Offset operator-() const;
        Offset operator+(const Offset&) const;
        Offset operator+(f32 scalar) const;
        Offset operator-(const Offset&) const;
        Offset operator-(f32 scalar) const;
        Offset operator*(const Offset&) const;
        Offset operator*(f32 scalar) const;
        Offset operator/(const Offset&) const;
        Offset operator/(f32 scalar) const;

        static Offset FromDirection(f32 direction, f32 distance);
        static Offset Lerp(const Offset& a, const Offset& b, f64 t);

        f64 Direction() const;
        f64 Distance() const;
        f64 DistanceSqr() const;
        bool IsFinite() const;
        bool IsInfinite() const;
        Offset Scale(f32 scaleX, f32 scaleY) const;
        Offset Translate(f32 translateX, f32 translateY) const;

        static Offset Infinite;
        static Offset Zero;

        f32 X = 0;
        f32 Y = 0;
    };
}  // namespace Xen