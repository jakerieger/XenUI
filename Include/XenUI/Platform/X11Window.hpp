// Author: Jake Rieger
// Created: 12/3/24.
//

#pragma once

#include "NativeWindow.hpp"
#include "Events.hpp"

#include <Panic.hpp>
#include <iostream>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <chrono>
#include <cstring>

namespace Xen {
    class X11Window final : public INativeWindow {
    public:
        X11Window(const Weak<EventDispatcher>& dispatcher, const str& title, i32 width, i32 height);
        ~X11Window() override;

        void Show() override;
        void Hide() override;
        void Minimize() override;
        void ToggleMaximize() override;
        void Close() override;
        void ImgBlit(const vector<u8>& data) override;
        void DispatchMessages() override;

    private:
        Display* mDisplay;
        Screen* mScreen;
        Window mWindow;
        XEvent mEvent;
        i32 mScreenId;
        i32 mCursorXPrev;
        i32 mCursorYPrev;

        void QueryMousePosition();
    };
}  // namespace Xen