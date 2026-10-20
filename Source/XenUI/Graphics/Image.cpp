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
#include "XenUI/AppContext.hpp"
#include "XenUI/Graphics/ImageData.hpp"
#include "XenUI/Platform/HResult.hpp"
#include "XenUI/Platform/Utf8.hpp"

namespace XenUI {
    namespace {
        using Microsoft::WRL::ComPtr;

        std::shared_ptr<ImageData> MakePremultipliedData(IWICImagingFactory& Factory, IWICBitmapSource& Source) {
            ComPtr<IWICFormatConverter> Converter;
            ThrowIfFailed(Factory.CreateFormatConverter(&Converter), "IWICImagingFactory::CreateFormatConverter");
            ThrowIfFailed(Converter->Initialize(&Source,
                                                GUID_WICPixelFormat32bppPBGRA,
                                                WICBitmapDitherTypeNone,
                                                nullptr,
                                                0.0,
                                                WICBitmapPaletteTypeMedianCut),
                          "IWICFormatConverter::Initialize");

            auto Data = std::make_shared<ImageData>();
            ThrowIfFailed(Factory.CreateBitmapFromSource(Converter.Get(), WICBitmapCacheOnLoad, &Data->Pixels),
                          "IWICImagingFactory::CreateBitmapFromSource");

            UINT Width  = 0;
            UINT Height = 0;
            Data->Pixels->GetSize(&Width, &Height);
            Data->Width  = Width;
            Data->Height = Height;
            return Data;
        }
    }  // namespace

    ID2D1Bitmap1* ImageData::GetBitmap(ID2D1DeviceContext& Context, uint64_t DeviceGeneration) {
        if (_Bitmap && _BitmapGeneration == DeviceGeneration) { return _Bitmap.Get(); }

        _Bitmap.Reset();
        const D2D1_BITMAP_PROPERTIES1 Properties =
          D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_NONE,
                                  D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
        ThrowIfFailed(Context.CreateBitmapFromWicBitmap(Pixels.Get(), &Properties, &_Bitmap),
                      "ID2D1DeviceContext::CreateBitmapFromWicBitmap");
        _BitmapGeneration = DeviceGeneration;
        return _Bitmap.Get();
    }

    Image::Image() = default;

    Image::Image(std::shared_ptr<ImageData> Data) : _Data(std::move(Data)) {}

    Image Image::FromFile(std::string_view FilePath) {
        IWICImagingFactory& Factory = *AppContext::Get().GetGraphicsDevice().GetWicFactory();

        ComPtr<IWICBitmapDecoder> Decoder;
        ThrowIfFailed(Factory.CreateDecoderFromFilename(Widen(FilePath).c_str(),
                                                        nullptr,
                                                        GENERIC_READ,
                                                        WICDecodeMetadataCacheOnDemand,
                                                        &Decoder),
                      "IWICImagingFactory::CreateDecoderFromFilename");

        ComPtr<IWICBitmapFrameDecode> Frame;
        ThrowIfFailed(Decoder->GetFrame(0, &Frame), "IWICBitmapDecoder::GetFrame");
        return Image(MakePremultipliedData(Factory, *Frame.Get()));
    }

    Image
    Image::FromPixels(uint32_t PixelWidth, uint32_t PixelHeight, std::span<const uint8_t> Pixels, uint32_t Stride) {
        if (PixelWidth == 0 || PixelHeight == 0) { return {}; }

        const uint32_t RowStride    = Stride == 0 ? PixelWidth * 4 : Stride;
        const size_t RequiredLength = static_cast<size_t>(RowStride) * (PixelHeight - 1) + PixelWidth * 4;
        if (RowStride < PixelWidth * 4 || Pixels.size() < RequiredLength) {
            throw Error("Image::FromPixels: pixel buffer is smaller than the given dimensions");
        }

        IWICImagingFactory& Factory = *AppContext::Get().GetGraphicsDevice().GetWicFactory();

        ComPtr<IWICBitmap> Straight;
        ThrowIfFailed(Factory.CreateBitmapFromMemory(PixelWidth,
                                                     PixelHeight,
                                                     GUID_WICPixelFormat32bppBGRA,
                                                     RowStride,
                                                     static_cast<UINT>(Pixels.size()),
                                                     const_cast<BYTE*>(Pixels.data()),
                                                     &Straight),
                      "IWICImagingFactory::CreateBitmapFromMemory");
        return Image(MakePremultipliedData(Factory, *Straight.Get()));
    }

    uint32_t Image::GetPixelWidth() const {
        return _Data ? _Data->Width : 0;
    }

    uint32_t Image::GetPixelHeight() const {
        return _Data ? _Data->Height : 0;
    }
}  // namespace XenUI
