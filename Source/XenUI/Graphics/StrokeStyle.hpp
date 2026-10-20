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
#include <vector>

namespace XenUI {
    enum class LineCap : uint8_t { Flat, Square, Round, Triangle };
    enum class LineJoin : uint8_t { Miter, Bevel, Round, MiterOrBevel };

    struct StrokeStyle {
        float Width      = 1.0f;
        LineCap Cap      = LineCap::Flat;
        LineJoin Join    = LineJoin::Miter;
        float MiterLimit = 10.0f;
        /// @brief Alternating dash and gap lengths, in multiples of Width. Empty for a solid line.
        std::vector<float> Dashes;
        float DashOffset = 0.0f;

        bool operator==(const StrokeStyle&) const = default;
    };
}  // namespace XenUI
