//
// Created by conta on 3/16/2024.
//

#pragma once

#include "XenAPI.h"

#include <vector>

namespace Xen {
    // ReSharper disable once CppClassCanBeFinal
    class XEN_API Widget {
    public:
        Widget()          = default;
        virtual ~Widget() = default;

    private:
        std::vector<Widget*> Children;
    };
}  // namespace Xen