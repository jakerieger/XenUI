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

#include "XenUI/Core/FrameStats.hpp"

#include <algorithm>

namespace XenUI {
    void FrameStatistics::Record(const FrameSample& Sample) {
        _Samples[_Next] = Sample;
        _Next           = (_Next + 1) % kCapacity;
        _Count          = std::min(_Count + 1, kCapacity);
        ++_Total;
    }

    std::vector<FrameSample> FrameStatistics::GetRecent(size_t Count) const {
        Count = std::min(Count, _Count);
        std::vector<FrameSample> Result;
        Result.reserve(Count);
        for (size_t i = 0; i < Count; ++i) {
            Result.push_back(_Samples[(_Next + kCapacity - Count + i) % kCapacity]);
        }
        return Result;
    }

    size_t FrameStatistics::CountFramesSince(double Start) const {
        size_t Frames = 0;
        for (size_t i = 0; i < _Count; ++i) {
            if (_Samples[(_Next + kCapacity - 1 - i) % kCapacity].Time < Start) { break; }
            ++Frames;
        }
        return Frames;
    }

    FrameCounters& FrameCounters::Current() {
        thread_local FrameCounters Counters;
        return Counters;
    }
}  // namespace XenUI
