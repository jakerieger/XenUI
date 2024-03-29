// Copyright (C) 2024 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "ElementAllocator.h"
#include "RenderTree.h"
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

    // `create` is a macro replacement for `new (Allocator)`
    // See ElementAllocator for details on how this works
    return create Box(
      Rect::FromPoints({0, 0}, Window->GetDimensions().AsOffset()),
      Context.AppTheme.FrameBackground,
      0,
      create Text(
        "(Click the window background to quit)",
        Context.FontFamily,
        Window->GetWindowCenter(),
        Rect::FromPoints({0, 0}, Window->GetDimensions().AsOffset().Translate(0.f, -90.f)),
        0,
        create Text(
          "Hello Xen!",
          Context.FontFamily,
          Window->GetWindowCenter(),
          Rect::FromPoints({0, 0}, Window->GetDimensions().AsOffset().Translate(0.f, -140.f)),
          0,
          create Box(btnRect,
                     Context.AppTheme.Tertiary,
                     1,
                     create Text("Say Hello",
                                 Context.FontFamily,
                                 Window->GetWindowCenter(),
                                 btnRect,
                                 0,
                                 nullptr,
                                 600,
                                 16.f,
                                 Context.AppTheme.White),
                     true,
                     btnCallback),
          300,
          24.f,
          Context.AppTheme.TextHighlight),
        300,
        14.f,
        Context.AppTheme.TextHighlight),
      false,
      windowCallback);
}

void HelloXen::Shutdown() {}

int main() {
    HelloXen app(Size<u32>(600, 400), "HelloXen", APPICON);
    RUN_XEN_APP(app, "HelloXen")
    return 0;
}
