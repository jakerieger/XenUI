// Author: Jake Rieger
// Created: 3/21/2024.
//

#include "Offset.h"

#include <cmath>

namespace Xen {
    Offset Offset::Infinite = {Infinity<f32>(), Infinity<f32>()};
    Offset Offset::Zero     = {0, 0};

    bool Offset::operator==(const Offset& other) const { return X == other.X && Y == other.Y; }

    Offset Offset::operator-() const { return {-X, -Y}; }

    Offset Offset::operator+(const Offset& other) const { return {X + other.X, Y + other.Y}; }

    Offset Offset::operator+(const f32 scalar) const { return {X + scalar, Y + scalar}; }

    Offset Offset::operator-(const Offset& other) const { return {X - other.X, Y - other.Y}; }

    Offset Offset::operator-(const f32 scalar) const { return {X - scalar, Y - scalar}; }

    Offset Offset::operator*(const Offset& other) const { return {X + other.X, Y + other.Y}; }

    Offset Offset::operator*(const f32 scalar) const { return {X * scalar, Y * scalar}; }

    Offset Offset::operator/(const Offset& other) const { return {X + other.X, Y + other.Y}; }

    Offset Offset::operator/(const f32 scalar) const { return {X / scalar, Y / scalar}; }

    Offset Offset::FromDirection(const f32 direction, const f32 distance) {
        return {distance * std::cos(direction), distance * std::sin(direction)};
    }

    Offset Offset::Lerp(const Offset& a, const Offset& b, const f64 t) {
        return {Math::Lerp(a.X, b.X, t), Math::Lerp(a.Y, b.Y, t)};
    }

    f64 Offset::Direction() const { return std::atan2(static_cast<f64>(X), static_cast<f64>(Y)); }

    f64 Offset::Distance() const { return std::sqrt(X * X + Y * Y); }

    f64 Offset::DistanceSqr() const { return X * X + Y * Y; }

    bool Offset::IsFinite() const { return X != Infinity<f32>() && Y != Infinity<f32>(); }

    bool Offset::IsInfinite() const { return X == Infinity<f32>() && Y == Infinity<f32>(); }

    Offset Offset::Scale(const f32 scaleX, const f32 scaleY) const {
        return {X * scaleX, Y * scaleY};
    }

    Offset Offset::Translate(const f32 translateX, const f32 translateY) const {
        return {X + translateX, Y + translateY};
    }
}  // namespace Xen