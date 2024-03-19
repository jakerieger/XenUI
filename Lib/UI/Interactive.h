//
// Created by conta on 3/19/2024.
//

#pragma once

#include "XenAPI.h"

#include <functional>

namespace Xen {
    class XEN_API Interactive {
    public:
        std::function<void()> OnPressed;
        std::function<void()> OnReleased;
        std::function<void()> OnEnter;
        std::function<void()> OnExit;
    };
}  // namespace Xen