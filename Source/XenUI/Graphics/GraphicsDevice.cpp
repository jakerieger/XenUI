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

#include "XenUI/Graphics/GraphicsDevice.hpp"
#include "XenUI/Graphics/D2DRenderer.hpp"
#include "XenUI/Graphics/LiveRenderer.hpp"
#include "XenUI/Platform/HResult.hpp"
#include "XenUI/Platform/Utf8.hpp"

#include <functional>
#include <iterator>

namespace XenUI {
    namespace {
        void HashCombine(size_t& Seed, size_t Value) {
            Seed ^= Value + 0x9E3779B97F4A7C15ull + (Seed << 6) + (Seed >> 2);
        }

        DWRITE_FONT_STYLE ToDWrite(FontStyle Style) {
            switch (Style) {
                case FontStyle::Italic:
                    return DWRITE_FONT_STYLE_ITALIC;
                case FontStyle::Oblique:
                    return DWRITE_FONT_STYLE_OBLIQUE;
                default:
                    return DWRITE_FONT_STYLE_NORMAL;
            }
        }
    }  // namespace

    size_t FontHash::operator()(const Font& Value) const noexcept {
        size_t Seed = std::hash<std::string> {}(Value.Family);
        HashCombine(Seed, std::hash<float> {}(Value.Size));
        HashCombine(Seed, static_cast<size_t>(Value.Weight));
        HashCombine(Seed, static_cast<size_t>(Value.Style));
        return Seed;
    }

    GraphicsDevice::GraphicsDevice() : _Resources(std::make_unique<D2DResources>()) {
        CreateFactories();
        CreateDevices();
    }

    GraphicsDevice::~GraphicsDevice() = default;

    LiveRenderer& GraphicsDevice::GetLiveRenderer() {
        if (!_LiveRenderer) { _LiveRenderer = std::make_unique<LiveRenderer>(*this); }
        return *_LiveRenderer;
    }

    void GraphicsDevice::Recreate() {
        _LiveRenderer.reset();
        _Resources->Reset();
        _D2DDevice.Reset();
        _DxgiFactory.Reset();
        _DxgiDevice.Reset();
        if (_D3DContext) { _D3DContext->ClearState(); }
        _D3DContext.Reset();
        _D3DDevice.Reset();

        CreateDevices();
        ++_Generation;
    }

    IDWriteTextFormat* GraphicsDevice::GetTextFormat(const Font& TextFont) {
        if (const auto It = _TextFormats.find(TextFont); It != _TextFormats.end()) { return It->second.Get(); }

        ComPtr<IDWriteTextFormat> Format;
        ThrowIfFailed(_DWriteFactory->CreateTextFormat(Widen(TextFont.Family).c_str(),
                                                       nullptr,
                                                       static_cast<DWRITE_FONT_WEIGHT>(TextFont.Weight),
                                                       ToDWrite(TextFont.Style),
                                                       DWRITE_FONT_STRETCH_NORMAL,
                                                       TextFont.Size,
                                                       L"en-us",
                                                       &Format),
                      "IDWriteFactory::CreateTextFormat");
        return _TextFormats.emplace(TextFont, std::move(Format)).first->second.Get();
    }

    void GraphicsDevice::CreateFactories() {
        D2D1_FACTORY_OPTIONS Options {};
#ifndef NDEBUG
        Options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif

        HRESULT Result =
          D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &Options, &_D2DFactory);
        if (FAILED(Result) && Options.debugLevel != D2D1_DEBUG_LEVEL_NONE) {
            Options.debugLevel = D2D1_DEBUG_LEVEL_NONE;
            Result =
              D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &Options, &_D2DFactory);
        }
        ThrowIfFailed(Result, "D2D1CreateFactory");

        ThrowIfFailed(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                                          __uuidof(IDWriteFactory),
                                          reinterpret_cast<IUnknown**>(_DWriteFactory.GetAddressOf())),
                      "DWriteCreateFactory");

        ThrowIfFailed(
          CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&_WicFactory)),
          "CoCreateInstance(WICImagingFactory)");
    }

    void GraphicsDevice::CreateDevices() {
        CreateD3DDevice();

        ThrowIfFailed(_D3DDevice.As(&_DxgiDevice), "Query IDXGIDevice1");

        ComPtr<IDXGIAdapter> Adapter;
        ThrowIfFailed(_DxgiDevice->GetAdapter(&Adapter), "IDXGIDevice::GetAdapter");
        ThrowIfFailed(Adapter->GetParent(IID_PPV_ARGS(&_DxgiFactory)), "IDXGIAdapter::GetParent");

        ThrowIfFailed(_D2DFactory->CreateDevice(_DxgiDevice.Get(), &_D2DDevice), "ID2D1Factory1::CreateDevice");
    }

    void GraphicsDevice::CreateD3DDevice() {
        constexpr D3D_FEATURE_LEVEL kFeatureLevels[] = {
          D3D_FEATURE_LEVEL_11_1,
          D3D_FEATURE_LEVEL_11_0,
          D3D_FEATURE_LEVEL_10_1,
          D3D_FEATURE_LEVEL_10_0,
        };

        UINT Flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifndef NDEBUG
        Flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        const auto Create = [&](D3D_DRIVER_TYPE DriverType) {
            return D3D11CreateDevice(nullptr,
                                     DriverType,
                                     nullptr,
                                     Flags,
                                     kFeatureLevels,
                                     static_cast<UINT>(std::size(kFeatureLevels)),
                                     D3D11_SDK_VERSION,
                                     &_D3DDevice,
                                     nullptr,
                                     &_D3DContext);
        };

        HRESULT Result = Create(D3D_DRIVER_TYPE_HARDWARE);
        if (Result == DXGI_ERROR_SDK_COMPONENT_MISSING) {
            Flags &= ~D3D11_CREATE_DEVICE_DEBUG;
            Result = Create(D3D_DRIVER_TYPE_HARDWARE);
        }
        if (FAILED(Result)) { Result = Create(D3D_DRIVER_TYPE_WARP); }
        ThrowIfFailed(Result, "D3D11CreateDevice");
    }
}  // namespace XenUI
