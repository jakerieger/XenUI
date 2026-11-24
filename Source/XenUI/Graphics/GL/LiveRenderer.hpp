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

#include <cstdint>

namespace XenUI {
    /// @brief Draws LiveRecordings with OpenGL: one instanced draw per run of primitives, one quad per texture. A
    /// GLSL port of Graphics/D2D/Shaders/LivePrimitives.hlsl and LiveTexture.hlsl.
    class LiveRenderer {
    public:
        explicit LiveRenderer(uint64_t Generation);

        LiveRenderer(const LiveRenderer&)            = delete;
        LiveRenderer& operator=(const LiveRenderer&) = delete;

        /// @param Framebuffer Target the size of Extent multiplied by PixelsPerDip.
        void Render(uint32_t Framebuffer, const Size& Extent, float PixelsPerDip, const LiveRecording& Recording);

    private:
        void UploadTexture(LiveTextureData& Texture);

        uint64_t _Generation;
        uint32_t _PrimitiveProgram = 0;
        uint32_t _TextureProgram   = 0;
        uint32_t _PrimitiveArray   = 0;
        uint32_t _TextureArray     = 0;
        uint32_t _Instances        = 0;
        size_t _InstanceCapacity   = 0;
        int32_t _PrimitiveViewport = -1;
        int32_t _PrimitiveScale    = -1;
        int32_t _TextureViewport   = -1;
        int32_t _TextureDestination = -1;
        int32_t _TextureOffset     = -1;
        int32_t _TextureOpacity    = -1;
    };
}  // namespace XenUI
