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

#include "XenUI.h"
#include "resource.h"

using namespace Xen;

class SVGRendering final : public XenApp {
public:
    SVGRendering(const Size<u32>& windowSize, const str& windowTitle, u32 windowIcon = 0);
    Element* BuildUI() override;
    void Shutdown() override;
};

SVGRendering::SVGRendering(const Size<u32>& windowSize,
                           const str& windowTitle,
                           const u32 windowIcon)
    : XenApp(windowSize, windowTitle, windowIcon) {
    Context.AppTheme.WindowBackground = Color(0xFFF1F1F1);
}

Element* SVGRendering::BuildUI() {
    return create Box(Rect({0, 0}, Window->GetDimensions().To<f32>()),
                      Context.AppTheme.FrameBackground,
                      0,
                      create SVG(1, "XenLogo.svg", Window->GetWindowCenter(), nullptr));  //
}

void SVGRendering::Shutdown() {}

int main() {
    SVGRendering app(Size<u32>(600, 300), "SVGRendering", APPICON);
    RUN_XEN_APP(app, "SVGRendering")
    return 0;
}