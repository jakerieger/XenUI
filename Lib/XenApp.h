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

#pragma once

#include "AppWindow.h"
#include "AppContext.h"
#include "ElementAllocator.h"
#include "Types.h"
#include "XenAPI.h"
#include "UI/Element.h"

namespace Xen {
#define create new (Allocator)

    class XEN_API XenApp {
    public:
        XenApp(const Size<u32>& windowSize, const str& windowTitle, u32 windowIcon = 0);
        virtual ~XenApp();
        virtual Element* BuildUI() = 0;
        virtual void RunApp();
        virtual void Shutdown();

        static void Quit() { ::PostQuitMessage(0); }
        static void AttachRootElement(Element* root);

        [[nodiscard]] AppWindow* GetNativeWindow() const { return RawPtr(Window); }
        AppContext& GetContext() { return Context; }

        void ResetElements();
        ElementAllocator& GetAllocator() { return Allocator; }

    protected:
        Unique<AppWindow> Window;
        AppContext Context;
        ElementAllocator Allocator;
    };
}  // namespace Xen