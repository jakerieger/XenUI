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

#include <vector>

namespace XenUI {
    /// @brief Small set of rectangles approximating the area that needs repainting. Merges rectangles once there
    /// are more than a handful, trading some overdraw for fewer clip/replay passes.
    class DamageRegion {
    public:
        void Add(const Rect& Area);
        void AddAll();
        void Merge(const DamageRegion& Other);
        void Clear();

        [[nodiscard]] bool IsEmpty() const { return !_Full && _Rects.empty(); }
        [[nodiscard]] bool IsFull() const { return _Full; }
        [[nodiscard]] const std::vector<Rect>& GetRects() const { return _Rects; }

    private:
        void MergeClosestPair();

        std::vector<Rect> _Rects;
        bool _Full = false;
    };
}  // namespace XenUI
