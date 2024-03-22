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

#pragma once

#include "UI/Offset.h"
#include "UI/Size.h"
#include "UI/Offset.h"
#include "UI/Color.h"
#include "UI/Rect.h"
#include "UI/Box.h"
#include "UI/Text.h"
#include "UI/Element.h"
#include "UI/SVG.h"

#include "AppWindow.h"
#include "Types.h"
#include "Helpers.inl"
#include "XenApp.h"

#define RUN_XEN_APP(app, name)                                                                     \
    if (const auto hr = CoInitialize(nullptr); SUCCEEDED(hr)) {                                    \
        std::cout << "Running demo: '" << name << "'\n";                                           \
        app.RunApp();                                                                              \
        CoUninitialize();                                                                          \
    } else {                                                                                       \
        ::MessageBoxA(                                                                             \
          nullptr,                                                                                 \
          " Failed to initialize COM                                                   \
                 ",                                                                                \
          " Windows API Error                                                          \
                 ",                                                                                \
          MB_ICONERROR | MB_OK);                                                                   \
        return 1;                                                                                  \
    }

// new operator overload
// ReSharper disable once CppParameterMayBeConst
XEN_API inline void* operator new(size_t _Size, Xen::ElementAllocator& allocator) {
    return allocator.Allocate(_Size);
}