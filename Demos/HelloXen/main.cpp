#include "XenUI.h"
#include "resource.h"

#include <iostream>

using namespace Xen;

int main() {
    if (const auto hr = CoInitialize(nullptr); SUCCEEDED(hr)) {
        AppWindow app(Size<u32>(600, 400), "HelloXenClass", "HelloXen", APPICON);
        app.Run();
        CoUninitialize();
    }
    return 0;
}

// int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
//     Xen::AppWindow app(Size<u32>(800, 600), "HelloXenClass", "HelloXen");
//     app.Run();
//     return 0;
// }
