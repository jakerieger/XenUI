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

#include "XenUI/Core/Color.hpp"
#include "XenUI/Core/Math.hpp"
#include "XenUI/Core/Matrix.hpp"

#include <d2d1_1.h>

namespace XenUI {
    inline D2D1_POINT_2F ToD2D(const Vec2& Point) {
        return {Point.X, Point.Y};
    }

    inline D2D1_RECT_F ToD2D(const Rect& Bounds) {
        return {Bounds.Left(), Bounds.Top(), Bounds.Right(), Bounds.Bottom()};
    }

    inline D2D1_COLOR_F ToD2D(const Color& Value) {
        return {Value.R, Value.G, Value.B, Value.A};
    }

    inline D2D1_MATRIX_3X2_F ToD2D(const Matrix3x2& Transform) {
        D2D1_MATRIX_3X2_F Result;
        Result._11 = Transform.M11;
        Result._12 = Transform.M12;
        Result._21 = Transform.M21;
        Result._22 = Transform.M22;
        Result._31 = Transform.Dx;
        Result._32 = Transform.Dy;
        return Result;
    }

    inline Rect FromD2D(const D2D1_RECT_F& Bounds) {
        return Rect::FromEdges(Bounds.left, Bounds.top, Bounds.right, Bounds.bottom);
    }
}  // namespace XenUI
