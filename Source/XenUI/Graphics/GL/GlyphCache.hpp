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

#include "XenUI/Graphics/GL/AtlasPacker.hpp"
#include "XenUI/Graphics/GL/GLObjects.hpp"

#include <cstdint>
#include <unordered_map>

struct FT_LibraryRec_;
struct FT_FaceRec_;

namespace XenUI {
    class FontFace;

    /// @brief Glyphs rasterized with FreeType into one BGRA atlas texture. Outline glyphs are stored as premultiplied
    /// white coverage, so the shader tints them with the brush; colour glyphs (emoji) keep their own colours.
    class GlyphCache {
    public:
        /// @brief Glyph positions are rounded to this fraction of a pixel horizontally.
        static constexpr int kSubpixelSteps = 4;

        struct Entry {
            /// @brief Offset of the bitmap's top-left corner from the pen position, in pixels, y down.
            int32_t Left   = 0;
            int32_t Top    = 0;
            uint32_t Width = 0;
            uint32_t Height = 0;
            uint32_t AtlasX = 0;
            uint32_t AtlasY = 0;
            bool Color      = false;

            [[nodiscard]] bool IsEmpty() const { return Width == 0 || Height == 0; }
        };

        GlyphCache();
        ~GlyphCache();

        GlyphCache(const GlyphCache&)            = delete;
        GlyphCache& operator=(const GlyphCache&) = delete;

        /// @brief Creates the atlas texture. Call with the context current.
        void Initialize(uint64_t Generation);

        /// @brief Glyph Index of Face at PixelSize, shifted right by Subpixel / kSubpixelSteps of a pixel.
        /// @return nullptr when the atlas is full; flush draws that sample it, call Reset and ask again.
        [[nodiscard]] const Entry* Get(const FontFace& Face, uint32_t Index, float PixelSize, int Subpixel);

        /// @brief Forgets every glyph so the atlas can be refilled.
        void Reset();

        [[nodiscard]] uint32_t GetTexture() const { return _Texture.Get(); }
        [[nodiscard]] uint32_t GetWidth() const { return _Packer.GetWidth(); }
        [[nodiscard]] uint32_t GetHeight() const { return _Packer.GetHeight(); }

    private:
        struct Key {
            const FontFace* Face = nullptr;
            uint32_t Index       = 0;
            int32_t Size64       = 0;
            int32_t Subpixel     = 0;

            bool operator==(const Key&) const = default;
        };

        struct KeyHash {
            size_t operator()(const Key& Value) const noexcept;
        };

        FT_FaceRec_* GetFace(const FontFace& Face);
        Entry Rasterize(const FontFace& Face, uint32_t Index, int32_t Size64, int Subpixel, bool& Full);

        FT_LibraryRec_* _Library = nullptr;
        std::unordered_map<const FontFace*, FT_FaceRec_*> _Faces;
        std::unordered_map<Key, Entry, KeyHash> _Entries;
        AtlasPacker _Packer;
        GLTexture _Texture;
    };
}  // namespace XenUI
