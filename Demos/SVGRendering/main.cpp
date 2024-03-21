//
// Created by conta on 3/21/2024.
//

#include "XenUI.h"
#include "resource.h"
#include "UI/SVG.h"

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
    return create SVG(0, "XenLogo.svg", Window->GetWindowCenter(), nullptr);  //
}

void SVGRendering::Shutdown() {}

int main() {
    SVGRendering app(Size<u32>(600, 300), "SVGRendering", APPICON);
    RUN_XEN_APP(app)
    return 0;
}