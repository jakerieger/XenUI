//
// Created by conta on 3/17/2024.
//

#include "RenderTree.h"

namespace Xen::RenderTree {
    Unique<Element> g_RootElement;

    void Init() {}

    // clang-format off
    void SetRoot(Element* root) {
        g_RootElement = Unique<Element>(root);
    }
    // clang-format on

    void Render() {}

    void Cleanup() {}
}  // namespace Xen::RenderTree