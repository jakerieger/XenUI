#include "XenUI.h"

#include <iostream>

using namespace Xen;

int main() {
    AppWindow app(Size<u32>(800, 600), "HelloXenClass", "HelloXen");
    app.Run();
    return 0;
}

// int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
//     Xen::AppWindow app(Size<u32>(800, 600), "HelloXenClass", "HelloXen");
//     app.Run();
//     return 0;
// }
