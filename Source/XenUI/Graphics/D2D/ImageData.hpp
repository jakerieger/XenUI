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

#include <d2d1_1.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <cstdint>

namespace XenUI {
    class ImageData {
    public:
        /// @brief Returns the GPU copy, uploading it if missing or created on a lost device.
        ID2D1Bitmap1* GetBitmap(ID2D1DeviceContext& Context, uint64_t DeviceGeneration);

        Microsoft::WRL::ComPtr<IWICBitmap> Pixels;
        uint32_t Width  = 0;
        uint32_t Height = 0;

    private:
        Microsoft::WRL::ComPtr<ID2D1Bitmap1> _Bitmap;
        uint64_t _BitmapGeneration = 0;
    };
}  // namespace XenUI
