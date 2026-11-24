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

#include "XenUI/Graphics/GL/GlyphCache.hpp"
#include "XenUI/Core/Error.hpp"
#include "XenUI/Graphics/GL/FontLibrary.hpp"
#include "XenUI/Graphics/GL/GLApi.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_BITMAP_H
#include FT_OUTLINE_H

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace XenUI {
    namespace {
        constexpr uint32_t kAtlasSize = 2048;
        // Matches the synthesis HarfBuzz applies to advances in FontLibrary.cpp.
        constexpr float kSyntheticEmbolden = 0.02f;
        constexpr float kSyntheticSlant    = 0.2f;
        // Linear coverage blended in sRGB looks thinner than DirectWrite's text, so lift it a little.
        constexpr float kCoverageGamma = 1.0f / 1.2f;

        const std::array<uint8_t, 256>& GetCoverageCurve() {
            static const std::array<uint8_t, 256> Curve = [] {
                std::array<uint8_t, 256> Values {};
                for (size_t I = 0; I < Values.size(); ++I) {
                    const float Linear = static_cast<float>(I) / 255.0f;
                    Values[I]          = static_cast<uint8_t>(std::pow(Linear, kCoverageGamma) * 255.0f + 0.5f);
                }
                return Values;
            }();
            return Curve;
        }

        /// @brief Shrinks a premultiplied BGRA bitmap by averaging the source area under each target pixel.
        std::vector<uint8_t> Downscale(const uint8_t* Source,
                                       uint32_t SourceWidth,
                                       uint32_t SourceHeight,
                                       int32_t SourcePitch,
                                       uint32_t Width,
                                       uint32_t Height) {
            std::vector<uint8_t> Result(static_cast<size_t>(Width) * Height * 4, 0);
            const float ScaleX = static_cast<float>(SourceWidth) / static_cast<float>(Width);
            const float ScaleY = static_cast<float>(SourceHeight) / static_cast<float>(Height);
            for (uint32_t Y = 0; Y < Height; ++Y) {
                const auto Y0 = static_cast<uint32_t>(static_cast<float>(Y) * ScaleY);
                const auto Y1 = std::max(Y0 + 1, std::min(SourceHeight, static_cast<uint32_t>((Y + 1) * ScaleY)));
                for (uint32_t X = 0; X < Width; ++X) {
                    const auto X0 = static_cast<uint32_t>(static_cast<float>(X) * ScaleX);
                    const auto X1 = std::max(X0 + 1, std::min(SourceWidth, static_cast<uint32_t>((X + 1) * ScaleX)));
                    uint32_t Sum[4] = {};
                    for (uint32_t SY = Y0; SY < Y1; ++SY) {
                        const uint8_t* Row = Source + static_cast<ptrdiff_t>(SY) * SourcePitch;
                        for (uint32_t SX = X0; SX < X1; ++SX) {
                            for (int C = 0; C < 4; ++C) {
                                Sum[C] += Row[SX * 4 + static_cast<uint32_t>(C)];
                            }
                        }
                    }
                    const uint32_t Count = (Y1 - Y0) * (X1 - X0);
                    uint8_t* Out         = Result.data() + (static_cast<size_t>(Y) * Width + X) * 4;
                    for (int C = 0; C < 4; ++C) {
                        Out[C] = static_cast<uint8_t>((Sum[C] + Count / 2) / Count);
                    }
                }
            }
            return Result;
        }
    }  // namespace

    size_t GlyphCache::KeyHash::operator()(const Key& Value) const noexcept {
        size_t Seed = std::hash<const void*> {}(Value.Face);
        Seed ^= (static_cast<size_t>(Value.Index) << 20) ^ (static_cast<size_t>(Value.Size64) << 3) ^
                static_cast<size_t>(Value.Subpixel);
        return Seed * 0x9E3779B97F4A7C15ull;
    }

    GlyphCache::GlyphCache() : _Packer(kAtlasSize, kAtlasSize) {
        if (FT_Init_FreeType(&_Library) != 0) { throw Error("FreeType: initialization failed"); }
    }

    GlyphCache::~GlyphCache() {
        for (const auto& [Face, Handle] : _Faces) {
            FT_Done_Face(Handle);
        }
        FT_Done_FreeType(_Library);
    }

    void GlyphCache::Initialize(uint64_t Generation) {
        Reset();
        GL::GLuint Name = 0;
        GL::Api.GenTextures(1, &Name);
        _Texture = GLTexture(Name, Generation);
        // The renderer keeps the atlas on unit 0; binding elsewhere would replace another unit's texture.
        GL::Api.ActiveTexture(GL::TEXTURE0);
        GL::Api.BindTexture(GL::TEXTURE_2D, Name);
        GL::Api.TexImage2D(GL::TEXTURE_2D,
                           0,
                           static_cast<GL::GLint>(GL::RGBA8),
                           static_cast<GL::GLsizei>(kAtlasSize),
                           static_cast<GL::GLsizei>(kAtlasSize),
                           0,
                           GL::BGRA,
                           GL::UNSIGNED_BYTE,
                           nullptr);
        GL::Api.TexParameteri(GL::TEXTURE_2D, GL::TEXTURE_MIN_FILTER, static_cast<GL::GLint>(GL::LINEAR));
        GL::Api.TexParameteri(GL::TEXTURE_2D, GL::TEXTURE_MAG_FILTER, static_cast<GL::GLint>(GL::LINEAR));
        GL::Api.TexParameteri(GL::TEXTURE_2D, GL::TEXTURE_WRAP_S, static_cast<GL::GLint>(GL::CLAMP_TO_EDGE));
        GL::Api.TexParameteri(GL::TEXTURE_2D, GL::TEXTURE_WRAP_T, static_cast<GL::GLint>(GL::CLAMP_TO_EDGE));
    }

    void GlyphCache::Reset() {
        _Entries.clear();
        _Packer.Reset();
    }

    const GlyphCache::Entry* GlyphCache::Get(const FontFace& Face, uint32_t Index, float PixelSize, int Subpixel) {
        const Key Lookup {&Face, Index, static_cast<int32_t>(std::lround(PixelSize * 64.0f)), Subpixel};
        if (const auto It = _Entries.find(Lookup); It != _Entries.end()) { return &It->second; }

        bool Full         = false;
        const Entry Glyph = Rasterize(Face, Index, Lookup.Size64, Subpixel, Full);
        if (Full) { return nullptr; }
        return &_Entries.emplace(Lookup, Glyph).first->second;
    }

    FT_FaceRec_* GlyphCache::GetFace(const FontFace& Face) {
        if (const auto It = _Faces.find(&Face); It != _Faces.end()) { return It->second; }

        FT_Face Handle = nullptr;
        if (FT_New_Face(_Library, Face.GetFile().c_str(), Face.GetIndex(), &Handle) != 0) { Handle = nullptr; }
        _Faces.emplace(&Face, Handle);
        return Handle;
    }

    GlyphCache::Entry
    GlyphCache::Rasterize(const FontFace& Face, uint32_t Index, int32_t Size64, int Subpixel, bool& Full) {
        Entry Result;
        FT_Face Handle = GetFace(Face);
        if (Handle == nullptr || Size64 <= 0) { return Result; }

        // Bitmap-only colour fonts come in fixed strikes: take the smallest at least as large as asked, then shrink.
        float BitmapScale = 1.0f;
        if (FT_IS_SCALABLE(Handle)) {
            if (FT_Set_Char_Size(Handle, 0, Size64, 72, 72) != 0) { return Result; }
        } else if (Handle->num_fixed_sizes > 0) {
            int Best = 0;
            for (int I = 0; I < Handle->num_fixed_sizes; ++I) {
                const FT_Pos Strike = Handle->available_sizes[I].y_ppem;
                const FT_Pos Chosen = Handle->available_sizes[Best].y_ppem;
                if ((Chosen < Size64 && Strike > Chosen) || (Strike >= Size64 && Strike < Chosen)) { Best = I; }
            }
            if (FT_Select_Size(Handle, Best) != 0) { return Result; }
            BitmapScale = static_cast<float>(Size64) / static_cast<float>(Handle->available_sizes[Best].y_ppem);
        } else {
            return Result;
        }

        if (FT_Load_Glyph(Handle, Index, FT_LOAD_TARGET_LIGHT | FT_LOAD_COLOR) != 0) { return Result; }
        FT_GlyphSlot Slot = Handle->glyph;

        if (Slot->format == FT_GLYPH_FORMAT_OUTLINE) {
            const float Pixels = static_cast<float>(Size64) / 64.0f;
            if (Face.IsSyntheticBold()) {
                const auto Strength = static_cast<FT_Pos>(std::lround(kSyntheticEmbolden * Pixels * 64.0f));
                FT_Outline_EmboldenXY(&Slot->outline, Strength, Strength);
            }
            if (Face.IsSyntheticItalic()) {
                FT_Matrix Shear {0x10000, static_cast<FT_Fixed>(kSyntheticSlant * 0x10000), 0, 0x10000};
                FT_Outline_Transform(&Slot->outline, &Shear);
            }
            FT_Outline_Translate(&Slot->outline, Subpixel * 64 / kSubpixelSteps, 0);
        }
        if (Slot->format != FT_GLYPH_FORMAT_BITMAP && FT_Render_Glyph(Slot, FT_RENDER_MODE_NORMAL) != 0) {
            return Result;
        }

        FT_Bitmap& Bitmap = Slot->bitmap;
        if (Bitmap.width == 0 || Bitmap.rows == 0) { return Result; }

        std::vector<uint8_t> Pixels;
        uint32_t Width  = Bitmap.width;
        uint32_t Height = Bitmap.rows;
        if (Bitmap.pixel_mode == FT_PIXEL_MODE_BGRA) {
            Result.Color = true;
            if (BitmapScale < 1.0f) {
                const auto ScaledWidth  = std::max(1u, static_cast<uint32_t>(std::lround(Width * BitmapScale)));
                const auto ScaledHeight = std::max(1u, static_cast<uint32_t>(std::lround(Height * BitmapScale)));
                Pixels = Downscale(Bitmap.buffer, Width, Height, Bitmap.pitch, ScaledWidth, ScaledHeight);
                Width  = ScaledWidth;
                Height = ScaledHeight;
            } else {
                Pixels.resize(static_cast<size_t>(Width) * Height * 4);
                for (uint32_t Y = 0; Y < Height; ++Y) {
                    std::copy_n(Bitmap.buffer + static_cast<ptrdiff_t>(Y) * Bitmap.pitch,
                                Width * 4,
                                Pixels.data() + static_cast<size_t>(Y) * Width * 4);
                }
            }
        } else {
            FT_Bitmap Gray;
            FT_Bitmap_Init(&Gray);
            const FT_Bitmap* Source = &Bitmap;
            if (Bitmap.pixel_mode != FT_PIXEL_MODE_GRAY) {
                if (FT_Bitmap_Convert(_Library, &Bitmap, &Gray, 1) != 0) { return Result; }
                // Converted 1-bit bitmaps hold 0 or 1; stretch them to full coverage.
                if (Gray.num_grays == 2) {
                    for (size_t I = 0; I < static_cast<size_t>(Gray.rows) * static_cast<size_t>(Gray.pitch); ++I) {
                        Gray.buffer[I] = Gray.buffer[I] != 0 ? 255 : 0;
                    }
                }
                Source = &Gray;
            }
            const std::array<uint8_t, 256>& Curve = GetCoverageCurve();
            Pixels.resize(static_cast<size_t>(Width) * Height * 4);
            for (uint32_t Y = 0; Y < Height; ++Y) {
                const uint8_t* Row = Source->buffer + static_cast<ptrdiff_t>(Y) * Source->pitch;
                for (uint32_t X = 0; X < Width; ++X) {
                    const uint8_t Coverage = Curve[Row[X]];
                    uint8_t* Out           = Pixels.data() + (static_cast<size_t>(Y) * Width + X) * 4;
                    Out[0] = Out[1] = Out[2] = Out[3] = Coverage;
                }
            }
            FT_Bitmap_Done(_Library, &Gray);
        }

        const std::optional<AtlasPacker::Slot> Slot2 = _Packer.Allocate(Width, Height);
        if (!Slot2) {
            // Too big for even an empty atlas: draw nothing rather than resetting forever.
            Full = Width + 1 <= _Packer.GetWidth() && Height + 1 <= _Packer.GetHeight();
            return {};
        }

        GL::Api.ActiveTexture(GL::TEXTURE0);
        GL::Api.BindTexture(GL::TEXTURE_2D, _Texture.Get());
        GL::Api.PixelStorei(GL::UNPACK_ALIGNMENT, 4);
        GL::Api.TexSubImage2D(GL::TEXTURE_2D,
                              0,
                              static_cast<GL::GLint>(Slot2->X),
                              static_cast<GL::GLint>(Slot2->Y),
                              static_cast<GL::GLsizei>(Width),
                              static_cast<GL::GLsizei>(Height),
                              GL::BGRA,
                              GL::UNSIGNED_BYTE,
                              Pixels.data());

        const float Scale = Result.Color ? std::min(BitmapScale, 1.0f) : 1.0f;
        Result.Left       = static_cast<int32_t>(std::lround(static_cast<float>(Slot->bitmap_left) * Scale));
        Result.Top        = -static_cast<int32_t>(std::lround(static_cast<float>(Slot->bitmap_top) * Scale));
        Result.Width      = Width;
        Result.Height     = Height;
        Result.AtlasX     = Slot2->X;
        Result.AtlasY     = Slot2->Y;
        return Result;
    }
}  // namespace XenUI
