//
// Created by conta on 3/13/2024.
//

#pragma once

#include "XenAPI.h"
#include "Types.h"

namespace Xen {
    class XEN_API Offset {
    public:
        Offset() = default;
        Offset(const i64 x, const i64 y) : X(x), Y(y) {}

        i64 X = 0;
        i64 Y = 0;
    };
}  // namespace Xen