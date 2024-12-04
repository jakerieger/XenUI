// Author: Jake Rieger
// Created: 12/3/24.
//

#pragma once

#include "Export.hpp"
#include "EventSystem.hpp"

#include <utility>
#include <Types.hpp>

namespace Xen {
    class XEN_API INativeWindow {
    public:
        INativeWindow(const Weak<EventDispatcher>& dispatcher,
                      const str& title,
                      i32 width,
                      i32 height);
        virtual ~INativeWindow();

        virtual void Show()                          = 0;
        virtual void Hide()                          = 0;
        virtual void Minimize()                      = 0;
        virtual void ToggleMaximize()                = 0;
        virtual void Close()                         = 0;
        virtual void ImgBlit(const vector<u8>& data) = 0;
        virtual void DispatchMessages()              = 0;

        [[nodiscard]] bool ShouldExit() const;

        friend class XEN_API X11Window;
        friend class XEN_API WaylandWindow;
        friend class XEN_API NSWindow;
        friend class XEN_API Win32Window;
        friend class XEN_API IApp;

    protected:
        str mTitle;
        i32 mWidth;
        i32 mHeight;
        Weak<EventDispatcher> mDispatcher;

    private:
        bool mMaximized;
        bool mMinimized;
        bool mShouldExit;
        void SetShouldExit(bool exit);
    };
}  // namespace Xen
