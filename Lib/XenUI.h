//
// Created by Jake Rieger on 3/13/2024.
//

#pragma once

#include "AppWindow.h"
#include "Types.h"
#include "Helpers.inl"
#include "XenApp.h"

#include "UI/Offset.h"
#include "UI/Size.h"
#include "UI/Offset.h"
#include "UI/Color.h"
#include "UI/Rect.h"
#include "UI/Box.h"
#include "UI/Text.h"

#define RUN_XEN_APP(app)                                                                           \
    if (const auto hr = CoInitialize(nullptr); SUCCEEDED(hr)) {                                    \
        app.RunApp();                                                                              \
        CoUninitialize();                                                                          \
    } else {                                                                                       \
        ::MessageBoxA(nullptr,                                                                     \
                      "Failed to initialize COM",                                                  \
                      "Windows API Error",                                                         \
                      MB_ICONERROR | MB_OK);                                                       \
        return 1;                                                                                  \
    }

// new operator overload
// ReSharper disable once CppParameterMayBeConst
XEN_API inline void* operator new(size_t _Size, Xen::ElementAllocator& allocator) {
    return allocator.Allocate(_Size);
}