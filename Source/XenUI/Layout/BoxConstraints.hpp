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

#include <algorithm>
#include <cmath>
#include <optional>

namespace XenUI {
    struct EdgeInsets {
        float Left   = 0.0f;
        float Top    = 0.0f;
        float Right  = 0.0f;
        float Bottom = 0.0f;

        static constexpr EdgeInsets All(float Amount) { return {Amount, Amount, Amount, Amount}; }

        static constexpr EdgeInsets Symmetric(float Horizontal, float Vertical) {
            return {Horizontal, Vertical, Horizontal, Vertical};
        }

        [[nodiscard]] constexpr float GetHorizontal() const { return Left + Right; }
        [[nodiscard]] constexpr float GetVertical() const { return Top + Bottom; }
        [[nodiscard]] constexpr bool IsZero() const {
            return Left == 0.0f && Top == 0.0f && Right == 0.0f && Bottom == 0.0f;
        }

        constexpr bool operator==(const EdgeInsets&) const = default;
    };

    /// @brief Point within a box, from (-1, -1) at the top-left corner to (1, 1) at the bottom-right.
    struct Alignment {
        float X = 0.0f;
        float Y = 0.0f;

        static constexpr Alignment TopLeft() { return {-1.0f, -1.0f}; }
        static constexpr Alignment TopCenter() { return {0.0f, -1.0f}; }
        static constexpr Alignment TopRight() { return {1.0f, -1.0f}; }
        static constexpr Alignment CenterLeft() { return {-1.0f, 0.0f}; }
        static constexpr Alignment Center() { return {0.0f, 0.0f}; }
        static constexpr Alignment CenterRight() { return {1.0f, 0.0f}; }
        static constexpr Alignment BottomLeft() { return {-1.0f, 1.0f}; }
        static constexpr Alignment BottomCenter() { return {0.0f, 1.0f}; }
        static constexpr Alignment BottomRight() { return {1.0f, 1.0f}; }

        /// @brief Offset that places a box of size Inner at this alignment inside Outer.
        [[nodiscard]] constexpr Vec2 Place(const Size& Inner, const Size& Outer) const {
            return {(Outer.Width - Inner.Width) * (X + 1.0f) * 0.5f, (Outer.Height - Inner.Height) * (Y + 1.0f) * 0.5f};
        }

        constexpr bool operator==(const Alignment&) const = default;
    };

    /// @brief Allowed sizes for a render box. Parents pass constraints down; children pick a size within them.
    struct BoxConstraints {
        float MinWidth  = 0.0f;
        float MaxWidth  = kUnbounded;
        float MinHeight = 0.0f;
        float MaxHeight = kUnbounded;

        static constexpr BoxConstraints Tight(const Size& Exact) {
            return {Exact.Width, Exact.Width, Exact.Height, Exact.Height};
        }

        static constexpr BoxConstraints Loose(const Size& Maximum) {
            return {0.0f, Maximum.Width, 0.0f, Maximum.Height};
        }

        /// @brief Tight on each axis that has a value, unconstrained on the others.
        static constexpr BoxConstraints TightFor(std::optional<float> Width, std::optional<float> Height) {
            return {
              Width.value_or(0.0f),
              Width.value_or(kUnbounded),
              Height.value_or(0.0f),
              Height.value_or(kUnbounded),
            };
        }

        [[nodiscard]] constexpr bool IsTight() const { return MinWidth >= MaxWidth && MinHeight >= MaxHeight; }
        [[nodiscard]] bool HasBoundedWidth() const { return std::isfinite(MaxWidth); }
        [[nodiscard]] bool HasBoundedHeight() const { return std::isfinite(MaxHeight); }

        [[nodiscard]] constexpr Size Constrain(const Size& Desired) const {
            return {std::clamp(Desired.Width, MinWidth, std::max(MinWidth, MaxWidth)),
                    std::clamp(Desired.Height, MinHeight, std::max(MinHeight, MaxHeight))};
        }

        /// @brief Largest allowed size, treating unbounded axes as their minimum.
        [[nodiscard]] Size GetBiggest() const {
            return {HasBoundedWidth() ? MaxWidth : MinWidth, HasBoundedHeight() ? MaxHeight : MinHeight};
        }

        [[nodiscard]] constexpr Size GetSmallest() const { return {MinWidth, MinHeight}; }

        [[nodiscard]] constexpr BoxConstraints Loosen() const { return {0.0f, MaxWidth, 0.0f, MaxHeight}; }

        [[nodiscard]] constexpr BoxConstraints Deflate(const EdgeInsets& Insets) const {
            const float Horizontal   = Insets.GetHorizontal();
            const float Vertical     = Insets.GetVertical();
            const float NewMinWidth  = std::max(0.0f, MinWidth - Horizontal);
            const float NewMinHeight = std::max(0.0f, MinHeight - Vertical);
            return {
              NewMinWidth,
              std::max(NewMinWidth, MaxWidth - Horizontal),
              NewMinHeight,
              std::max(NewMinHeight, MaxHeight - Vertical),
            };
        }

        /// @brief These constraints, clamped so they also satisfy Outer.
        [[nodiscard]] constexpr BoxConstraints Enforce(const BoxConstraints& Outer) const {
            return {
              std::clamp(MinWidth, Outer.MinWidth, Outer.MaxWidth),
              std::clamp(MaxWidth, Outer.MinWidth, Outer.MaxWidth),
              std::clamp(MinHeight, Outer.MinHeight, Outer.MaxHeight),
              std::clamp(MaxHeight, Outer.MinHeight, Outer.MaxHeight),
            };
        }

        constexpr bool operator==(const BoxConstraints&) const = default;
    };
}  // namespace XenUI
