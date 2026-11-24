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

#include <d2d1_1.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <cstdint>

namespace XenUI {
    struct LayerCache {
        Microsoft::WRL::ComPtr<ID2D1Bitmap1> Bitmap;
        Rect Bounds;
        float Scale          = 0.0f;
        uint32_t PixelWidth  = 0;
        uint32_t PixelHeight = 0;
        uint64_t Generation  = 0;
        bool Dirty           = true;

        [[nodiscard]] bool IsAllocated() const { return Bitmap != nullptr; }
    };

    struct LiveSurface {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> Target;
        Microsoft::WRL::ComPtr<ID2D1Bitmap1> Bitmap;
        uint32_t PixelWidth  = 0;
        uint32_t PixelHeight = 0;
        float Scale          = 0.0f;
        uint64_t Generation  = 0;

        [[nodiscard]] bool IsAllocated() const { return Bitmap != nullptr; }
    };
}  // namespace XenUI
