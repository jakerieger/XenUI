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

#include "XenUI/Graphics/Text.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

struct hb_font_t;
struct _FcPattern;
struct _FcFontSet;

namespace XenUI {
    class FontLibrary;

    /// @brief One font file, ready for shaping at any size. Positions from HarfBuzz are in font units; multiply by
    /// GetScale(em size) to get device-independent pixels.
    class FontFace {
    public:
        FontFace(const std::string& File, int Index, bool SyntheticBold, bool SyntheticItalic);
        ~FontFace();

        FontFace(const FontFace&)            = delete;
        FontFace& operator=(const FontFace&) = delete;

        [[nodiscard]] hb_font_t* GetFont() const { return _Font; }
        [[nodiscard]] const std::string& GetFile() const { return _File; }
        [[nodiscard]] int GetIndex() const { return _Index; }
        [[nodiscard]] bool IsSyntheticBold() const { return _SyntheticBold; }
        [[nodiscard]] bool IsSyntheticItalic() const { return _SyntheticItalic; }

        [[nodiscard]] float GetScale(float EmSize) const { return EmSize / _UnitsPerEm; }
        /// @brief Distance above the baseline, in font units.
        [[nodiscard]] float GetAscender() const { return _Ascender; }
        /// @brief Distance below the baseline as a positive number, in font units.
        [[nodiscard]] float GetDescender() const { return _Descender; }
        [[nodiscard]] float GetLineGap() const { return _LineGap; }

        [[nodiscard]] bool HasGlyph(uint32_t Codepoint) const;

    private:
        hb_font_t* _Font = nullptr;
        std::string _File;
        int _Index            = 0;
        bool _SyntheticBold   = false;
        bool _SyntheticItalic = false;
        float _UnitsPerEm     = 1000.0f;
        float _Ascender       = 0.0f;
        float _Descender      = 0.0f;
        float _LineGap        = 0.0f;
    };

    /// @brief The face fontconfig picked for a Font, plus the faces it falls back to for characters that face lacks.
    class FontSet {
    public:
        FontSet(FontLibrary& Library, _FcPattern* Pattern, std::shared_ptr<FontFace> Primary);
        ~FontSet();

        FontSet(const FontSet&)            = delete;
        FontSet& operator=(const FontSet&) = delete;

        [[nodiscard]] const std::shared_ptr<FontFace>& GetPrimary() const { return _Primary; }

        /// @brief The face that draws Codepoint: the primary face when it has the glyph, else the closest fontconfig
        /// match that does, else the primary face (which then draws its missing-glyph box).
        [[nodiscard]] const std::shared_ptr<FontFace>& GetFaceFor(uint32_t Codepoint);

    private:
        FontLibrary& _Library;
        _FcPattern* _Pattern = nullptr;
        std::shared_ptr<FontFace> _Primary;
        /// @brief Every installed font, best match first, for finding fallbacks. Sorted on first use.
        struct _FcFontSet* _Candidates = nullptr;
        std::unordered_map<uint32_t, std::shared_ptr<FontFace>> _Fallbacks;
    };

    /// @brief Resolves fonts through fontconfig and owns every loaded face. Needs no GL context, so text can be laid
    /// out headless.
    class FontLibrary {
    public:
        FontLibrary();
        ~FontLibrary();

        FontLibrary(const FontLibrary&)            = delete;
        FontLibrary& operator=(const FontLibrary&) = delete;

        /// @brief The font set for a family, weight and style. The em size doesn't matter; faces scale.
        [[nodiscard]] std::shared_ptr<FontSet> GetFontSet(const Font& Request);

        /// @brief The face fontconfig matched for Pattern, loaded once per file, index and synthesis.
        [[nodiscard]] std::shared_ptr<FontFace> LoadMatch(_FcPattern* Requested, _FcPattern* Matched);

    private:
        using SetKey  = std::tuple<std::string, uint16_t, uint8_t>;
        using FaceKey = std::tuple<std::string, int, bool, bool>;

        std::map<SetKey, std::shared_ptr<FontSet>> _Sets;
        std::map<FaceKey, std::shared_ptr<FontFace>> _Faces;
    };
}  // namespace XenUI
