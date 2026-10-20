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

#include <cstdint>

namespace XenUI {
    /// @brief sRGB-encoded RGBA color with straight (non-premultiplied) alpha.
    struct Color {
        float R = 0.0f;
        float G = 0.0f;
        float B = 0.0f;
        float A = 1.0f;

        /// @brief Creates a color from a packed 0xRRGGBB value.
        static constexpr Color FromHex(uint32_t Rgb, float Alpha = 1.0f) {
            return {
              static_cast<float>((Rgb >> 16) & 0xFF) / 255.0f,
              static_cast<float>((Rgb >> 8) & 0xFF) / 255.0f,
              static_cast<float>(Rgb & 0xFF) / 255.0f,
              Alpha,
            };
        }

        static constexpr Color FromRgba8(uint8_t Red, uint8_t Green, uint8_t Blue, uint8_t Alpha = 255) {
            return {
              static_cast<float>(Red) / 255.0f,
              static_cast<float>(Green) / 255.0f,
              static_cast<float>(Blue) / 255.0f,
              static_cast<float>(Alpha) / 255.0f,
            };
        }

        [[nodiscard]] constexpr Color WithAlpha(float Alpha) const { return {R, G, B, Alpha}; }

        /// @brief Linear blend from this color (Amount 0) to Target (Amount 1).
        [[nodiscard]] constexpr Color Lerp(const Color& Target, float Amount) const {
            return {R + (Target.R - R) * Amount,
                    G + (Target.G - G) * Amount,
                    B + (Target.B - B) * Amount,
                    A + (Target.A - A) * Amount};
        }

        constexpr bool operator==(const Color&) const = default;
    };
}  // namespace XenUI
