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