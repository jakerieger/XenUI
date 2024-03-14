//
// Created by Jake Rieger on 3/13/2024.
//

#pragma once

#include "Types.h"
#include "XenAPI.h"
#include "Platform.h"

namespace Xen {
    class XEN_API Rect {
    public:
        Rect();

    private:
        D2D1_RECT_F D2DRect = {};
    };
}  // namespace Xen