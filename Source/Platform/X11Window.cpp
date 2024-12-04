// Author: Jake Rieger
// Created: 12/3/24.
//

#include "Platform/X11Window.hpp"

namespace Xen {
    X11Window::X11Window(const Weak<EventDispatcher>& mDispatcher,
                         const str& title,
                         i32 width,
                         i32 height)
        : INativeWindow(mDispatcher, title, width, height), mEvent({}), mCursorXPrev(0),
          mCursorYPrev(0) {
        const auto display = XOpenDisplay(nullptr);
        if (!display) { Panic("XOpenDisplay failed"); }

        mDisplay  = display;
        mScreen   = DefaultScreenOfDisplay(mDisplay);
        mScreenId = DefaultScreen(mDisplay);
        mWindow   = XCreateSimpleWindow(mDisplay,
                                      RootWindowOfScreen(mScreen),
                                      0,
                                      0,
                                      width,
                                      height,
                                      1,
                                      BlackPixel(mDisplay, mScreenId),
                                      WhitePixel(mDisplay, mScreenId));
        XStoreName(mDisplay, mWindow, title.c_str());
        static constexpr auto eventFlags = ButtonPressMask | KeyPressMask | KeyReleaseMask |
                                           ButtonReleaseMask | PointerMotionMask |
                                           StructureNotifyMask | ExposureMask | ConfigureNotify;
        XSelectInput(mDisplay, mWindow, eventFlags);
    }

    X11Window::~X11Window() {
        Close();
        INativeWindow::~INativeWindow();
    }

    void X11Window::Show() {
        XFlush(mDisplay);
        XClearWindow(mDisplay, mWindow);
        XMapRaised(mDisplay, mWindow);
    }

    void X11Window::Hide() {
        XClearWindow(mDisplay, mWindow);
        XLowerWindow(mDisplay, mWindow);
    }

    void X11Window::Minimize() {
        XLowerWindow(mDisplay, mWindow);
    }

    void X11Window::ToggleMaximize() {
        mMaximized = !mMaximized;
    }

    void X11Window::Close() {
        XDestroyWindow(mDisplay, mWindow);
        XCloseDisplay(mDisplay);
        mDisplay = nullptr;
        mScreen  = nullptr;
    }

    void X11Window::ImgBlit(const vector<u8>& data) {
        const auto imageData = new char[mWidth * mHeight * 4];  // 4 bytes per pixel (RGBA)
        memcpy(imageData, data.data(), mWidth * mHeight * 4);

        XImage* image = XCreateImage(mDisplay,
                                     DefaultVisual(mDisplay, mScreenId),
                                     24,
                                     ZPixmap,
                                     0,
                                     imageData,
                                     mWidth,
                                     mHeight,
                                     32,
                                     0);

        const auto gc = XCreateGC(mDisplay, mWindow, 0, nullptr);
        XPutImage(mDisplay, mWindow, gc, image, 0, 0, 0, 0, mWidth, mHeight);

        // Free resources
        XFreeGC(mDisplay, gc);
        image->data = nullptr;  // Prevent xlib from freeing the memory
        XDestroyImage(image);

        delete[] imageData;
    }

    void X11Window::DispatchMessages() {
        XNextEvent(mDisplay, &mEvent);

        static std::chrono::steady_clock::time_point lastResizeTime =
          std::chrono::steady_clock::now();
        static constexpr std::chrono::milliseconds kDebounceDelay(30);

        switch (mEvent.type) {
            case Expose:
                XClearWindow(mDisplay, mWindow);
                mDispatcher.lock()->Dispatch(PaintEvent());
                break;
            case KeyPress: {
                const KeySym key = XLookupKeysym(&mEvent.xkey, 0);
                mDispatcher.lock()->Dispatch(KeyPressEvent(key));
                if (key == XK_Escape) { SetShouldExit(true); }
            } break;
            case ConfigureNotify: {
                const auto xce = mEvent.xconfigure;
                if (xce.width != mWidth or xce.height != mHeight) {
                    mWidth  = xce.width;
                    mHeight = xce.height;

                    // Caps redraw rate to at most 30ms
                    const auto now = std::chrono::steady_clock::now();
                    if (now - lastResizeTime > kDebounceDelay) {
                        lastResizeTime = now;
                        XClearWindow(mDisplay, mWindow);
                        mDispatcher.lock()->Dispatch(ResizeEvent(mWidth, mHeight));
                    }
                }
            } break;
            default:
                break;
        }

        QueryMousePosition();
    }

    void X11Window::QueryMousePosition() {
        Window root, child;
        int rootX, rootY;
        int winX, winY;
        unsigned int mask;

        if (XQueryPointer(mDisplay, mWindow, &root, &child, &rootX, &rootY, &winX, &winY, &mask)) {
            if (winX != mCursorXPrev or winY != mCursorYPrev) {
                mCursorXPrev = winX;
                mCursorYPrev = winY;
                mDispatcher.lock()->Dispatch(CursorMoveEvent(winX, winY));
            }
        }
    }
}  // namespace Xen