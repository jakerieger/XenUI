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

#include "XenUI/Graphics/LiveRenderer.hpp"
#include "XenUI/Graphics/DeviceLost.hpp"
#include "XenUI/Graphics/GraphicsDevice.hpp"

#include <Windows.h>

#include "LivePrimitivesPS.h"
#include "LivePrimitivesVS.h"
#include "LiveTexturePS.h"
#include "LiveTextureVS.h"

#include <algorithm>
#include <cstring>
#include <iterator>

namespace XenUI {
    namespace {
        constexpr size_t kInitialInstanceCapacity = 4096;
        constexpr UINT kQuadVertexCount           = 4;

        struct FrameConstants {
            float ViewportSize[2];
            float PixelsPerDip;
            float Unused;
        };

        struct TextureConstants {
            float ViewportSize[2];
            float SourceOffset;
            float Opacity;
            float Destination[4];
        };

        template<class T>
        void WriteConstants(ID3D11DeviceContext& Context, ID3D11Buffer& Buffer, const T& Value) {
            D3D11_MAPPED_SUBRESOURCE Mapped {};
            CheckDeviceResult(Context.Map(&Buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped), "ID3D11DeviceContext::Map");
            std::memcpy(Mapped.pData, &Value, sizeof(T));
            Context.Unmap(&Buffer, 0);
        }

        Microsoft::WRL::ComPtr<ID3D11Buffer> CreateConstantBuffer(ID3D11Device& Device, UINT ByteWidth) {
            const D3D11_BUFFER_DESC Desc {
              .ByteWidth      = (ByteWidth + 15) / 16 * 16,
              .Usage          = D3D11_USAGE_DYNAMIC,
              .BindFlags      = D3D11_BIND_CONSTANT_BUFFER,
              .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
            };
            Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer;
            CheckDeviceResult(Device.CreateBuffer(&Desc, nullptr, &Buffer), "ID3D11Device::CreateBuffer");
            return Buffer;
        }
    }  // namespace

    LiveRenderer::LiveRenderer(GraphicsDevice& Device) : _Device(Device) {
        ID3D11Device& D3D = *Device.GetD3DDevice();

        CheckDeviceResult(D3D.CreateVertexShader(kLivePrimitivesVertexShader,
                                                 sizeof(kLivePrimitivesVertexShader),
                                                 nullptr,
                                                 &_PrimitiveVertexShader),
                          "ID3D11Device::CreateVertexShader");
        CheckDeviceResult(D3D.CreatePixelShader(kLivePrimitivesPixelShader,
                                                sizeof(kLivePrimitivesPixelShader),
                                                nullptr,
                                                &_PrimitivePixelShader),
                          "ID3D11Device::CreatePixelShader");
        CheckDeviceResult(D3D.CreateVertexShader(kLiveTextureVertexShader,
                                                 sizeof(kLiveTextureVertexShader),
                                                 nullptr,
                                                 &_TextureVertexShader),
                          "ID3D11Device::CreateVertexShader");
        CheckDeviceResult(D3D.CreatePixelShader(kLiveTexturePixelShader,
                                                sizeof(kLiveTexturePixelShader),
                                                nullptr,
                                                &_TexturePixelShader),
                          "ID3D11Device::CreatePixelShader");

        const D3D11_INPUT_ELEMENT_DESC Elements[] = {
          {"START", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(LiveInstance, Start), D3D11_INPUT_PER_INSTANCE_DATA, 1},
          {"END", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(LiveInstance, End), D3D11_INPUT_PER_INSTANCE_DATA, 1},
          {"COLOR",
           0,
           DXGI_FORMAT_R32G32B32A32_FLOAT,
           0,
           offsetof(LiveInstance, Color),
           D3D11_INPUT_PER_INSTANCE_DATA,
           1},
          {"WIDTH", 0, DXGI_FORMAT_R32_FLOAT, 0, offsetof(LiveInstance, Width), D3D11_INPUT_PER_INSTANCE_DATA, 1},
          {"KIND", 0, DXGI_FORMAT_R32_FLOAT, 0, offsetof(LiveInstance, Kind), D3D11_INPUT_PER_INSTANCE_DATA, 1},
        };
        CheckDeviceResult(D3D.CreateInputLayout(Elements,
                                                static_cast<UINT>(std::size(Elements)),
                                                kLivePrimitivesVertexShader,
                                                sizeof(kLivePrimitivesVertexShader),
                                                &_PrimitiveLayout),
                          "ID3D11Device::CreateInputLayout");

        _FrameConstants   = CreateConstantBuffer(D3D, sizeof(FrameConstants));
        _TextureConstants = CreateConstantBuffer(D3D, sizeof(TextureConstants));
        EnsureInstanceCapacity(kInitialInstanceCapacity);

        D3D11_BLEND_DESC Blend {};
        Blend.RenderTarget[0] = {
          .BlendEnable           = TRUE,
          .SrcBlend              = D3D11_BLEND_ONE,
          .DestBlend             = D3D11_BLEND_INV_SRC_ALPHA,
          .BlendOp               = D3D11_BLEND_OP_ADD,
          .SrcBlendAlpha         = D3D11_BLEND_ONE,
          .DestBlendAlpha        = D3D11_BLEND_INV_SRC_ALPHA,
          .BlendOpAlpha          = D3D11_BLEND_OP_ADD,
          .RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL,
        };
        CheckDeviceResult(D3D.CreateBlendState(&Blend, &_PremultipliedBlend), "ID3D11Device::CreateBlendState");

        const D3D11_RASTERIZER_DESC Rasterizer {
          .FillMode        = D3D11_FILL_SOLID,
          .CullMode        = D3D11_CULL_NONE,
          .DepthClipEnable = TRUE,
        };
        CheckDeviceResult(D3D.CreateRasterizerState(&Rasterizer, &_Rasterizer), "ID3D11Device::CreateRasterizerState");

        const D3D11_SAMPLER_DESC Sampler {
          .Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR,
          .AddressU       = D3D11_TEXTURE_ADDRESS_WRAP,
          .AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP,
          .AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP,
          .MaxAnisotropy  = 1,
          .ComparisonFunc = D3D11_COMPARISON_NEVER,
          .MaxLOD         = D3D11_FLOAT32_MAX,
        };
        CheckDeviceResult(D3D.CreateSamplerState(&Sampler, &_WrapSampler), "ID3D11Device::CreateSamplerState");
    }

    void LiveRenderer::Render(ID3D11RenderTargetView& Target,
                              const Size& Extent,
                              float PixelsPerDip,
                              const LiveRecording& Recording) {
        ID3D11DeviceContext& Context = *_Device.GetD3DContext();

        const D3D11_VIEWPORT Viewport {0.0f,
                                       0.0f,
                                       Extent.Width * PixelsPerDip,
                                       Extent.Height * PixelsPerDip,
                                       0.0f,
                                       1.0f};
        ID3D11RenderTargetView* const Targets[] = {&Target};
        constexpr float kTransparent[4]         = {0.0f, 0.0f, 0.0f, 0.0f};
        Context.ClearRenderTargetView(&Target, kTransparent);
        Context.OMSetRenderTargets(1, Targets, nullptr);
        Context.OMSetBlendState(_PremultipliedBlend.Get(), nullptr, 0xFFFFFFFF);
        Context.OMSetDepthStencilState(nullptr, 0);
        Context.RSSetViewports(1, &Viewport);
        Context.RSSetState(_Rasterizer.Get());
        Context.IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

        if (!Recording.Instances.empty()) {
            EnsureInstanceCapacity(Recording.Instances.size());
            D3D11_MAPPED_SUBRESOURCE Mapped {};
            CheckDeviceResult(Context.Map(_InstanceBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped),
                              "ID3D11DeviceContext::Map");
            std::memcpy(Mapped.pData, Recording.Instances.data(), Recording.Instances.size() * sizeof(LiveInstance));
            Context.Unmap(_InstanceBuffer.Get(), 0);
        }

        WriteConstants(Context,
                       *_FrameConstants.Get(),
                       FrameConstants {{Extent.Width, Extent.Height}, PixelsPerDip, 0.0f});

        for (const LiveBatch& Batch : Recording.Batches) {
            if (!Batch.IsTexture) {
                BindPrimitivePipeline();
                Context.DrawInstanced(kQuadVertexCount, Batch.InstanceCount, 0, Batch.FirstInstance);
                continue;
            }

            auto& Texture = const_cast<LiveTextureData&>(*Batch.TextureDraw.Texture);
            UploadTexture(Texture);

            const Rect& Destination = Batch.TextureDraw.Destination;
            WriteConstants(Context,
                           *_TextureConstants.Get(),
                           TextureConstants {
                             {Extent.Width, Extent.Height},
                             Batch.TextureDraw.SourceOffset,
                             Batch.TextureDraw.Opacity,
                             {Destination.X, Destination.Y, Destination.Width, Destination.Height},
                           });

            ID3D11Buffer* const Constants[]         = {_TextureConstants.Get()};
            ID3D11ShaderResourceView* const Views[] = {Texture.View.Get()};
            ID3D11SamplerState* const Samplers[]    = {_WrapSampler.Get()};
            Context.IASetInputLayout(nullptr);
            Context.VSSetShader(_TextureVertexShader.Get(), nullptr, 0);
            Context.VSSetConstantBuffers(0, 1, Constants);
            Context.PSSetShader(_TexturePixelShader.Get(), nullptr, 0);
            Context.PSSetConstantBuffers(0, 1, Constants);
            Context.PSSetShaderResources(0, 1, Views);
            Context.PSSetSamplers(0, 1, Samplers);
            Context.Draw(kQuadVertexCount, 0);
        }

        ID3D11ShaderResourceView* const NoViews[] = {nullptr};
        Context.PSSetShaderResources(0, 1, NoViews);
        Context.OMSetRenderTargets(0, nullptr, nullptr);
    }

    void LiveRenderer::EnsureInstanceCapacity(size_t Count) {
        if (Count <= _InstanceCapacity && _InstanceBuffer) { return; }

        size_t Capacity = std::max(_InstanceCapacity, kInitialInstanceCapacity);
        while (Capacity < Count) {
            Capacity *= 2;
        }

        const D3D11_BUFFER_DESC Desc {
          .ByteWidth      = static_cast<UINT>(Capacity * sizeof(LiveInstance)),
          .Usage          = D3D11_USAGE_DYNAMIC,
          .BindFlags      = D3D11_BIND_VERTEX_BUFFER,
          .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
        };
        _InstanceBuffer.Reset();
        CheckDeviceResult(_Device.GetD3DDevice()->CreateBuffer(&Desc, nullptr, &_InstanceBuffer),
                          "ID3D11Device::CreateBuffer");
        _InstanceCapacity = Capacity;
    }

    void LiveRenderer::UploadTexture(LiveTextureData& Texture) {
        ID3D11Device& D3D = *_Device.GetD3DDevice();
        if (!Texture.Texture || Texture.Generation != _Device.GetGeneration()) {
            const D3D11_TEXTURE2D_DESC Desc {
              .Width      = Texture.Width,
              .Height     = Texture.Height,
              .MipLevels  = 1,
              .ArraySize  = 1,
              .Format     = DXGI_FORMAT_B8G8R8A8_UNORM,
              .SampleDesc = {.Count = 1},
              .Usage      = D3D11_USAGE_DEFAULT,
              .BindFlags  = D3D11_BIND_SHADER_RESOURCE,
            };
            Texture.View.Reset();
            Texture.Texture.Reset();
            CheckDeviceResult(D3D.CreateTexture2D(&Desc, nullptr, &Texture.Texture), "ID3D11Device::CreateTexture2D");
            CheckDeviceResult(D3D.CreateShaderResourceView(Texture.Texture.Get(), nullptr, &Texture.View),
                              "ID3D11Device::CreateShaderResourceView");
            Texture.Generation = _Device.GetGeneration();
            Texture.Dirty      = true;
        }

        if (!Texture.Dirty) { return; }
        _Device.GetD3DContext()
          ->UpdateSubresource(Texture.Texture.Get(), 0, nullptr, Texture.Pixels.data(), Texture.Width * 4, 0);
        Texture.Dirty = false;
    }

    void LiveRenderer::BindPrimitivePipeline() {
        ID3D11DeviceContext& Context = *_Device.GetD3DContext();

        ID3D11Buffer* const Buffers[]   = {_InstanceBuffer.Get()};
        const UINT Strides[]            = {sizeof(LiveInstance)};
        const UINT Offsets[]            = {0};
        ID3D11Buffer* const Constants[] = {_FrameConstants.Get()};
        Context.IASetInputLayout(_PrimitiveLayout.Get());
        Context.IASetVertexBuffers(0, 1, Buffers, Strides, Offsets);
        Context.VSSetShader(_PrimitiveVertexShader.Get(), nullptr, 0);
        Context.VSSetConstantBuffers(0, 1, Constants);
        Context.PSSetShader(_PrimitivePixelShader.Get(), nullptr, 0);
        Context.PSSetConstantBuffers(0, 1, Constants);
    }
}  // namespace XenUI
