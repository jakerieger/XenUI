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
#include "XenUI/Graphics/Path.hpp"

#include <cstdint>
#include <vector>

namespace XenUI {
    /// @brief Path geometry flattened to polylines on the CPU, in the path's own coordinate space.
    class PathData {
    public:
        /// @brief How far a flattened curve may stray from the true curve, in path units. Icons are authored on a
        /// 24-unit grid and drawn at a few times that size, so this stays below a hundredth of a device pixel there.
        static constexpr float kTolerance = 0.01f;

        struct Figure {
            std::vector<Vec2> Points;
            bool Closed = false;
        };

        std::vector<Figure> Figures;
        FillRule Rule = FillRule::NonZero;
        Rect Bounds;
        /// @brief Unique per built path, so renderer caches can key on it without keeping the path alive.
        uint64_t Id = 0;
    };
}  // namespace XenUI
