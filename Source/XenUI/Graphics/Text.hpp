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

#pragma once

#include "XenUI/Api.hpp"
#include "XenUI/Core/Math.hpp"

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace XenUI {
    class TextLayoutData;

    enum class FontWeight : uint16_t {
        Thin       = 100,
        ExtraLight = 200,
        Light      = 300,
        Regular    = 400,
        Medium     = 500,
        SemiBold   = 600,
        Bold       = 700,
        ExtraBold  = 800,
        Black      = 900,
    };

    enum class FontStyle : uint8_t { Normal, Italic, Oblique };

    struct Font {
        std::string Family = "Segoe UI";
        /// @brief Em size in device-independent pixels.
        float Size        = 14.0f;
        FontWeight Weight = FontWeight::Regular;
        FontStyle Style   = FontStyle::Normal;

        bool operator==(const Font&) const = default;
    };

    enum class TextAlignment : uint8_t { Leading, Center, Trailing, Justified };
    enum class ParagraphAlignment : uint8_t { Top, Center, Bottom };
    enum class TextWrapping : uint8_t { NoWrap, Wrap };
    enum class TextTrimming : uint8_t { None, Ellipsis };

    struct TextOptions {
        TextAlignment Alignment              = TextAlignment::Leading;
        ParagraphAlignment VerticalAlignment = ParagraphAlignment::Top;
        TextWrapping Wrapping                = TextWrapping::NoWrap;
        TextTrimming Trimming                = TextTrimming::None;

        bool operator==(const TextOptions&) const = default;
    };

    /// @brief Immutable, shaped block of UTF-8 text. Cheap to copy; copies share the same layout.
    class XENUI_API TextLayout {
    public:
        TextLayout();

        /// @param MaxSize Layout box. An unbounded axis shrinks to fit the text. Requires an App to exist.
        TextLayout(std::string_view Text,
                   const Font& TextFont,
                   const TextOptions& Options = {},
                   const Size& MaxSize        = {kUnbounded, kUnbounded});

        [[nodiscard]] bool IsEmpty() const { return _Data == nullptr; }

        /// @brief Size of the layout box that alignment is relative to.
        [[nodiscard]] Size GetBoxSize() const;

        /// @brief Size actually occupied by the laid-out lines.
        [[nodiscard]] Size GetMeasuredSize() const;

        /// @brief Area that may receive ink, relative to the draw origin. Includes glyph overhangs.
        [[nodiscard]] Rect GetInkBounds() const;

        /// @brief Distance from the top of the box to the first line's baseline.
        [[nodiscard]] float GetBaseline() const;

        [[nodiscard]] uint32_t GetLineCount() const;

        /// @brief The caret position nearest to Point, as a UTF-8 byte offset into the text.
        [[nodiscard]] size_t HitTest(const Vec2& Point) const;

        /// @brief A zero-width rectangle spanning the line at a UTF-8 caret offset.
        [[nodiscard]] Rect GetCaretBounds(size_t Offset) const;

        /// @brief One rectangle per line fragment covered by the UTF-8 range [Start, End).
        [[nodiscard]] std::vector<Rect> GetRangeBounds(size_t Start, size_t End) const;

        /// @brief UTF-8 offsets where a caret may stand: cluster boundaries, so a caret never splits a
        /// character, a combining sequence or an emoji. Includes 0 and the text length.
        [[nodiscard]] std::vector<size_t> GetCaretStops() const;

        /// @internal
        [[nodiscard]] const TextLayoutData* GetData() const { return _Data.get(); }

    private:
        std::shared_ptr<const TextLayoutData> _Data;
    };
}  // namespace XenUI
