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

#include "XenUI/Render/RenderControls.hpp"
#include "XenUI/Render/Semantics.hpp"

#include <algorithm>
#include <cmath>

namespace XenUI {
    namespace {
        constexpr float kSliderThickness   = 20.0f;
        constexpr float kSliderDefaultSpan = 160.0f;
        constexpr float kTrackThickness    = 4.0f;
        constexpr float kThumbRadius       = 7.0f;
        constexpr float kWheelFraction     = 0.02f;
        constexpr float kFineFactor        = 0.1f;
    }  // namespace

    RenderSlider::RenderSlider() {
        SetBehavior(HitTestBehavior::Opaque);
    }

    void RenderSlider::SetConfiguration(Configuration NewConfiguration) {
        const bool DirectionChanged = NewConfiguration.Direction != _Configuration.Direction;
        _Configuration              = std::move(NewConfiguration);
        if (DirectionChanged) { MarkNeedsLayout(); }
        MarkNeedsPaint();
    }

    bool RenderSlider::HandlePointer(const PointerEvent& Event) {
        if (!_Configuration.Enabled) { return false; }

        switch (Event.Phase) {
            case PointerPhase::Down:
                if (Event.Button != MouseButton::Primary) { return false; }
                _Dragging = true;
                CommitFromPosition(Event.LocalPosition);
                return true;
            case PointerPhase::Move:
                if (_Dragging) { CommitFromPosition(Event.LocalPosition); }
                return _Dragging;
            case PointerPhase::Up:
            case PointerPhase::Cancel:
                _Dragging = false;
                return false;
            case PointerPhase::Wheel: {
                const float Range = _Configuration.Max - _Configuration.Min;
                const float Step =
                  Range * kWheelFraction * (HasFlags(Event.Modifiers, KeyModifiers::Shift) ? kFineFactor : 1.0f);
                const float Next = std::clamp(_Configuration.Source.Peek() + Event.WheelDelta.Y * Step,
                                              std::min(_Configuration.Min, _Configuration.Max),
                                              std::max(_Configuration.Min, _Configuration.Max));
                _Configuration.Source.Commit(Next);
                return true;
            }
            default:
                return false;
        }
    }

    void RenderSlider::Paint(PaintContext& Context) {
        float Value = 0.0f;
        {
            TrackingScope Scope(*this);
            Value = _Configuration.Source.Get();
        }

        const Size& Extent     = GetSize();
        const bool Horizontal  = _Configuration.Direction == Axis::Horizontal;
        const float Fraction   = GetFraction(Value);
        const float Span       = (Horizontal ? Extent.Width : Extent.Height) - kThumbRadius * 2.0f;
        const float Center     = (Horizontal ? Extent.Height : Extent.Width) * 0.5f;
        const float ThumbAlong = kThumbRadius + Span * (Horizontal ? Fraction : 1.0f - Fraction);
        const Color Fill       = _Configuration.Enabled ? _Configuration.FillColor : _Configuration.TrackColor;

        Painter& Canvas = Context.GetPainter();
        if (Horizontal) {
            const Rect Track {kThumbRadius, Center - kTrackThickness * 0.5f, Span, kTrackThickness};
            Canvas.FillRoundedRect(Track, kTrackThickness * 0.5f, _Configuration.TrackColor);
            Canvas.FillRoundedRect({Track.X, Track.Y, ThumbAlong - kThumbRadius, Track.Height},
                                   kTrackThickness * 0.5f,
                                   Fill);
            Canvas.FillCircle({ThumbAlong, Center}, kThumbRadius, _Configuration.ThumbColor);
        } else {
            const Rect Track {Center - kTrackThickness * 0.5f, kThumbRadius, kTrackThickness, Span};
            Canvas.FillRoundedRect(Track, kTrackThickness * 0.5f, _Configuration.TrackColor);
            Canvas.FillRoundedRect({Track.X, ThumbAlong, Track.Width, Track.Bottom() - ThumbAlong},
                                   kTrackThickness * 0.5f,
                                   Fill);
            Canvas.FillCircle({Center, ThumbAlong}, kThumbRadius, _Configuration.ThumbColor);
        }
    }

    bool RenderSlider::DescribeSemantics(SemanticsProperties& Properties) const {
        const float Low  = std::min(_Configuration.Min, _Configuration.Max);
        const float High = std::max(_Configuration.Min, _Configuration.Max);
        Properties       = {
                .Role       = SemanticsRole::Slider,
                .Label      = _Configuration.Label,
                .Range      = SemanticsRange {_Configuration.Source.Peek(), Low, High, (High - Low) * 0.05f},
                .Enabled    = _Configuration.Enabled,
                .OnSetRange = [Source = _Configuration.Source](float Value) { Source.Commit(Value); },
        };
        return true;
    }

    std::optional<CursorShape> RenderSlider::GetCursor() const {
        return _Configuration.Enabled ? std::optional<CursorShape>(CursorShape::Hand) : std::nullopt;
    }

    void RenderSlider::PerformLayout() {
        const bool Horizontal             = _Configuration.Direction == Axis::Horizontal;
        const BoxConstraints& Constraints = GetConstraints();
        const float Along = Horizontal ? (Constraints.HasBoundedWidth() ? Constraints.MaxWidth : kSliderDefaultSpan)
                                       : (Constraints.HasBoundedHeight() ? Constraints.MaxHeight : kSliderDefaultSpan);
        SetSize(Constraints.Constrain(Horizontal ? Size {Along, kSliderThickness} : Size {kSliderThickness, Along}));
    }

    bool RenderSlider::HitTestSelf(const Vec2& Position) const {
        (void)Position;
        return true;
    }

    float RenderSlider::GetFraction(float Value) const {
        const float Range = _Configuration.Max - _Configuration.Min;
        return Range == 0.0f ? 0.0f : std::clamp((Value - _Configuration.Min) / Range, 0.0f, 1.0f);
    }

    void RenderSlider::CommitFromPosition(const Vec2& Local) {
        const bool Horizontal = _Configuration.Direction == Axis::Horizontal;
        const float Span      = (Horizontal ? GetSize().Width : GetSize().Height) - kThumbRadius * 2.0f;
        if (Span <= 0.0f) { return; }

        const float Along    = (Horizontal ? Local.X : Local.Y) - kThumbRadius;
        const float Position = std::clamp(Along / Span, 0.0f, 1.0f);
        const float Fraction = Horizontal ? Position : 1.0f - Position;
        _Configuration.Source.Commit(_Configuration.Min + (_Configuration.Max - _Configuration.Min) * Fraction);
    }

    void RenderSplit::SetConfiguration(const Configuration& NewConfiguration) {
        const bool LayoutChanged = NewConfiguration.Ratio != _Configuration.Ratio ||
                                   NewConfiguration.Direction != _Configuration.Direction ||
                                   NewConfiguration.MinFirst != _Configuration.MinFirst ||
                                   NewConfiguration.MinSecond != _Configuration.MinSecond;
        _Configuration = NewConfiguration;
        if (LayoutChanged) { MarkNeedsLayout(); }
        MarkNeedsPaint();
    }

    bool RenderSplit::HitTest(HitTestResult& Result, const Vec2& Position) {
        if (!Rect {0.0f, 0.0f, GetSize().Width, GetSize().Height}.Contains(Position)) { return false; }
        if (GetDividerRect().Contains(Position)) {
            Result.Add(*this, Position);
            return true;
        }
        return HitTestChildren(Result, Position);
    }

    bool RenderSplit::HandlePointer(const PointerEvent& Event) {
        switch (Event.Phase) {
            case PointerPhase::Down: {
                if (Event.Button != MouseButton::Primary) { return false; }
                const float Along = IsHorizontal() ? Event.LocalPosition.X : Event.LocalPosition.Y;
                _Grab             = Along - _FirstExtent;
                MarkNeedsPaint();
                return true;
            }
            case PointerPhase::Move: {
                if (!_Grab || _Configuration.Ratio == nullptr || _Available <= 0.0f) { return false; }
                const float Along     = IsHorizontal() ? Event.LocalPosition.X : Event.LocalPosition.Y;
                *_Configuration.Ratio = std::clamp((Along - *_Grab) / _Available, 0.0f, 1.0f);
                return true;
            }
            case PointerPhase::Up:
            case PointerPhase::Cancel:
                _Grab.reset();
                MarkNeedsPaint();
                return false;
            default:
                return false;
        }
    }

    void RenderSplit::HandleHoverEnter(const PointerEvent& Event) {
        (void)Event;
        _Hovered = true;
        MarkNeedsPaint();
    }

    void RenderSplit::HandleHoverExit(const PointerEvent& Event) {
        (void)Event;
        _Hovered = false;
        MarkNeedsPaint();
    }

    std::optional<CursorShape> RenderSplit::GetCursor() const {
        return IsHorizontal() ? CursorShape::ResizeHorizontal : CursorShape::ResizeVertical;
    }

    void RenderSplit::Paint(PaintContext& Context) {
        RenderBox::Paint(Context);

        const Rect Divider  = GetDividerRect();
        const bool Active   = _Hovered || _Grab.has_value();
        const Color Line    = Active ? _Configuration.ActiveColor : _Configuration.DividerColor;
        const float Weight  = Active ? 2.0f : 1.0f;
        const Rect LineRect = IsHorizontal() ? Rect {Divider.Center().X - Weight * 0.5f, 0.0f, Weight, Divider.Height}
                                             : Rect {0.0f, Divider.Center().Y - Weight * 0.5f, Divider.Width, Weight};
        Context.GetPainter().FillRect(LineRect, Line);
    }

    void RenderSplit::PerformLayout() {
        const BoxConstraints& Constraints = GetConstraints();
        SetSize(Constraints.Constrain(Constraints.GetBiggest()));
        const Size& Extent = GetSize();

        float Ratio = 0.5f;
        if (_Configuration.Ratio != nullptr) {
            TrackingScope Scope(*this);
            Ratio = _Configuration.Ratio->Get();
        }

        const float Total = IsHorizontal() ? Extent.Width : Extent.Height;
        const float Cross = IsHorizontal() ? Extent.Height : Extent.Width;
        _Available        = std::max(0.0f, Total - kDividerThickness);

        const float Lower        = std::min(_Configuration.MinFirst, _Available);
        const float Upper        = std::max(Lower, _Available - _Configuration.MinSecond);
        _FirstExtent             = std::clamp(Ratio * _Available, Lower, Upper);
        const float SecondExtent = _Available - _FirstExtent;

        const auto MakeSize   = [&](float Along) { return IsHorizontal() ? Size {Along, Cross} : Size {Cross, Along}; };
        const auto MakeOffset = [&](float Along) { return IsHorizontal() ? Vec2 {Along, 0.0f} : Vec2 {0.0f, Along}; };

        if (GetChildCount() > 0) {
            GetChild(0).Layout(BoxConstraints::Tight(MakeSize(_FirstExtent)));
            GetChild(0).SetOffset({});
        }
        if (GetChildCount() > 1) {
            GetChild(1).Layout(BoxConstraints::Tight(MakeSize(SecondExtent)));
            GetChild(1).SetOffset(MakeOffset(_FirstExtent + kDividerThickness));
        }
    }

    Rect RenderSplit::GetDividerRect() const {
        const Size& Extent = GetSize();
        return IsHorizontal() ? Rect {_FirstExtent, 0.0f, kDividerThickness, Extent.Height}
                              : Rect {0.0f, _FirstExtent, Extent.Width, kDividerThickness};
    }
}  // namespace XenUI
