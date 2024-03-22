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

#include "Types.h"
#include <complex>

namespace Xen {
    template<typename T = int>
    class Size {
    public:
        Size(T width, T height);

        static Size Copy(Size& other);
        static Size FromRadius(T radius);
        static Size Square(T dimension);
        static Size FromHeight(T height);
        static Size FromWidth(T width);

        f64 GetAspectRatio();
        f64 GetHypotenuse();

        template<typename newT>
        Size<newT> To() {
            return Size<newT>(static_cast<newT>(Width), static_cast<newT>(Height));
        }

        Offset AsOffset() { return Offset(Width, Height); }

        T Width;
        T Height;
    };

    template<typename T>
    Size<T>::Size(T width, T height) : Width(width), Height(height) {}

    template<typename T>
    Size<T> Size<T>::Copy(Size& other) {
        return Size(other.Width, other.Height);
    }

    template<typename T>
    Size<T> Size<T>::FromRadius(T radius) {
        return Size(radius * 2, radius * 2);
    }

    template<typename T>
    Size<T> Size<T>::Square(T dimension) {
        return Size(dimension, dimension);
    }

    template<typename T>
    Size<T> Size<T>::FromHeight(T height) {
        return Size(std::numeric_limits<T>::max(), height);
    }

    template<typename T>
    Size<T> Size<T>::FromWidth(T width) {
        return Size(width, std::numeric_limits<T>::max());
    }

    template<typename T>
    f64 Size<T>::GetAspectRatio() {
        return static_cast<f64>(Width) / static_cast<f64>(Height);
    }

    template<typename T>
    f64 Size<T>::GetHypotenuse() {
        T aSqrd = Height * Height;
        T bSqrd = Width * Width;
        return std::sqrt(aSqrd + bSqrd);
    }
}  // namespace Xen