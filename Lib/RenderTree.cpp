//
// Created by conta on 3/17/2024.
//

#include "RenderTree.h"
#include "XenApp.h"

namespace Xen::RenderTree {
    Element* g_RootElement;
    XenApp* g_App = nullptr;

    void Init(XenApp* app) { g_App = app; }

    void SetRoot(Element* root) { g_RootElement = root; }

    Element* GetRoot() { return g_RootElement; }

    void RebuildUI() {
        Cleanup();
        SetRoot(g_App->BuildUI());
    }

    void Render() {
        auto currentElement = g_RootElement;
        while (currentElement != nullptr) {
            currentElement->Draw();
            currentElement = currentElement->GetChild();
        }
    }

    std::vector<Element*> GetTreeAsVector() {
        std::vector<Element*> elements(g_App->GetAllocator().GetUsed() / sizeof(Element));
        auto current = static_cast<Element*>(g_App->GetAllocator().GetArena());
        while (current) {
            elements.push_back(current);
            ++current;
        }
        return elements;
    }

    void Cleanup() { g_App->ResetElements(); }
}  // namespace Xen::RenderTree