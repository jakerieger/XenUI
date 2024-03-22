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
#include "Helpers.inl"
#include "XenAPI.h"
#include "Platform.h"

namespace Xen {
    class XEN_API Color {
    public:
        Color();
        Color(f32 r, f32 g, f32 b, f32 a = 1.0);
        explicit Color(u32 color);

        [[nodiscard]] u32 Value() const;
        [[nodiscard]] f32 Luminance() const;
        [[nodiscard]] Color WithAlpha(u32 a) const;
        [[nodiscard]] Color WithAlpha(f32 a) const;
        [[nodiscard]] Color WithRed(u32 r) const;
        [[nodiscard]] Color WithRed(f32 r) const;
        [[nodiscard]] Color WithGreen(u32 g) const;
        [[nodiscard]] Color WithGreen(f32 g) const;
        [[nodiscard]] Color WithBlue(u32 b) const;
        [[nodiscard]] Color WithBlue(f32 b) const;
        [[nodiscard]] Color Greyscale() const;

        static Color AlphaBlend(const Color& foreground, const Color& background);
        static u32 GetAlphaFromOpacity(f32 opacity);
        static Color Lerp(const Color& a, const Color& b, f32 t);
        static f32 LinearizeComponent(f32 c);

        D2D1_COLOR_F& GetD2DColor() { return InternalColor; }

        [[nodiscard]] f32 Red() const { return InternalColor.r; }
        [[nodiscard]] f32 Green() const { return InternalColor.g; }
        [[nodiscard]] f32 Blue() const { return InternalColor.g; }
        [[nodiscard]] f32 Alpha() const { return InternalColor.a; }

    private:
        D2D1_COLOR_F InternalColor = {};
    };

    namespace Colors {
        inline static auto White       = Color(0xFFFFFFFF);
        inline static auto Black       = Color(0xFF000000);
        inline static auto Red         = Color(0xFFFF0000);
        inline static auto Yellow      = Color(0xFFFFFF00);
        inline static auto Green       = Color(0xFF00FF00);
        inline static auto Cyan        = Color(0xFF00FFFF);
        inline static auto Blue        = Color(0xFF0000FF);
        inline static auto Magenta     = Color(0xFFFF00FF);
        inline static auto Transparent = Color(0x00000000);
    }  // namespace Colors
}  // namespace Xen