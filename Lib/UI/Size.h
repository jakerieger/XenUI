//
// Created by conta on 3/13/2024.
//

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

        double GetAspectRatio();
        double GetHypotenuse();

        template<typename newT>
        Size<newT> To() {
            return Size<newT>(static_cast<newT>(Width), static_cast<newT>(Height));
        }

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
        return Size(std::numeric_limits<T>::infinity(), height);
    }

    template<typename T>
    Size<T> Size<T>::FromWidth(T width) {
        return Size(width, std::numeric_limits<T>::infinity());
    }

    template<typename T>
    double Size<T>::GetAspectRatio() {
        // Stupid hack to stop C++ from chopping off the decimal
        return (Width * 1.f) / (Height * 1.f);
    }

    template<typename T>
    double Size<T>::GetHypotenuse() {
        T aSqrd = Height * Height;
        T bSqrd = Width * Width;
        return std::sqrt(aSqrd + bSqrd);
    }
}  // namespace Xen