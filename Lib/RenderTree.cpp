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

    Element* GetRoot() { return RawPtr(g_RootElement); }

    void RebuildUI() { SetRoot(g_App->BuildUI()); }

    void Render() {
        auto currentElement = g_RootElement.get();
        while (currentElement != nullptr) {
            currentElement->Draw();
            currentElement = currentElement->GetChild();
        }
    }

    void Cleanup() {
        std::vector<Element*> elements;
        auto currentElement = g_RootElement.release();
        while (currentElement != nullptr) {
            elements.push_back(currentElement);
            currentElement = currentElement->GetChild();
        }
        std::ranges::reverse(elements);
        for (const auto el : elements) {
            delete el;
        }
    }
}  // namespace Xen::RenderTree