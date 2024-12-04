// Author: Jake Rieger
// Created: 12/3/24.
//

#pragma once

#if defined(_WS_WINDOWS)
    #if defined(BUILDING_XENUI)  // When building the library
        #define XEN_API __declspec(dllexport)
    #else                        // When using the library
        #define XEN_API __declspec(dllimport)
    #endif
#elif defined(_WS_X11) || defined(_WS_WAYLAND) || defined(_WS_APPLE)
    #if defined(BUILDING_XENUI)  // When building the library
        #define XEN_API __attribute__((visibility("default")))
    #else                        // When using the library
        #define XEN_API
    #endif
#else
    #error "Unknown platform"
#endif