//
// Created by conta on 3/18/2024.
//

#pragma once

#include "AppWindow.h"
#include "AppContext.h"
#include "Types.h"
#include "XenAPI.h"
#include "UI/Element.h"

namespace Xen {
    class XEN_API XenApp {
    public:
        XenApp(const Size<u32>& windowSize, const str& windowTitle, u32 windowIcon = 0);
        virtual ~XenApp();
        virtual Element* BuildUI() = 0;
        virtual void RunApp();
        virtual void Shutdown();

        static void AttachRootElement(Element* root);
        [[nodiscard]] AppWindow* GetNativeWindow() const { return RawPtr(Window); }
        AppContext& GetContext() { return Context; }

    protected:
        Unique<AppWindow> Window;
        AppContext Context;
    };
}  // namespace Xen