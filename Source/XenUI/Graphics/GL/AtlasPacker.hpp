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

#include <cstdint>
#include <optional>
#include <vector>

namespace XenUI {
    /// @brief Packs rectangles into a fixed-size texture on shelves, rows whose height is set by their first item.
    /// Items are never freed one by one; the owner resets the whole atlas when it fills up.
    class AtlasPacker {
    public:
        struct Slot {
            uint32_t X = 0;
            uint32_t Y = 0;
        };

        AtlasPacker(uint32_t Width, uint32_t Height) : _Width(Width), _Height(Height) {}

        [[nodiscard]] uint32_t GetWidth() const { return _Width; }
        [[nodiscard]] uint32_t GetHeight() const { return _Height; }

        /// @brief A free Width by Height area with Padding empty pixels after it on both axes, or nullopt when the
        /// atlas is full.
        std::optional<Slot> Allocate(uint32_t Width, uint32_t Height, uint32_t Padding = 1) {
            const uint32_t PaddedWidth  = Width + Padding;
            const uint32_t PaddedHeight = Height + Padding;
            if (PaddedWidth > _Width || PaddedHeight > _Height) { return std::nullopt; }

            for (Shelf& Row : _Shelves) {
                // Reuse a shelf only if the item fills most of its height, so short glyphs don't waste tall rows.
                if (PaddedHeight <= Row.Height && PaddedHeight * 2 > Row.Height && Row.Used + PaddedWidth <= _Width) {
                    const Slot Result {Row.Used, Row.Y};
                    Row.Used += PaddedWidth;
                    return Result;
                }
            }
            if (_NextY + PaddedHeight > _Height) { return std::nullopt; }

            _Shelves.push_back({_NextY, PaddedHeight, PaddedWidth});
            const Slot Result {0, _NextY};
            _NextY += PaddedHeight;
            return Result;
        }

        void Reset() {
            _Shelves.clear();
            _NextY = 0;
        }

    private:
        struct Shelf {
            uint32_t Y      = 0;
            uint32_t Height = 0;
            uint32_t Used   = 0;
        };

        uint32_t _Width;
        uint32_t _Height;
        uint32_t _NextY = 0;
        std::vector<Shelf> _Shelves;
    };
}  // namespace XenUI
