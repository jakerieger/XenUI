//
// Created by conta on 3/17/2024.
//

#include "RenderTree.h"
#include "XenApp.h"

namespace Xen::RenderTree {
    Unique<Element> g_RootElement;
    XenApp* g_App = nullptr;

    void Init(XenApp* app) { g_App = app; }

    void SetRoot(Element* root) { g_RootElement = Unique<Element>(root); }

    void RebuildUI() { g_App->BuildUI(); }

    void Render() {
        auto currentElement = g_RootElement.get();
        while (currentElement != nullptr) {
            currentElement->Draw();
            currentElement = currentElement->GetChild();
        }
    }

    void Cleanup() {}
}  // namespace Xen::RenderTree