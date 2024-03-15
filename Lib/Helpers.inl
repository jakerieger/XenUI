//
// Created by Jake Rieger on 3/14/2024.
//

#pragma once

#include "Types.h"
#include <cassert>

namespace Xen::Math {
    template<typename T>
    bool IsFinite(T val) {
        return !(val == Infinity<T>());
    }

    template<typename T>
    T Lerp(T a, T b, double t) {
        if (a == b) {
            return a;
        }

        assert(IsFinite<T>(a));
        assert(IsFinite<T>(b));
        assert(IsFinite<double>(t));

        return a * (1.0 - t) + b * t;
    }
}  // namespace Xen::Math

#define ASSERT_NUMERIC(T) static_assert(std::is_arithmetic<T>::value, "Not a numeric type");