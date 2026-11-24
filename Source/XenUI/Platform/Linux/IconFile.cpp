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

#include "XenUI/Platform/Linux/IconFile.hpp"

#include <fstream>
#include <iterator>
#include <vector>

namespace XenUI {
    namespace {
        constexpr uint8_t kPngSignature[] = {0x89, 'P', 'N', 'G'};

        uint32_t Read16(const std::vector<uint8_t>& Bytes, size_t Offset) {
            return Offset + 2 <= Bytes.size() ? static_cast<uint32_t>(Bytes[Offset] | (Bytes[Offset + 1] << 8)) : 0;
        }

        uint32_t Read32(const std::vector<uint8_t>& Bytes, size_t Offset) {
            return Read16(Bytes, Offset) | (Read16(Bytes, Offset + 2) << 16);
        }

        /// @brief Decodes a BITMAPINFOHEADER image as stored in .ico files: bottom-up rows at double height, the
        /// lower half an AND mask that gives 24-bit images their transparency.
        std::optional<IconPixels> DecodeIconBitmap(const std::vector<uint8_t>& Bytes, size_t Offset, size_t Length) {
            if (Length < 40 || Offset + Length > Bytes.size()) { return std::nullopt; }
            const uint32_t HeaderSize = Read32(Bytes, Offset);
            const auto Width          = static_cast<int32_t>(Read32(Bytes, Offset + 4));
            const auto Height         = static_cast<int32_t>(Read32(Bytes, Offset + 8)) / 2;
            const uint32_t Bits       = Read16(Bytes, Offset + 14);
            if (Width <= 0 || Height <= 0 || Width > 1024 || Height > 1024 || (Bits != 32 && Bits != 24)) {
                return std::nullopt;
            }

            const size_t Stride     = (static_cast<size_t>(Width) * Bits / 8 + 3) & ~size_t {3};
            const size_t MaskStride = ((static_cast<size_t>(Width) + 31) / 32) * 4;
            const size_t Pixels     = Offset + HeaderSize;
            const size_t Mask       = Pixels + Stride * static_cast<size_t>(Height);
            if (Mask > Offset + Length) { return std::nullopt; }
            const bool HasMask = Mask + MaskStride * static_cast<size_t>(Height) <= Offset + Length;

            IconPixels Result;
            Result.Width  = static_cast<uint32_t>(Width);
            Result.Height = static_cast<uint32_t>(Height);
            Result.Pixels.resize(static_cast<size_t>(Width) * static_cast<size_t>(Height) * 4);
            for (int32_t Row = 0; Row < Height; ++Row) {
                const size_t SourceRow = static_cast<size_t>(Height - 1 - Row);
                for (int32_t Column = 0; Column < Width; ++Column) {
                    const uint8_t* In = Bytes.data() + Pixels + SourceRow * Stride + static_cast<size_t>(Column) * (Bits / 8);
                    uint8_t* Out      = Result.Pixels.data() + (static_cast<size_t>(Row) * static_cast<size_t>(Width) + static_cast<size_t>(Column)) * 4;
                    Out[0]            = In[0];
                    Out[1]            = In[1];
                    Out[2]            = In[2];
                    Out[3]            = Bits == 32 ? In[3] : 255;
                    if (Bits == 24 && HasMask) {
                        const uint8_t MaskByte = Bytes[Mask + SourceRow * MaskStride + static_cast<size_t>(Column) / 8];
                        if ((MaskByte >> (7 - Column % 8)) & 1) { Out[3] = 0; }
                    }
                }
            }
            return Result;
        }
    }  // namespace

    std::optional<IconPixels> LoadIconFile(const std::filesystem::path& File) {
        std::ifstream Stream(File, std::ios::binary);
        if (!Stream) { return std::nullopt; }
        const std::vector<uint8_t> Bytes((std::istreambuf_iterator<char>(Stream)), std::istreambuf_iterator<char>());

        const bool IsIco = Bytes.size() >= 6 && Read16(Bytes, 0) == 0 && Read16(Bytes, 2) == 1;
        if (!IsIco) { return DecodeImage(Bytes); }

        // Take the largest entry; a size byte of 0 means 256.
        const uint32_t Count = Read16(Bytes, 4);
        size_t Best          = 0;
        uint32_t BestArea    = 0;
        for (uint32_t I = 0; I < Count; ++I) {
            const size_t Entry = 6 + static_cast<size_t>(I) * 16;
            if (Entry + 16 > Bytes.size()) { break; }
            const uint32_t Width  = Bytes[Entry] == 0 ? 256 : Bytes[Entry];
            const uint32_t Height = Bytes[Entry + 1] == 0 ? 256 : Bytes[Entry + 1];
            if (Width * Height > BestArea) {
                BestArea = Width * Height;
                Best     = Entry;
            }
        }
        if (BestArea == 0) { return std::nullopt; }

        const size_t Length = Read32(Bytes, Best + 8);
        const size_t Offset = Read32(Bytes, Best + 12);
        if (Offset + Length > Bytes.size() || Length < 8) { return std::nullopt; }
        if (std::equal(std::begin(kPngSignature), std::end(kPngSignature), Bytes.begin() + static_cast<ptrdiff_t>(Offset))) {
            return DecodeImage(std::span(Bytes).subspan(Offset, Length));
        }
        return DecodeIconBitmap(Bytes, Offset, Length);
    }
}  // namespace XenUI
