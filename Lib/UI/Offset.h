//
// Created by Jake Rieger on 3/13/2024.
//

#pragma once

#include "Helpers.inl"
#include "XenAPI.h"
#include "Types.h"
#include <cmath>

namespace Xen {
    template<typename T = int>
    class XEN_API Offset {
    public:
        Offset() = default;
        Offset(const T x, const T y) : X(x), Y(y) { ASSERT_NUMERIC(T); }

        bool operator==(const Offset&) const;
        Offset operator-();
        Offset operator+(const Offset&) const;
        Offset operator+(T scalar) const;
        Offset operator-(const Offset&) const;
        Offset operator-(T scalar) const;
        Offset operator*(const Offset&) const;
        Offset operator*(T scalar) const;
        Offset operator/(const Offset&) const;
        Offset operator/(T scalar) const;

        static Offset<f64> FromDirection(T direction, T distance);
        static Offset Lerp(const Offset& a, const Offset& b, f64 t);

        f64 Direction();
        f64 Distance();
        f64 DistanceSqr();
        bool IsFinite();
        bool IsInfinite();
        Offset Scale(T scaleX, T scaleY);
        Offset Translate(T translateX, T translateY);

        template<typename newT>
        Offset<newT> To() {
            ASSERT_NUMERIC(T)
            ASSERT_NUMERIC(newT)
            return Offset<newT>(static_cast<newT>(X), static_cast<newT>(Y));
        }

        inline static Offset Infinite = Offset(Infinity<T>(), Infinity<T>());
        inline static Offset Zero     = Offset(static_cast<T>(0), static_cast<T>(0));

        T X = 0;
        T Y = 0;
    };

    template<typename T>
    bool Offset<T>::operator==(const Offset& other) const {
        ASSERT_NUMERIC(T)
        return X == other.X && Y == other.Y;
    }

    template<typename T>
    Offset<T> Offset<T>::operator-() {
        ASSERT_NUMERIC(T)
        return Offset(-X, -Y);
    }

    template<typename T>
    Offset<T> Offset<T>::operator+(const Offset& other) const {
        ASSERT_NUMERIC(T)
        return Offset(X + other.X, Y + other.Y);
    }

    template<typename T>
    Offset<T> Offset<T>::operator+(T scalar) const {
        ASSERT_NUMERIC(T)
        return Offset(X + scalar, Y + scalar);
    }

    template<typename T>
    Offset<T> Offset<T>::operator-(const Offset& other) const {
        ASSERT_NUMERIC(T)
        return Offset(X - other.X, Y - other.Y);
    }

    template<typename T>
    Offset<T> Offset<T>::operator-(T scalar) const {
        ASSERT_NUMERIC(T)
        return Offset(X - scalar, Y - scalar);
    }

    template<typename T>
    Offset<T> Offset<T>::operator*(const Offset& other) const {
        ASSERT_NUMERIC(T)
        return Offset(X + other.X, Y + other.Y);
    }

    template<typename T>
    Offset<T> Offset<T>::operator*(T scalar) const {
        ASSERT_NUMERIC(T)
        return Offset(X * scalar, Y * scalar);
    }

    template<typename T>
    Offset<T> Offset<T>::operator/(const Offset& other) const {
        ASSERT_NUMERIC(T)
        return Offset(X + other.X, Y + other.Y);
    }

    template<typename T>
    Offset<T> Offset<T>::operator/(T scalar) const {
        ASSERT_NUMERIC(T)
        return Offset(X / scalar, Y / scalar);
    }

    template<typename T>
    Offset<f64> Offset<T>::FromDirection(T direction, T distance) {
        ASSERT_NUMERIC(T)
        return Offset(distance * std::cos(direction), distance * std::sin(direction));
    }

    template<typename T>
    Offset<T> Offset<T>::Lerp(const Offset& a, const Offset& b, const f64 t) {
        ASSERT_NUMERIC(T)
        return Offset(Math::Lerp<T>(a.X, b.X, t), Math::Lerp<T>(a.Y, b.Y, t));
    }

    template<typename T>
    f64 Offset<T>::Direction() {
        ASSERT_NUMERIC(T)
        return std::atan2(static_cast<f64>(X), static_cast<f64>(Y));
    }

    template<typename T>
    f64 Offset<T>::Distance() {
        ASSERT_NUMERIC(T)
        return std::sqrt(X * X + Y * Y);
    }

    template<typename T>
    f64 Offset<T>::DistanceSqr() {
        ASSERT_NUMERIC(T)
        return X * X + Y * Y;
    }

    template<typename T>
    bool Offset<T>::IsFinite() {
        ASSERT_NUMERIC(T)
        return X != Infinity<T>() && Y != Infinity<T>();
    }

    template<typename T>
    bool Offset<T>::IsInfinite() {
        ASSERT_NUMERIC(T)
        return X == Infinity<T>() && Y == Infinity<T>();
    }

    template<typename T>
    Offset<T> Offset<T>::Scale(T scaleX, T scaleY) {
        ASSERT_NUMERIC(T)
        return Offset(X * scaleX, Y * scaleY);
    }

    template<typename T>
    Offset<T> Offset<T>::Translate(T translateX, T translateY) {
        ASSERT_NUMERIC(T)
        return Offset(X + translateX, Y + translateY);
    }
}  // namespace Xen