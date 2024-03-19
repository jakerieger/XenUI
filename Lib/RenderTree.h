//
// Created by conta on 3/17/2024.
//

#pragma once

#include "Types.h"
#include "UI/Element.h"

namespace Xen {
    class XenApp;
}

namespace Xen::RenderTree {
    void Init(XenApp* app);
    void SetRoot(Element* root);
    Element* GetRoot();
    void RebuildUI();
    void Render();
    void Cleanup();
}  // namespace Xen::RenderTree