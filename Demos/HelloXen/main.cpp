#include "XenUI.h"
#include "resource.h"

#include <iostream>

using namespace Xen;

class HelloXen final : public XenApp {
public:
    HelloXen(const Size<u32>& windowSize, const str& windowTitle, u32 windowIcon = 0);
    void BuildUI() override;
    void Shutdown() override;

private:
    Box* btnBox   = nullptr;
    Text* btnText = nullptr;
};

HelloXen::HelloXen(const Size<u32>& windowSize, const str& windowTitle, const u32 windowIcon)
    : XenApp(windowSize, windowTitle, windowIcon) {}

void HelloXen::BuildUI() {
    // Reset UI elements
    XenApp::BuildUI();

    const auto btnRect = Rect::FromCenter(Window->GetWindowCenter(), 200, 48);
    btnText            = new Text("Quit",
                       Context.FontFamily,
                       Window->GetWindowCenter(),
                       btnRect,
                       0,
                       nullptr,
                       600,
                       16.f,
                       Context.AppTheme.White);
    btnBox             = new Box(btnRect, Context.AppTheme.Primary, 0, btnText);

    AttachRootElement(btnBox);
}

void HelloXen::Shutdown() {}

int main() {
    if (const auto hr = CoInitialize(nullptr); SUCCEEDED(hr)) {
        HelloXen app(Size<u32>(600, 400), "HelloXen", APPICON);
        app.RunApp();
        CoUninitialize();
    }
    return 0;
}

// int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
//     Xen::AppWindow app(Size<u32>(800, 600), "HelloXenClass", "HelloXen");
//     app.Run();
//     return 0;
// }
