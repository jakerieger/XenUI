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

#include "Types.h"
#include "Platform.h"
#include "XenAPI.h"
#include "UI/Offset.h"

namespace Xen {
    class XEN_API XenApp;
}

namespace Xen::Renderer {
    void Init();
    int Render();
    void Shutdown();
    void OnResize(u32 width, u32 height);
    void CheckHit(const Offset& mousePos);
    void CheckOverlap(const Offset& mousePos);
    void SetOwner(XenApp* owner);
    Offset GetRenderTargetCenter();
    u64 GetTicks();

    ID2D1Factory* GetD2DFactory();
    IDWriteFactory* GetDWFactory();
    ID2D1HwndRenderTarget* GetRenderTarget();
    XenApp* GetOwner();
}  // namespace Xen::Renderer