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


#include "XenUI/Graphics/Image.hpp"
#include "XenUI/Core/Error.hpp"
#include "XenUI/Graphics/GL/ImageData.hpp"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_NO_STDIO_DEPRECATION
#if defined(__GNUC__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wunused-function"
    #pragma GCC diagnostic ignored "-Wsign-compare"
    #pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
#include <stb_image.h>
#if defined(__GNUC__)
    #pragma GCC diagnostic pop
#endif

#include <format>
#include <memory>
#include <string>

namespace XenUI {
    namespace {
        /// @brief Copies 32-bit rows into tightly packed premultiplied BGRA. Red and blue swap when the source is RGBA.
        std::shared_ptr<ImageData>
        MakePremultipliedData(uint32_t Width, uint32_t Height, const uint8_t* Source, size_t Stride, bool SwapRedBlue) {
            auto Data    = std::make_shared<ImageData>();
            Data->Width  = Width;
            Data->Height = Height;
            Data->Pixels.resize(static_cast<size_t>(Width) * Height * 4);

            uint8_t* Out = Data->Pixels.data();
            for (uint32_t Row = 0; Row < Height; ++Row) {
                const uint8_t* In = Source + Stride * Row;
                for (uint32_t Column = 0; Column < Width; ++Column, In += 4, Out += 4) {
                    const uint32_t Alpha = In[3];
                    const auto Scale     = [Alpha](uint8_t Channel) {
                        return static_cast<uint8_t>((Channel * Alpha + 127) / 255);
                    };
                    Out[0] = Scale(SwapRedBlue ? In[2] : In[0]);
                    Out[1] = Scale(In[1]);
                    Out[2] = Scale(SwapRedBlue ? In[0] : In[2]);
                    Out[3] = static_cast<uint8_t>(Alpha);
                }
            }
            return Data;
        }
    }  // namespace

    Image::Image() = default;

    Image::Image(std::shared_ptr<ImageData> Data) : _Data(std::move(Data)) {}

    Image Image::FromFile(std::string_view FilePath) {
        const std::string Path(FilePath);
        int Width    = 0;
        int Height   = 0;
        int Channels = 0;
        const std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> Pixels(
          stbi_load(Path.c_str(), &Width, &Height, &Channels, 4),
          &stbi_image_free);
        if (!Pixels || Width <= 0 || Height <= 0) {
            throw Error(std::format("Image::FromFile: cannot decode '{}': {}", Path, stbi_failure_reason()));
        }
        return Image(MakePremultipliedData(static_cast<uint32_t>(Width),
                                           static_cast<uint32_t>(Height),
                                           Pixels.get(),
                                           static_cast<size_t>(Width) * 4,
                                           true));
    }

    Image
    Image::FromPixels(uint32_t PixelWidth, uint32_t PixelHeight, std::span<const uint8_t> Pixels, uint32_t Stride) {
        if (PixelWidth == 0 || PixelHeight == 0) { return {}; }

        const uint32_t RowStride    = Stride == 0 ? PixelWidth * 4 : Stride;
        const size_t RequiredLength = static_cast<size_t>(RowStride) * (PixelHeight - 1) + PixelWidth * 4;
        if (RowStride < PixelWidth * 4 || Pixels.size() < RequiredLength) {
            throw Error("Image::FromPixels: pixel buffer is smaller than the given dimensions");
        }
        return Image(MakePremultipliedData(PixelWidth, PixelHeight, Pixels.data(), RowStride, false));
    }

    uint32_t Image::GetPixelWidth() const {
        return _Data ? _Data->Width : 0;
    }

    uint32_t Image::GetPixelHeight() const {
        return _Data ? _Data->Height : 0;
    }

    std::optional<DecodedImage> DecodeImage(std::span<const uint8_t> Encoded) {
        int Width    = 0;
        int Height   = 0;
        int Channels = 0;
        std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> Pixels(
          stbi_load_from_memory(Encoded.data(), static_cast<int>(Encoded.size()), &Width, &Height, &Channels, 4),
          &stbi_image_free);
        if (!Pixels || Width <= 0 || Height <= 0) { return std::nullopt; }

        DecodedImage Result;
        Result.Width  = static_cast<uint32_t>(Width);
        Result.Height = static_cast<uint32_t>(Height);
        Result.Pixels.assign(Pixels.get(), Pixels.get() + static_cast<size_t>(Width) * static_cast<size_t>(Height) * 4);
        for (size_t I = 0; I < Result.Pixels.size(); I += 4) {
            std::swap(Result.Pixels[I], Result.Pixels[I + 2]);
        }
        return Result;
    }
}  // namespace XenUI
