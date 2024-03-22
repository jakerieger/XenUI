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

// Author: Jake Rieger
// Created: 3/21/2024.
//

#pragma once

#include "Element.h"
#include "XenAPI.h"
#include "Platform.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#define NANOSVG_ALL_COLOR_KEYWORDS  // Include full list of color keywords.
#define NANOSVG_IMPLEMENTATION      // Expands implementation
#include <nanosvg.h>
#include <nanosvgrast.h>

namespace Xen {
    class XEN_API SVG final : public Element {
    public:
        SVG(i64 zIndex, const str& fileName, const Offset& position, Element* child);
        ~SVG() override;

        void Draw() override;

    private:
        void SVGToBitmap();
        void Update();
        Offset Position;
        NSVGimage* SvgData  = nullptr;
        ID2D1Bitmap* Bitmap = nullptr;
    };
}  // namespace Xen
