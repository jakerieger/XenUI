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

#include "XenUI/Core/Math.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace XenUI {
    class FontFace;
    class FontSet;

    /// @brief Text shaped with HarfBuzz and broken into lines. Everything is in device-independent pixels relative to
    /// the layout origin, with paragraph and line alignment already applied.
    class TextLayoutData {
    public:
        struct Glyph {
            /// @brief Index into Faces.
            uint32_t Face  = 0;
            uint32_t Index = 0;
            /// @brief Pen position on the baseline.
            Vec2 Origin;
        };

        /// @brief Where a caret may stand on a line: a grapheme boundary and its x position.
        struct CaretStop {
            size_t Offset = 0;
            float X       = 0.0f;
        };

        struct Line {
            /// @brief UTF-8 range of the line's text, excluding a hard line break that ends it.
            size_t Start = 0;
            size_t End   = 0;
            /// @brief Where the next line starts: past the hard line break, or equal to End for a wrapped line.
            size_t Next  = 0;
            float Top      = 0.0f;
            float Height   = 0.0f;
            float Baseline = 0.0f;
            /// @brief Grapheme boundaries from Start up to End, in text order.
            std::vector<CaretStop> Stops;
        };

        std::shared_ptr<FontSet> Fonts;
        float EmSize = 0.0f;
        std::vector<std::shared_ptr<FontFace>> Faces;
        std::vector<Glyph> Glyphs;
        std::vector<Line> Lines;
        /// @brief Grapheme boundaries of the whole text, including 0 and its length.
        std::vector<size_t> Clusters;

        Size BoxSize;
        Size MeasuredSize;
        Rect InkBounds;
        float Baseline     = 0.0f;
        uint32_t LineCount = 0;

        /// @brief The line a caret at Offset sits on.
        [[nodiscard]] const Line& LineAt(size_t Offset) const;
        /// @brief X of a caret at Offset on Entry, which must contain it.
        [[nodiscard]] static float CaretX(const Line& Entry, size_t Offset);
    };
}  // namespace XenUI
