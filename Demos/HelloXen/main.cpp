// ReSharper disable CppDFAMemoryLeak
#include "XenUI.h"
#include "resource.h"

#include <iostream>

using namespace Xen;

class HelloXen final : public XenApp {
public:
    HelloXen(const Size<u32>& windowSize, const str& windowTitle, u32 windowIcon = 0);
    Element* BuildUI() override;
    void Shutdown() override;
};

HelloXen::HelloXen(const Size<u32>& windowSize, const str& windowTitle, const u32 windowIcon)
    : XenApp(windowSize, windowTitle, windowIcon) {}

Element* HelloXen::BuildUI() {
    const auto btnRect = Rect::FromCenter(Window->GetWindowCenter().Translate(0.f, 20.f), 200, 48);
    auto btnCallback   = [&]() {
        ::MessageBoxA(Window->GetHandle(), "Hello!", "HelloXen", MB_OK | MB_ICONINFORMATION);
    };
    auto windowCallback = [&] { Quit(); };

    // Your compiler might warn you that this leaks memory,
    // but all of these class instances get cleaned up and deleted
    // before each rebuild of the UI. See the RenderTree namespace
    // for impl details
    return new Box(
      Rect::FromPoints({0, 0}, Window->GetDimensions().AsOffset()),
      Context.AppTheme.FrameBackground,
      0,
      new Text("(Click the window background to quit)",
               Context.FontFamily,
               Window->GetWindowCenter(),
               Rect::FromPoints({0, 0}, Window->GetDimensions().AsOffset().Translate(0.f, -90.f)),
               0,
               new Text("Hello Xen!",
                        Context.FontFamily,
                        Window->GetWindowCenter(),
                        Rect::FromPoints({0, 0},
                                         Window->GetDimensions().AsOffset().Translate(0.f, -140.f)),
                        0,
                        new Box(btnRect,
                                Context.AppTheme.Tertiary,
                                1,
                                new Text("Say Hello",
                                         Context.FontFamily,
                                         Window->GetWindowCenter(),
                                         btnRect,
                                         0,
                                         nullptr,
                                         600,
                                         16.f,
                                         Context.AppTheme.White),
                                btnCallback),
                        300,
                        24.f,
                        Context.AppTheme.TextHighlight),
               300,
               14.f,
               Context.AppTheme.TextHighlight),
      windowCallback);
}

void HelloXen::Shutdown() {}

int main() {
    HelloXen app(Size<u32>(600, 400), "HelloXen", APPICON);
    RUN_XEN_APP(app)
    return 0;
}
