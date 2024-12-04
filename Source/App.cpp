// Author: Jake Rieger
// Created: 12/3/24.
//

#include "App.hpp"

#include "Events.hpp"
#include "PlatformWindow.hpp"

namespace Xen {
    IApp::IApp() {
        mEventDispatcher = std::make_shared<EventDispatcher>();
        mWindow = std::make_unique<WindowType>(mEventDispatcher, "XenApp (Change This)", 400, 300);
        RegisterEvents();
    }

    i32 IApp::Run() const {
        mWindow->Show();
        while (!mWindow->ShouldExit()) {
            mWindow->DispatchMessages();
        }
        return 0;
    }

    void IApp::OnPaint() {
        // mWindow->ImgBlit({'\0'});
    }

    void IApp::OnResize(i32 width, i32 height) {}

    void IApp::OnClose() {}

    void IApp::OnKeyPress(u64 keyCode) {}

    void IApp::OnCursorMove(i32 x, i32 y) {}

    void IApp::RegisterEvents() {
        mEventDispatcher->RegisterListener<PaintEvent>([this](const PaintEvent&) { OnPaint(); });
        mEventDispatcher->RegisterListener<ResizeEvent>(
          [this](const ResizeEvent& e) { OnResize(e.Width, e.Height); });
        mEventDispatcher->RegisterListener<CloseEvent>([this](const CloseEvent&) { OnClose(); });
        mEventDispatcher->RegisterListener<CursorMoveEvent>(
          [this](const CursorMoveEvent& e) { OnCursorMove(e.X, e.Y); });
        mEventDispatcher->RegisterListener<KeyPressEvent>(
          [this](const KeyPressEvent& e) { OnKeyPress(e.KeyCode); });
    }
}  // namespace Xen