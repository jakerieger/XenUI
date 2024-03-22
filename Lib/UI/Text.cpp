// Author: Jake Rieger
// Created: 3/21/2024.
//

#include "Text.h"

#include "Rect.h"
#include "Renderer.h"

namespace Xen {
    Text::Text(wstr value,
               const wstr& fontFamily,
               const Offset& position,
               const Rect& size,
               const i64 zIndex,
               Element* child,
               const u32 fontWeight,
               const f32 fontSize,
               const Color& color)
        : Element(zIndex, size), Value(std::move(value)), TextColor(color), Position(position),
          FontSize(fontSize), Weight(fontWeight) {
        this->Children.push_back(child);

        auto hr =
          Renderer::GetDWFactory()->CreateTextFormat(fontFamily.c_str(),
                                                     nullptr,
                                                     static_cast<DWRITE_FONT_WEIGHT>(Weight),
                                                     DWRITE_FONT_STYLE_NORMAL,
                                                     DWRITE_FONT_STRETCH_NORMAL,
                                                     fontSize,
                                                     L"en-us",
                                                     &TextFormat);
        if (FAILED(hr)) {
            throw std::runtime_error("Failed to create IDWriteTextFormat!");
        }

        hr = TextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        hr = TextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

        hr =
          Renderer::GetRenderTarget()->CreateSolidColorBrush(TextColor.GetD2DColor(), &FillColor);

        if (FAILED(hr)) {
            throw std::runtime_error("Failed to create SolidColorBrush!");
        }
    }

    Text::Text(const str& value,
               const str& fontFamily,
               const Offset& position,
               const Rect& size,
               const i64 zIndex,
               Element* child,
               const u32 fontWeight,
               const f32 fontSize,
               const Color& color)
        : Element(zIndex, size), TextColor(color), Position(position), FontSize(fontSize),
          Weight(fontWeight) {
        this->Children.push_back(child);
        Converters::ANSIToWide(value, Value);

        wstr fontFamilyW;
        Converters::ANSIToWide(fontFamily, fontFamilyW);

        auto hr =
          Renderer::GetDWFactory()->CreateTextFormat(fontFamilyW.c_str(),
                                                     nullptr,
                                                     static_cast<DWRITE_FONT_WEIGHT>(Weight),
                                                     DWRITE_FONT_STYLE_NORMAL,
                                                     DWRITE_FONT_STRETCH_NORMAL,
                                                     fontSize,
                                                     L"en-us",
                                                     &TextFormat);
        if (FAILED(hr)) {
            throw std::runtime_error("Failed to create IDWriteTextFormat!");
        }

        hr = TextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        hr = TextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

        hr =
          Renderer::GetRenderTarget()->CreateSolidColorBrush(TextColor.GetD2DColor(), &FillColor);

        if (FAILED(hr)) {
            throw std::runtime_error("Failed to create SolidColorBrush!");
        }
    }

    void Text::Draw() {
        Renderer::GetRenderTarget()->DrawText(Value.c_str(),
                                              wcslen(Value.c_str()),
                                              TextFormat,
                                              Size.GetD2DRect(),
                                              FillColor);
    }

    void Text::SetText(const wstr& value) {}

    void Text::SetText(const str& value) {}

}  // namespace Xen