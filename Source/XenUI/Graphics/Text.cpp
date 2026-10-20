// Copyright 2026 Jake Rieger
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "XenUI/Graphics/Text.hpp"
#include "XenUI/AppContext.hpp"
#include "XenUI/Graphics/TextLayoutData.hpp"
#include "XenUI/Platform/HResult.hpp"
#include "XenUI/Platform/Utf8.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace XenUI {
    namespace {
        constexpr float kLayoutLimit = 1.0e6f;

        DWRITE_TEXT_ALIGNMENT ToDWrite(TextAlignment Alignment) {
            switch (Alignment) {
                case TextAlignment::Center:
                    return DWRITE_TEXT_ALIGNMENT_CENTER;
                case TextAlignment::Trailing:
                    return DWRITE_TEXT_ALIGNMENT_TRAILING;
                case TextAlignment::Justified:
                    return DWRITE_TEXT_ALIGNMENT_JUSTIFIED;
                default:
                    return DWRITE_TEXT_ALIGNMENT_LEADING;
            }
        }

        DWRITE_PARAGRAPH_ALIGNMENT ToDWrite(ParagraphAlignment Alignment) {
            switch (Alignment) {
                case ParagraphAlignment::Center:
                    return DWRITE_PARAGRAPH_ALIGNMENT_CENTER;
                case ParagraphAlignment::Bottom:
                    return DWRITE_PARAGRAPH_ALIGNMENT_FAR;
                default:
                    return DWRITE_PARAGRAPH_ALIGNMENT_NEAR;
            }
        }
    }  // namespace

    TextLayout::TextLayout() = default;

    TextLayout::TextLayout(std::string_view Text,
                           const Font& TextFont,
                           const TextOptions& Options,
                           const Size& MaxSize) {
        GraphicsDevice& Device    = AppContext::Get().GetGraphicsDevice();
        IDWriteFactory* Factory   = Device.GetDWriteFactory();
        IDWriteTextFormat* Format = Device.GetTextFormat(TextFont);
        const std::wstring Wide   = Widen(Text);

        const bool UnboundedWidth  = !std::isfinite(MaxSize.Width);
        const bool UnboundedHeight = !std::isfinite(MaxSize.Height);

        auto Data = std::make_shared<TextLayoutData>();
        Data->Utf8Offsets.reserve(Wide.size() + 1);
        for (size_t Byte = 0; Byte < Text.size();) {
            const auto Lead  = static_cast<unsigned char>(Text[Byte]);
            const size_t Run = Lead < 0x80           ? 1
                               : (Lead >> 5) == 0x6  ? 2
                               : (Lead >> 4) == 0xE  ? 3
                               : (Lead >> 3) == 0x1E ? 4
                                                     : 1;
            Data->Utf8Offsets.push_back(static_cast<uint32_t>(Byte));
            if (Run == 4) { Data->Utf8Offsets.push_back(static_cast<uint32_t>(Byte)); }
            Byte = std::min(Byte + Run, Text.size());
        }
        Data->Utf8Offsets.resize(Wide.size(), static_cast<uint32_t>(Text.size()));
        Data->Utf8Offsets.push_back(static_cast<uint32_t>(Text.size()));
        ThrowIfFailed(Factory->CreateTextLayout(Wide.c_str(),
                                                static_cast<UINT32>(Wide.size()),
                                                Format,
                                                UnboundedWidth ? kLayoutLimit : std::max(MaxSize.Width, 0.0f),
                                                UnboundedHeight ? kLayoutLimit : std::max(MaxSize.Height, 0.0f),
                                                &Data->Layout),
                      "IDWriteFactory::CreateTextLayout");

        IDWriteTextLayout& Layout = *Data->Layout.Get();
        Layout.SetTextAlignment(ToDWrite(Options.Alignment));
        Layout.SetParagraphAlignment(ToDWrite(Options.VerticalAlignment));
        Layout.SetWordWrapping(Options.Wrapping == TextWrapping::Wrap ? DWRITE_WORD_WRAPPING_WRAP
                                                                      : DWRITE_WORD_WRAPPING_NO_WRAP);

        if (Options.Trimming == TextTrimming::Ellipsis) {
            Microsoft::WRL::ComPtr<IDWriteInlineObject> EllipsisSign;
            ThrowIfFailed(Factory->CreateEllipsisTrimmingSign(&Layout, &EllipsisSign),
                          "IDWriteFactory::CreateEllipsisTrimmingSign");
            const DWRITE_TRIMMING Trimming {DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
            Layout.SetTrimming(&Trimming, EllipsisSign.Get());
        }

        DWRITE_TEXT_METRICS Metrics {};
        ThrowIfFailed(Layout.GetMetrics(&Metrics), "IDWriteTextLayout::GetMetrics");
        if (UnboundedWidth || UnboundedHeight) {
            if (UnboundedWidth) { Layout.SetMaxWidth(Metrics.widthIncludingTrailingWhitespace); }
            if (UnboundedHeight) { Layout.SetMaxHeight(Metrics.height); }
            ThrowIfFailed(Layout.GetMetrics(&Metrics), "IDWriteTextLayout::GetMetrics");
        }

        Data->BoxSize      = {Layout.GetMaxWidth(), Layout.GetMaxHeight()};
        Data->MeasuredSize = {Metrics.widthIncludingTrailingWhitespace, Metrics.height};

        DWRITE_OVERHANG_METRICS Overhang {};
        ThrowIfFailed(Layout.GetOverhangMetrics(&Overhang), "IDWriteTextLayout::GetOverhangMetrics");
        Data->InkBounds = Rect::FromEdges(std::min(0.0f, -Overhang.left),
                                          std::min(0.0f, -Overhang.top),
                                          std::max(Data->BoxSize.Width, Data->BoxSize.Width + Overhang.right),
                                          std::max(Data->BoxSize.Height, Data->BoxSize.Height + Overhang.bottom));

        UINT32 LineCount = 0;
        Layout.GetLineMetrics(nullptr, 0, &LineCount);
        if (LineCount > 0) {
            std::vector<DWRITE_LINE_METRICS> Lines(LineCount);
            ThrowIfFailed(Layout.GetLineMetrics(Lines.data(), LineCount, &LineCount),
                          "IDWriteTextLayout::GetLineMetrics");
            Data->Baseline = Lines.front().baseline;
        }
        Data->LineCount = LineCount;

        _Data = std::move(Data);
    }

    uint32_t TextLayoutData::ToUtf16(size_t Utf8Offset) const {
        const auto Found = std::lower_bound(Utf8Offsets.begin(), Utf8Offsets.end(), static_cast<uint32_t>(Utf8Offset));
        return static_cast<uint32_t>(
          std::min<ptrdiff_t>(Found - Utf8Offsets.begin(), static_cast<ptrdiff_t>(Utf8Offsets.size()) - 1));
    }

    size_t TextLayoutData::ToUtf8(uint32_t Utf16Offset) const {
        return Utf8Offsets.empty() ? 0 : Utf8Offsets[std::min<size_t>(Utf16Offset, Utf8Offsets.size() - 1)];
    }

    size_t TextLayout::HitTest(const Vec2& Point) const {
        if (!_Data) { return 0; }
        BOOL Trailing = FALSE;
        BOOL Inside   = FALSE;
        DWRITE_HIT_TEST_METRICS Metrics {};
        if (FAILED(_Data->Layout->HitTestPoint(Point.X, Point.Y, &Trailing, &Inside, &Metrics))) { return 0; }
        return _Data->ToUtf8(Metrics.textPosition + (Trailing != FALSE ? Metrics.length : 0));
    }

    Rect TextLayout::GetCaretBounds(size_t Offset) const {
        if (!_Data) { return {}; }
        FLOAT X = 0.0f;
        FLOAT Y = 0.0f;
        DWRITE_HIT_TEST_METRICS Metrics {};
        if (FAILED(_Data->Layout->HitTestTextPosition(_Data->ToUtf16(Offset), FALSE, &X, &Y, &Metrics))) { return {}; }
        return {X, Metrics.top, 0.0f, Metrics.height};
    }

    std::vector<Rect> TextLayout::GetRangeBounds(size_t Start, size_t End) const {
        std::vector<Rect> Result;
        if (!_Data || End <= Start) { return Result; }
        const uint32_t First  = _Data->ToUtf16(Start);
        const uint32_t Length = _Data->ToUtf16(End) - First;
        if (Length == 0) { return Result; }
        UINT32 Count = 0;
        _Data->Layout->HitTestTextRange(First, Length, 0.0f, 0.0f, nullptr, 0, &Count);
        std::vector<DWRITE_HIT_TEST_METRICS> Metrics(Count);
        if (Count == 0 ||
            FAILED(_Data->Layout->HitTestTextRange(First, Length, 0.0f, 0.0f, Metrics.data(), Count, &Count))) {
            return Result;
        }
        for (const DWRITE_HIT_TEST_METRICS& Entry : Metrics) {
            Result.push_back({Entry.left, Entry.top, Entry.width, Entry.height});
        }
        return Result;
    }

    std::vector<size_t> TextLayout::GetCaretStops() const {
        std::vector<size_t> Stops {0};
        if (!_Data) { return Stops; }
        UINT32 Count = 0;
        _Data->Layout->GetClusterMetrics(nullptr, 0, &Count);
        std::vector<DWRITE_CLUSTER_METRICS> Clusters(Count);
        if (Count > 0 && SUCCEEDED(_Data->Layout->GetClusterMetrics(Clusters.data(), Count, &Count))) {
            uint32_t Position = 0;
            for (const DWRITE_CLUSTER_METRICS& Cluster : Clusters) {
                Position += Cluster.length;
                Stops.push_back(_Data->ToUtf8(Position));
            }
        }
        if (Stops.back() != _Data->ToUtf8(static_cast<uint32_t>(_Data->Utf8Offsets.size() - 1))) {
            Stops.push_back(_Data->ToUtf8(static_cast<uint32_t>(_Data->Utf8Offsets.size() - 1)));
        }
        return Stops;
    }

    Size TextLayout::GetBoxSize() const {
        return _Data ? _Data->BoxSize : Size {};
    }

    Size TextLayout::GetMeasuredSize() const {
        return _Data ? _Data->MeasuredSize : Size {};
    }

    Rect TextLayout::GetInkBounds() const {
        return _Data ? _Data->InkBounds : Rect {};
    }

    float TextLayout::GetBaseline() const {
        return _Data ? _Data->Baseline : 0.0f;
    }

    uint32_t TextLayout::GetLineCount() const {
        return _Data ? _Data->LineCount : 0;
    }
}  // namespace XenUI
