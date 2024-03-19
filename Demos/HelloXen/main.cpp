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
    Box* appBackground = nullptr;
    Text* helloText    = nullptr;
    Text* hintText     = nullptr;
    Box* btnBox        = nullptr;
    Text* btnText      = nullptr;
};

HelloXen::HelloXen(const Size<u32>& windowSize, const str& windowTitle, const u32 windowIcon)
    : XenApp(windowSize, windowTitle, windowIcon) {}

void HelloXen::BuildUI() {
    // Reset UI elements
    // NOTE: Should always be called and always at the top of BuildUI()
    XenApp::BuildUI();

    const auto btnRect = Rect::FromCenter(Window->GetWindowCenter().Translate(0.f, 20.f), 200, 48);
    btnText            = new Text("Say Hello",
                       Context.FontFamily,
                       Window->GetWindowCenter(),
                       btnRect,
                       0,
                       nullptr,
                       600,
                       16.f,
                       Context.AppTheme.White);
    btnBox             = new Box(btnRect, Context.AppTheme.Tertiary, 1, btnText, [&]() {
        ::MessageBoxA(Window->GetHandle(), "Hello!", "HelloXen", MB_OK | MB_ICONINFORMATION);
    });
    helloText =
      new Text("Hello Xen!",
               Context.FontFamily,
               Window->GetWindowCenter(),
               Rect::FromPoints({0, 0}, Window->GetDimensions().AsOffset().Translate(0.f, -140.f)),
               0,
               btnBox,
               300,
               24.f,
               Context.AppTheme.TextHighlight);
    hintText =
      new Text("(Click the window background to quit)",
               Context.FontFamily,
               Window->GetWindowCenter(),
               Rect::FromPoints({0, 0}, Window->GetDimensions().AsOffset().Translate(0.f, -90.f)),
               0,
               helloText,
               300,
               14.f,
               Context.AppTheme.TextHighlight);
    appBackground = new Box(Rect::FromPoints({0, 0}, Window->GetDimensions().AsOffset()),
                            Context.AppTheme.FrameBackground,
                            0,
                            hintText,
                            [] { ::PostQuitMessage(0); });

    AttachRootElement(appBackground);
}

void HelloXen::Shutdown() {}

int main() {
    HelloXen app(Size<u32>(600, 400), "HelloXen", APPICON);
    RUN_XEN_APP(app)
    return 0;
}
