#include "XenUI.h"

#include <iostream>

using Xen::Size;

int main() {
    Xen::AppWindow app(Size(800, 600).To<u32>(), "HelloXenClass", "HelloXen");
    app.Run();
    return 0;
}
