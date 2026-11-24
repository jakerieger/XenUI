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


#include "XenUI/Graphics/GL/FontLibrary.hpp"
#include "XenUI/Core/Error.hpp"

#include <fontconfig/fontconfig.h>
#include <hb.h>

#include <format>

namespace XenUI {
    namespace {
        constexpr float kSyntheticEmbolden = 0.02f;
        constexpr float kSyntheticSlant    = 0.2f;

        /// @brief Blocks whose characters DirectWrite draws in colour by default: emoji, pictographs and symbols.
        bool IsPictograph(uint32_t Codepoint) {
            return (Codepoint >= 0x1F000 && Codepoint <= 0x1FAFF) || (Codepoint >= 0x2600 && Codepoint <= 0x27BF) ||
                   (Codepoint >= 0x2B00 && Codepoint <= 0x2BFF);
        }

        int ToFontconfigSlant(FontStyle Style) {
            switch (Style) {
                case FontStyle::Italic:
                    return FC_SLANT_ITALIC;
                case FontStyle::Oblique:
                    return FC_SLANT_OBLIQUE;
                default:
                    return FC_SLANT_ROMAN;
            }
        }
    }  // namespace

    FontFace::FontFace(const std::string& File, int Index, bool SyntheticBold, bool SyntheticItalic)
        : _File(File), _Index(Index), _SyntheticBold(SyntheticBold), _SyntheticItalic(SyntheticItalic) {
        hb_blob_t* Blob = hb_blob_create_from_file_or_fail(File.c_str());
        if (Blob == nullptr) { throw Error(std::format("Font: cannot read '{}'", File)); }
        // fontconfig packs a variable font's named instance into the upper 16 bits of the index, as FreeType does.
        const auto FaceIndex = static_cast<unsigned>(Index) & 0xFFFFu;
        const auto Instance  = static_cast<unsigned>(Index) >> 16;
        hb_face_t* Face      = hb_face_create(Blob, FaceIndex);
        hb_blob_destroy(Blob);
        _UnitsPerEm = static_cast<float>(std::max(1u, hb_face_get_upem(Face)));
        _Font       = hb_font_create(Face);
        hb_face_destroy(Face);
        if (Instance > 0) { hb_font_set_var_named_instance(_Font, Instance - 1); }

        if (SyntheticBold) { hb_font_set_synthetic_bold(_Font, kSyntheticEmbolden, kSyntheticEmbolden, true); }
        if (SyntheticItalic) { hb_font_set_synthetic_slant(_Font, kSyntheticSlant); }

        hb_font_extents_t Extents {};
        hb_font_get_h_extents(_Font, &Extents);
        _Ascender  = static_cast<float>(Extents.ascender);
        _Descender = static_cast<float>(-Extents.descender);
        _LineGap   = static_cast<float>(Extents.line_gap);
        if (_Ascender + _Descender <= 0.0f) {
            _Ascender  = _UnitsPerEm * 0.8f;
            _Descender = _UnitsPerEm * 0.2f;
        }
    }

    FontFace::~FontFace() {
        hb_font_destroy(_Font);
    }

    bool FontFace::HasGlyph(uint32_t Codepoint) const {
        hb_codepoint_t Glyph = 0;
        return hb_font_get_nominal_glyph(_Font, Codepoint, &Glyph) != 0;
    }

    FontSet::FontSet(FontLibrary& Library, FcPattern* Pattern, std::shared_ptr<FontFace> Primary)
        : _Library(Library), _Pattern(Pattern), _Primary(std::move(Primary)) {}

    FontSet::~FontSet() {
        if (_Candidates != nullptr) { FcFontSetDestroy(_Candidates); }
        FcPatternDestroy(_Pattern);
    }

    const std::shared_ptr<FontFace>& FontSet::GetFaceFor(uint32_t Codepoint) {
        if (_Primary->HasGlyph(Codepoint)) { return _Primary; }
        if (const auto Found = _Fallbacks.find(Codepoint); Found != _Fallbacks.end()) { return Found->second; }

        std::shared_ptr<FontFace> Choice = _Primary;
        if (_Candidates == nullptr) {
            FcResult Result = FcResultMatch;
            _Candidates     = FcFontSort(nullptr, _Pattern, FcTrue, nullptr, &Result);
        }
        const auto Search = [&](bool ColorOnly) {
            for (int Index = 0; _Candidates != nullptr && Index < _Candidates->nfont; ++Index) {
                FcPattern* Candidate = _Candidates->fonts[Index];
                FcCharSet* Chars     = nullptr;
                if (FcPatternGetCharSet(Candidate, FC_CHARSET, 0, &Chars) != FcResultMatch ||
                    FcCharSetHasChar(Chars, Codepoint) == FcFalse) {
                    continue;
                }
                FcBool Color = FcFalse;
                if (ColorOnly && (FcPatternGetBool(Candidate, FC_COLOR, 0, &Color) != FcResultMatch || !Color)) {
                    continue;
                }
                FcPattern* Prepared = FcFontRenderPrepare(nullptr, _Pattern, Candidate);
                if (Prepared == nullptr) { continue; }
                try {
                    std::shared_ptr<FontFace> Face = _Library.LoadMatch(_Pattern, Prepared);
                    FcPatternDestroy(Prepared);
                    if (Face->HasGlyph(Codepoint)) {
                        Choice = std::move(Face);
                        return true;
                    }
                } catch (const Error&) { FcPatternDestroy(Prepared); }
            }
            return false;
        };
        // Like DirectWrite with colour fonts on, pictographs come from a colour font when one has them, even if a
        // monochrome symbol font sorts first.
        if (!(IsPictograph(Codepoint) && Search(true))) { Search(false); }
        return _Fallbacks[Codepoint] = std::move(Choice);
    }

    FontLibrary::FontLibrary() {
        if (FcInit() == FcFalse) { throw Error("Font: fontconfig failed to initialise"); }
    }

    FontLibrary::~FontLibrary() = default;

    std::shared_ptr<FontSet> FontLibrary::GetFontSet(const Font& Request) {
        const SetKey Key {Request.Family, static_cast<uint16_t>(Request.Weight), static_cast<uint8_t>(Request.Style)};
        if (const auto Found = _Sets.find(Key); Found != _Sets.end()) { return Found->second; }

        FcPattern* Pattern = FcPatternCreate();
        FcPatternAddString(Pattern, FC_FAMILY, reinterpret_cast<const FcChar8*>(Request.Family.c_str()));
        FcPatternAddDouble(Pattern, FC_WEIGHT, FcWeightFromOpenTypeDouble(static_cast<double>(Request.Weight)));
        FcPatternAddInteger(Pattern, FC_SLANT, ToFontconfigSlant(Request.Style));
        FcConfigSubstitute(nullptr, Pattern, FcMatchPattern);
        FcDefaultSubstitute(Pattern);

        FcResult Result    = FcResultMatch;
        FcPattern* Matched = FcFontMatch(nullptr, Pattern, &Result);
        if (Matched == nullptr) {
            FcPatternDestroy(Pattern);
            throw Error(std::format("Font: no installed font matches '{}'", Request.Family));
        }

        std::shared_ptr<FontFace> Primary;
        try {
            Primary = LoadMatch(Pattern, Matched);
        } catch (...) {
            FcPatternDestroy(Matched);
            FcPatternDestroy(Pattern);
            throw;
        }
        FcPatternDestroy(Matched);

        auto Set = std::make_shared<FontSet>(*this, Pattern, std::move(Primary));
        _Sets.emplace(Key, Set);
        return Set;
    }

    std::shared_ptr<FontFace> FontLibrary::LoadMatch(FcPattern* Requested, FcPattern* Matched) {
        FcChar8* File = nullptr;
        if (FcPatternGetString(Matched, FC_FILE, 0, &File) != FcResultMatch || File == nullptr) {
            throw Error("Font: fontconfig matched a font without a file");
        }
        int Index = 0;
        FcPatternGetInteger(Matched, FC_INDEX, 0, &Index);

        // Synthesise what the request asks for but the matched face lacks, as DirectWrite does.
        double RequestedWeight = FC_WEIGHT_REGULAR;
        double MatchedWeight   = FC_WEIGHT_REGULAR;
        int RequestedSlant     = FC_SLANT_ROMAN;
        int MatchedSlant       = FC_SLANT_ROMAN;
        FcBool Embolden        = FcFalse;
        FcPatternGetDouble(Requested, FC_WEIGHT, 0, &RequestedWeight);
        FcPatternGetDouble(Matched, FC_WEIGHT, 0, &MatchedWeight);
        FcPatternGetInteger(Requested, FC_SLANT, 0, &RequestedSlant);
        FcPatternGetInteger(Matched, FC_SLANT, 0, &MatchedSlant);
        FcPatternGetBool(Matched, FC_EMBOLDEN, 0, &Embolden);
        const bool Bold =
          Embolden != FcFalse || (RequestedWeight >= FC_WEIGHT_DEMIBOLD && MatchedWeight <= FC_WEIGHT_MEDIUM);
        const bool Italic = RequestedSlant != FC_SLANT_ROMAN && MatchedSlant == FC_SLANT_ROMAN;

        const FaceKey Key {reinterpret_cast<const char*>(File), Index, Bold, Italic};
        if (const auto Found = _Faces.find(Key); Found != _Faces.end()) { return Found->second; }
        auto Face = std::make_shared<FontFace>(std::get<0>(Key), Index, Bold, Italic);
        _Faces.emplace(Key, Face);
        return Face;
    }
}  // namespace XenUI
