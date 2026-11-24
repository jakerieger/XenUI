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

#include "XenUI/Core/Math.hpp"
#include "XenUI/Graphics/LiveRecording.hpp"

#include <d3d11.h>
#include <wrl/client.h>

#include <cstdint>

namespace XenUI {
    class GraphicsDevice;

    /// @brief Draws LiveRecordings with Direct3D 11: one instanced draw per run of primitives, one quad per texture.
    class LiveRenderer {
    public:
        explicit LiveRenderer(GraphicsDevice& Device);

        LiveRenderer(const LiveRenderer&)            = delete;
        LiveRenderer& operator=(const LiveRenderer&) = delete;

        /// @param Target Render target the size of Extent multiplied by PixelsPerDip.
        void
        Render(ID3D11RenderTargetView& Target, const Size& Extent, float PixelsPerDip, const LiveRecording& Recording);

    private:
        void EnsureInstanceCapacity(size_t Count);
        void UploadTexture(LiveTextureData& Texture);
        void BindPrimitivePipeline();

        GraphicsDevice& _Device;
        Microsoft::WRL::ComPtr<ID3D11VertexShader> _PrimitiveVertexShader;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> _PrimitivePixelShader;
        Microsoft::WRL::ComPtr<ID3D11InputLayout> _PrimitiveLayout;
        Microsoft::WRL::ComPtr<ID3D11VertexShader> _TextureVertexShader;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> _TexturePixelShader;
        Microsoft::WRL::ComPtr<ID3D11Buffer> _InstanceBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer> _FrameConstants;
        Microsoft::WRL::ComPtr<ID3D11Buffer> _TextureConstants;
        Microsoft::WRL::ComPtr<ID3D11BlendState> _PremultipliedBlend;
        Microsoft::WRL::ComPtr<ID3D11RasterizerState> _Rasterizer;
        Microsoft::WRL::ComPtr<ID3D11SamplerState> _WrapSampler;
        size_t _InstanceCapacity = 0;
    };
}  // namespace XenUI
