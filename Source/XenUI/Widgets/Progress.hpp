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
#include "XenUI/Core/Animation.hpp"
#include "XenUI/Core/Color.hpp"
#include "XenUI/Core/Timer.hpp"
#include "XenUI/Reactive/Signal.hpp"
#include "XenUI/Tree/Widget.hpp"

#include <optional>
#include <string>

namespace XenUI {
    /// @brief A horizontal bar showing how far a task has progressed. With no Value it is indeterminate: a segment
    /// slides along the track to show that work is happening, or holds still when Windows animations are off.
    struct ProgressBar {
        /// @brief Progress between Min and Max, or nothing while the amount of work is unknown.
        std::optional<float> Value;
        float Min    = 0.0f;
        float Max    = 1.0f;
        float Height = 6.0f;
        /// @brief Fill color; the theme's accent when unset.
        std::optional<Color> Fill;
        /// @brief The name screen readers announce.
        std::string Label;

        class XENUI_API State : public ComponentState<ProgressBar> {
        protected:
            Node Build(BuildContext& Context) override;
            void Dispose() override { _Ticker.Cancel(); }

        private:
            Animated<float> _Fraction;
            Signal<float> _Phase = 0.0f;
            Timer _Ticker;
            bool _Ticking = false;
            bool _Shown   = false;
        };
    };
}  // namespace XenUI
