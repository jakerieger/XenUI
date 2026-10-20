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

#include "XenUI/Core/Color.hpp"
#include "XenUI/Core/Math.hpp"

#include <variant>
#include <vector>

namespace XenUI {
    struct GradientStop {
        /// @brief Position along the gradient, from 0 to 1.
        float Position = 0.0f;
        Color Value;

        constexpr bool operator==(const GradientStop&) const = default;
    };

    enum class GradientExtend : uint8_t { Clamp, Repeat, Mirror };

    struct LinearGradient {
        Vec2 Start;
        Vec2 End;
        std::vector<GradientStop> Stops;
        GradientExtend Extend = GradientExtend::Clamp;
    };

    struct RadialGradient {
        Vec2 Center;
        Vec2 Radius;
        /// @brief Offset of the gradient origin from Center.
        Vec2 FocusOffset;
        std::vector<GradientStop> Stops;
        GradientExtend Extend = GradientExtend::Clamp;
    };

    /// @brief How a shape is filled or stroked. Gradient coordinates are in the local space of the shape being drawn.
    using Brush = std::variant<Color, LinearGradient, RadialGradient>;
}  // namespace XenUI
