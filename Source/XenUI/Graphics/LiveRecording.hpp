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

#include <d3d11.h>
#include <wrl/client.h>

#include <cstdint>
#include <vector>

namespace XenUI {
    enum class LivePrimitiveKind : uint32_t { Line = 0, Rect = 1 };

    /// @brief Per-instance vertex data; layout must match the input layout and LivePrimitives.hlsl.
    struct LiveInstance {
        float Start[2];
        float End[2];
        float Color[4];
        float Width;
        float Kind;
    };

    struct LiveTextureDraw {
        const class LiveTextureData* Texture = nullptr;
        Rect Destination;
        float SourceOffset = 0.0f;
        float Opacity      = 1.0f;
    };

    /// @brief A run of primitives, or a single texture draw, in recording order.
    struct LiveBatch {
        bool IsTexture         = false;
        uint32_t FirstInstance = 0;
        uint32_t InstanceCount = 0;
        LiveTextureDraw TextureDraw;
    };

    class LiveRecording {
    public:
        void Clear() {
            Instances.clear();
            Batches.clear();
        }

        void AddInstance(const LiveInstance& Instance) {
            if (Batches.empty() || Batches.back().IsTexture) {
                Batches.push_back({.FirstInstance = static_cast<uint32_t>(Instances.size())});
            }
            Instances.push_back(Instance);
            ++Batches.back().InstanceCount;
        }

        void AddTexture(const LiveTextureDraw& Draw) { Batches.push_back({.IsTexture = true, .TextureDraw = Draw}); }

        std::vector<LiveInstance> Instances;
        std::vector<LiveBatch> Batches;
    };

    class LiveTextureData {
    public:
        LiveTextureData(uint32_t TextureWidth, uint32_t TextureHeight)
            : Width(TextureWidth), Height(TextureHeight), Pixels(static_cast<size_t>(TextureWidth) * TextureHeight, 0) {
        }

        uint32_t Width;
        uint32_t Height;
        std::vector<uint32_t> Pixels;
        bool Dirty = true;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> View;
        uint64_t Generation = 0;
    };
}  // namespace XenUI
