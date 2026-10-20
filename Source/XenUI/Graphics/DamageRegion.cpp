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

#include "XenUI/Graphics/DamageRegion.hpp"

#include <algorithm>
#include <limits>

namespace XenUI {
    namespace {
        constexpr size_t kMaxRects = 8;
    }  // namespace

    void DamageRegion::Add(const Rect& Area) {
        if (_Full || Area.IsEmpty()) { return; }
        if (std::any_of(_Rects.begin(), _Rects.end(), [&Area](const Rect& Existing) {
                return Existing.ContainsRect(Area);
            })) {
            return;
        }

        std::erase_if(_Rects, [&Area](const Rect& Existing) { return Area.ContainsRect(Existing); });
        _Rects.push_back(Area);
        while (_Rects.size() > kMaxRects) {
            MergeClosestPair();
        }
    }

    void DamageRegion::AddAll() {
        _Full = true;
        _Rects.clear();
    }

    void DamageRegion::Merge(const DamageRegion& Other) {
        if (Other._Full) {
            AddAll();
            return;
        }
        for (const Rect& Area : Other._Rects) {
            Add(Area);
        }
    }

    void DamageRegion::Clear() {
        _Full = false;
        _Rects.clear();
    }

    void DamageRegion::MergeClosestPair() {
        size_t BestFirst  = 0;
        size_t BestSecond = 1;
        float BestCost    = std::numeric_limits<float>::max();

        for (size_t i = 0; i < _Rects.size(); ++i) {
            for (size_t j = i + 1; j < _Rects.size(); ++j) {
                const float Cost = _Rects[i].Union(_Rects[j]).Area() - _Rects[i].Area() - _Rects[j].Area();
                if (Cost < BestCost) {
                    BestCost   = Cost;
                    BestFirst  = i;
                    BestSecond = j;
                }
            }
        }

        _Rects[BestFirst] = _Rects[BestFirst].Union(_Rects[BestSecond]);
        _Rects.erase(_Rects.begin() + static_cast<std::ptrdiff_t>(BestSecond));
    }
}  // namespace XenUI
