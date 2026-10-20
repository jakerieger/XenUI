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

#include <d2d1_1.h>
#include <d3d11.h>
#include <dwrite.h>
#include <dxgi1_3.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <cstdint>
#include <memory>
#include <unordered_map>

namespace XenUI {
    using Microsoft::WRL::ComPtr;

    class D2DResources;
    class LiveRenderer;

    struct FontHash {
        size_t operator()(const Font& Value) const noexcept;
    };

    /// @brief Shared D3D11/D2D/DirectWrite/WIC stack. Single-threaded; owned by the UI thread.
    /// Factories survive device loss; device-dependent objects are rebuilt by Recreate, which bumps the generation.
    class GraphicsDevice {
    public:
        GraphicsDevice();
        ~GraphicsDevice();

        GraphicsDevice(const GraphicsDevice&)            = delete;
        GraphicsDevice& operator=(const GraphicsDevice&) = delete;

        /// @brief Rebuilds the D3D11 and D2D devices after a device-removed or device-reset error.
        void Recreate();

        /// @brief Increments whenever the device is recreated, so cached GPU resources can detect staleness.
        [[nodiscard]] uint64_t GetGeneration() const { return _Generation; }

        [[nodiscard]] ID3D11Device* GetD3DDevice() const { return _D3DDevice.Get(); }
        [[nodiscard]] ID3D11DeviceContext* GetD3DContext() const { return _D3DContext.Get(); }
        [[nodiscard]] IDXGIFactory2* GetDxgiFactory() const { return _DxgiFactory.Get(); }
        [[nodiscard]] ID2D1Factory1* GetD2DFactory() const { return _D2DFactory.Get(); }
        [[nodiscard]] ID2D1Device* GetD2DDevice() const { return _D2DDevice.Get(); }
        [[nodiscard]] IDWriteFactory* GetDWriteFactory() const { return _DWriteFactory.Get(); }
        [[nodiscard]] IWICImagingFactory* GetWicFactory() const { return _WicFactory.Get(); }
        [[nodiscard]] D2DResources& GetResources() const { return *_Resources; }

        /// @brief Direct3D renderer for live canvases, created on first use and rebuilt after device loss.
        [[nodiscard]] LiveRenderer& GetLiveRenderer();

        IDWriteTextFormat* GetTextFormat(const Font& TextFont);

    private:
        void CreateFactories();
        void CreateDevices();
        void CreateD3DDevice();

        ComPtr<ID2D1Factory1> _D2DFactory;
        ComPtr<IDWriteFactory> _DWriteFactory;
        ComPtr<IWICImagingFactory> _WicFactory;

        ComPtr<ID3D11Device> _D3DDevice;
        ComPtr<ID3D11DeviceContext> _D3DContext;
        ComPtr<IDXGIDevice1> _DxgiDevice;
        ComPtr<IDXGIFactory2> _DxgiFactory;
        ComPtr<ID2D1Device> _D2DDevice;

        std::unique_ptr<D2DResources> _Resources;
        std::unique_ptr<LiveRenderer> _LiveRenderer;
        std::unordered_map<Font, ComPtr<IDWriteTextFormat>, FontHash> _TextFormats;
        uint64_t _Generation = 1;
    };
}  // namespace XenUI
