// Author: Jake Rieger
// Created: 3/21/2024.
//

#pragma once

#include "Types.h"
#include "UI/Element.h"

namespace Xen {
    class XenApp;
}  // namespace Xen

namespace Xen::RenderTree {
    void Init(XenApp* app);
    void SetRoot(Element* root);
    void RebuildUI();
    void Render();

    Element* GetRoot();
    std::vector<Element*> GetTreeAsVector();

    XEN_API void Cleanup();
}  // namespace Xen::RenderTree