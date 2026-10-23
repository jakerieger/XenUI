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

#include "XenUI/Widgets/Progress.hpp"
#include "XenUI/Widgets/Basic.hpp"
#include "XenUI/Widgets/Semantics.hpp"
#include "XenUI/Widgets/Theme.hpp"

#include <algorithm>
#include <cmath>

namespace XenUI {
    namespace {
        constexpr double kFillDuration   = 0.2;
        constexpr double kSweepSeconds   = 1.4;
        constexpr double kTickInterval   = 1.0 / 60.0;
        constexpr float kSegmentFraction = 0.3f;
    }  // namespace

    Node ProgressBar::State::Build(BuildContext& Context) {
        const Theme& Style       = Theme::Of(Context);
        const ProgressBar& Props = GetWidget();
        const float Low          = std::min(Props.Min, Props.Max);
        const float High         = std::max(Props.Min, Props.Max);
        const bool Indeterminate = !Props.Value.has_value();
        const bool Animating     = Indeterminate && AnimationTicker::AreAnimationsEnabled();

        if (Animating && !_Ticking) {
            _Ticker  = Timer::Every(kTickInterval, [this] {
                _Phase = static_cast<float>(std::fmod(AnimationTicker::Now() / kSweepSeconds, 1.0));
            });
            _Ticking = true;
        } else if (!Animating && _Ticking) {
            _Ticker.Cancel();
            _Ticking = false;
        }

        if (!Indeterminate) {
            const float Fraction = High > Low ? std::clamp((*Props.Value - Low) / (High - Low), 0.0f, 1.0f) : 0.0f;
            if (_Shown) {
                _Fraction.AnimateTo(Fraction, kFillDuration);
            } else {
                _Fraction.Set(Fraction);
            }
        }
        _Shown = true;

        const Color FillColor  = Props.Fill.value_or(Style.Accent);
        const Color TrackColor = Style.Track;
        const Node Bar         = CustomPaint {
          .OnPaint =
            [this, Indeterminate, Animating, FillColor, TrackColor](Painter& Canvas, const Size& Extent) {
                const float Radius = Extent.Height * 0.5f;
                Canvas.FillRoundedRect({0.0f, 0.0f, Extent.Width, Extent.Height}, Radius, TrackColor);
                if (!Indeterminate) {
                    const float Width = Extent.Width * _Fraction.Get();
                    if (Width > 0.5f) {
                        Canvas.FillRoundedRect({0.0f, 0.0f, std::max(Width, Extent.Height), Extent.Height},
                                               Radius,
                                               FillColor);
                    }
                    return;
                }
                const float Segment = Extent.Width * kSegmentFraction;
                const float Start =
                  Animating ? -Segment + (Extent.Width + Segment) * _Phase.Get() : (Extent.Width - Segment) * 0.5f;
                const float Left  = std::max(0.0f, Start);
                const float Right = std::min(Extent.Width, Start + Segment);
                if (Right - Left > 0.5f) {
                    Canvas.FillRoundedRect({Left, 0.0f, Right - Left, Extent.Height}, Radius, FillColor);
                }
            },
          .Height = Props.Height,
        };

        return Semantics {
          .Role     = SemanticsRole::ProgressBar,
          .Label    = Props.Label,
          .Range    = Indeterminate ? std::nullopt
                                    : std::optional<SemanticsRange>(
                                        SemanticsRange {std::clamp(*Props.Value, Low, High), Low, High, 0.0f}),
          .ReadOnly = true,
          .Child    = Bar,
        };
    }
}  // namespace XenUI
