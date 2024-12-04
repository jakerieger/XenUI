// Author: Jake Rieger
// Created: 12/3/24.
//

#pragma once

#include "EventSystem.hpp"
#include "Export.hpp"
#include "NativeWindow.hpp"

#include <Types.hpp>

namespace Xen {
    class XEN_API IApp {
    public:
        IApp();
        virtual ~IApp() = default;
        [[nodiscard]] i32 Run() const;

        virtual void OnPaint();
        virtual void OnResize(i32 width, i32 height);
        virtual void OnClose();
        virtual void OnKeyPress(u64 keyCode);
        virtual void OnCursorMove(i32 x, i32 y);

    private:
        Shared<EventDispatcher> mEventDispatcher;
        Unique<INativeWindow> mWindow;

        void RegisterEvents();
    };
}  // namespace Xen
