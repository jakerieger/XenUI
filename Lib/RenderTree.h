//
// Created by conta on 3/17/2024.
//

#pragma once

#include "Types.h"
#include "UI/Element.h"

namespace Xen::RenderTree {
    void Init();
    void SetRoot(Element* root);
    void Render();
    void Cleanup();
}  // namespace Xen::RenderTree