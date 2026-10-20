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

#include "XenUI/Api.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

namespace XenUI {
    class ImageData;

    enum class ImageInterpolation : uint8_t { NearestNeighbor, Linear, HighQuality };

    /// @brief Immutable bitmap held in system memory and uploaded to the GPU on first draw. Cheap to copy.
    class XENUI_API Image {
    public:
        Image();

        /// @brief Decodes any format supported by Windows Imaging Component (PNG, JPEG, BMP, ...).
        /// @throws Error if the file cannot be read or decoded.
        static Image FromFile(std::string_view FilePath);

        /// @param Pixels 32-bit BGRA rows with straight alpha.
        /// @param Stride Bytes per row, or 0 for tightly packed rows.
        static Image
        FromPixels(uint32_t PixelWidth, uint32_t PixelHeight, std::span<const uint8_t> Pixels, uint32_t Stride = 0);

        [[nodiscard]] bool IsEmpty() const { return _Data == nullptr; }
        [[nodiscard]] uint32_t GetPixelWidth() const;
        [[nodiscard]] uint32_t GetPixelHeight() const;

        /// @internal
        [[nodiscard]] ImageData* GetData() const { return _Data.get(); }

    private:
        explicit Image(std::shared_ptr<ImageData> Data);

        std::shared_ptr<ImageData> _Data;
    };
}  // namespace XenUI
