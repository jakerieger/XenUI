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

#include "XenAPI.h"
#include "Types.h"
#include "Platform.h"
#include "Color.h"
#include "Element.h"
#include "Offset.h"
#include "Rect.h"

namespace Xen {
    class XEN_API Text final : public Element {
    public:
        Text(wstr value,
             const wstr& fontFamily,
             const Offset& position,
             const Rect& size,
             i64 zIndex,
             Element* child     = nullptr,
             u32 fontWeight     = 400,
             f32 fontSize       = 12.f,
             const Color& color = Colors::White);

        Text(const str& value,
             const str& fontFamily,
             const Offset& position,
             const Rect& size,
             i64 zIndex,
             Element* child     = nullptr,
             u32 fontWeight     = 400,
             f32 fontSize       = 12.f,
             const Color& color = Colors::White);

        void Draw() override;
        void SetText(const wstr& value);
        void SetText(const str& value);

    private:
        wstr Value;
        Color TextColor;
        Offset Position;
        f32 FontSize;
        u32 Weight;
        IDWriteTextFormat* TextFormat   = nullptr;
        ID2D1SolidColorBrush* FillColor = nullptr;
    };
}  // namespace Xen