//
// Created by conta on 3/15/2024.
//

#pragma once

#include "XenAPI.h"
#include "Types.h"
#include "Platform.h"
#include "Color.h"
#include "Offset.h"

namespace Xen {
    class XEN_API XenText {
    public:
        XenText(wstr value,
                const wstr& fontFamily,
                const Offset<f32>& position,
                u32 fontWeight     = 400,
                f32 size           = 12.f,
                const Color& color = Colors::White);
        XenText(const str& value,
                const str& fontFamily,
                const Offset<f32>& position,
                u32 fontWeight     = 400,
                f32 size           = 12.f,
                const Color& color = Colors::White);

        void Draw() const;
        void SetText(const wstr& value);
        void SetText(const str& value);

    private:
        wstr Value;
        Color TextColor;
        Offset<f32> Position;
        u32 Weight;
        IDWriteTextFormat* TextFormat   = nullptr;
        ID2D1SolidColorBrush* FillColor = nullptr;
    };
}  // namespace Xen