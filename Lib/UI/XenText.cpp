//
// Created by conta on 3/15/2024.
//

#include "XenText.h"

#include "Rect.h"
#include "XenRenderer.h"

namespace Xen {
    XenText::XenText(wstr value,
                     const wstr& fontFamily,
                     const Offset& position,
                     const Rect& size,
                     const u32 fontWeight,
                     const f32 fontSize,
                     const Color& color)
        : Value(std::move(value)), TextColor(color), Position(position), BoxSize(size),
          FontSize(fontSize), Weight(fontWeight) {
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

    XenText::XenText(const str& value,
                     const str& fontFamily,
                     const Offset& position,
                     const Rect& size,
                     const u32 fontWeight,
                     const f32 fontSize,
                     const Color& color)
        : TextColor(color), Position(position), BoxSize(size), FontSize(fontSize),
          Weight(fontWeight) {
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

    void XenText::Draw() {
        Renderer::GetRenderTarget()->DrawText(Value.c_str(),
                                              wcslen(Value.c_str()),
                                              TextFormat,
                                              BoxSize.GetD2DRect(),
                                              FillColor);
    }

    void XenText::UpdateSize(const Rect& size) { BoxSize = size; }

    void XenText::SetText(const wstr& value) {}

    void XenText::SetText(const str& value) {}

}  // namespace Xen