//
// Created by conta on 3/18/2024.
//

#include "XenApp.h"

#include "RenderTree.h"
#include "Renderer.h"

const std::string DEFAULT_CLASSNAME = "XenAppClass";

namespace Xen {
    XenApp::XenApp(const Size<u32>& windowSize, const str& windowTitle, u32 windowIcon) {
        Window = MakeUnique<AppWindow>(windowSize, DEFAULT_CLASSNAME, windowTitle, windowIcon);

        if (RawPtr(Window) == nullptr) {
            throw std::runtime_error("Failed to initialize application window!");
        }

        // Pass our app instance to the RenderTree
        // This is necessary to allow our RenderTree to rebuild the UI
        // when the window state changes
        RenderTree::Init(this);
        Renderer::SetOwner(this);
    }

    XenApp::~XenApp() = default;

    void XenApp::RunApp() {
        // Initialize our native Win32 window
        Window->Init();

        // Tell the renderer about our window and initialize it
        Renderer::Init();

        // Call our BuildUI method
        RenderTree::SetRoot(BuildUI());

        // Run the main window loop
        Window->Run();

        // Shutdown the app and cleanup
        Shutdown();
    }

    void XenApp::Shutdown() {
        Renderer::Shutdown();
        Window->Shutdown();
        Window->Quit();
    }

    void XenApp::AttachRootElement(Element* root) {
        // Set the root element of our RenderTree.
        // This operates very similarily to the DOM in browsers and
        // can be thought of as a more complex linked list.
        RenderTree::SetRoot(root);
    }

    void XenApp::ResetElements() { Allocator.Reset(); }
}  // namespace Xen