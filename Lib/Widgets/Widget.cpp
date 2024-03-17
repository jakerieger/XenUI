//
// Created by conta on 3/16/2024.
//

#include "Widget.h"

namespace Xen {
    void Widget::Draw(Widget* parent) {
        for (const auto& child : Children) {
            child->Draw(this);
        }
    }
}  // namespace Xen