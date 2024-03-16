//
// Created by conta on 3/15/2024.
//

#pragma once

#include "XenAPI.h"
#include "Types.h"
#include "Platform.h"
#include "Color.h"
#include "Offset.h"
#include "Rect.h"

namespace Xen {
    class XEN_API XenText {
    public:
        XenText(wstr value,
                const wstr& fontFamily,
                const Offset& position,
                const Rect& size,
                u32 fontWeight     = 400,
                f32 fontSize       = 12.f,
                const Color& color = Colors::White);
        XenText(const str& value,
                const str& fontFamily,
                const Offset& position,
                const Rect& size,
                u32 fontWeight     = 400,
                f32 fontSize       = 12.f,
                const Color& color = Colors::White);

        void Draw();
        void UpdateSize(const Rect& size);
        void SetText(const wstr& value);
        void SetText(const str& value);

    private:
        wstr Value;
        Color TextColor;
        Offset Position;
        Rect BoxSize;
        f32 FontSize;
        u32 Weight;
        IDWriteTextFormat* TextFormat   = nullptr;
        ID2D1SolidColorBrush* FillColor = nullptr;
    };
}  // namespace Xen