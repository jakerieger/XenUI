// Author: Jake Rieger
// Created: 12/3/24.
//

#pragma once

#if defined(_WS_X11)
    #include "Platform/X11Window.hpp"
using WindowType = Xen::X11Window;
#elif defined(_WS_WAYLAND)
    #include <Platform/WaylandWindow.h>
using WindowType = Xen::WaylandWindow;
#elif defined(_WS_WINDOWS)
    #include <Platform/Win32Window.h>
using WindowType = Xen::Win32Window;
#elif defined(_WS_APPLE)
    #include <Platform/NSWindow.h>
using WindowType = Xen::NSWindow;
#else
    #error "No windowing system defined for this platform."
#endif