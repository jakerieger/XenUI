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

#include "XenUI/Graphics/GL/GLObjects.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace XenUI {
    class ImageData {
    public:
        /// @brief Tightly packed 32-bit BGRA rows with premultiplied alpha.
        std::vector<uint8_t> Pixels;
        uint32_t Width  = 0;
        uint32_t Height = 0;

        /// @brief Pixels uploaded by the GL renderer on first draw, tagged with the device generation it belongs to.
        mutable GLTexture Texture;
    };

    /// @brief Pixels decoded from an encoded image (PNG, JPEG, BMP, ...), as straight-alpha BGRA rows.
    struct DecodedImage {
        std::vector<uint8_t> Pixels;
        uint32_t Width  = 0;
        uint32_t Height = 0;
    };

    /// @brief Decodes Encoded with stb_image, or returns nothing if it isn't a supported image.
    [[nodiscard]] std::optional<DecodedImage> DecodeImage(std::span<const uint8_t> Encoded);
}  // namespace XenUI
