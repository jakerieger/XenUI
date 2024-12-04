// Author: Jake Rieger
// Created: 12/3/24.
//

#include "NativeWindow.hpp"

namespace Xen {
    INativeWindow::INativeWindow(const Weak<EventDispatcher>& dispatcher,
                                 const str& title,
                                 i32 width,
                                 i32 height) {
        mTitle      = title;
        mWidth      = width;
        mHeight     = height;
        mDispatcher = dispatcher;
        mMinimized  = false;
        mMaximized  = false;
        mShouldExit = false;
    }

    INativeWindow::~INativeWindow() = default;

    bool INativeWindow::ShouldExit() const {
        return mShouldExit;
    }

    void INativeWindow::SetShouldExit(bool exit) {
        mShouldExit = exit;
    }
}  // namespace Xen