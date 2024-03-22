// Copyright (C) 2024 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

// Author: Jake Rieger
// Created: 3/21/2024.
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