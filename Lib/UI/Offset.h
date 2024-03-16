//
// Created by Jake Rieger on 3/13/2024.
//

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