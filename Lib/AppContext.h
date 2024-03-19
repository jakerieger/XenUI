//
// Created by conta on 3/18/2024.
//

#pragma once

#include "XenAPI.h"
#include "UI/Theme.h"

namespace Xen {
    class XEN_API AppContext {
    public:
        Theme AppTheme = XenDarkTheme;
        str FontFamily = "Inter";
    };
}  // namespace Xen