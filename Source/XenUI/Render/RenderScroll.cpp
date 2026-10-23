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

#include "XenUI/Render/RenderScroll.hpp"

#include <algorithm>
#include <cmath>

namespace XenUI {
    namespace {
        constexpr float kMinimumThumb   = 24.0f;
        constexpr float kScrollbarInset = 2.0f;

    }  // namespace

    void ScrollController::SetOffset(float Offset) {
        _Offset = std::clamp(Offset, 0.0f, _MaxOffset.Peek());
    }

    void ScrollController::EnsureVisible(float Start, float End) {
        const float Current  = _Offset.Peek();
        const float Viewport = _ViewportExtent.Peek();
        if (Start < Current) {
            SetOffset(Start);
        } else if (End > Current + Viewport) {
            SetOffset(End - Viewport);
        }
    }

    void ScrollController::RequestVisible(float Start, float End) {
        const float Current  = _Offset.Peek();
        const float Viewport = _ViewportExtent.Peek();
        if (Start < Current) {
            RequestOffset(Start);
        } else if (End > Current + Viewport) {
            RequestOffset(End - Viewport);
        }
    }

    void ScrollController::UpdateMetrics(float ViewportExtent, float ContentExtent) {
        Batch([&] {
            _ViewportExtent = ViewportExtent;
            _MaxOffset      = std::max(0.0f, ContentExtent - ViewportExtent);
            SetOffset(_Offset.Peek());
        });
    }

    void RenderClipRect::Paint(PaintContext& Context) {
        GetLayer()->SetClip(Rect {0.0f, 0.0f, GetSize().Width, GetSize().Height});
        RenderBox::Paint(Context);
    }

    RenderScrollable::RenderScrollable() {
        SetBehavior(HitTestBehavior::Opaque);
    }

    void RenderScrollable::SetController(ScrollController* External) {
        ScrollController* const Next = External != nullptr ? External : &_InternalController;
        if (Next == _Controller) { return; }
        _Controller = Next;
        MarkNeedsLayout();
    }

    void RenderScrollable::SetDirection(Axis Direction) {
        if (Direction == _Direction) { return; }
        _Direction = Direction;
        MarkNeedsLayout();
    }

    void RenderScrollable::SetScrollbarColors(const Color& Thumb, const Color& Track) {
        if (Thumb == _ThumbColor && Track == _TrackColor) { return; }
        _ThumbColor = Thumb;
        _TrackColor = Track;
        MarkNeedsPaint();
    }

    void RenderScrollable::SetShowScrollbar(bool ShowScrollbar) {
        if (ShowScrollbar == _ShowScrollbar) { return; }
        _ShowScrollbar = ShowScrollbar;
        MarkNeedsPaint();
    }

    bool RenderScrollable::HitTest(HitTestResult& Result, const Vec2& Position) {
        if (HasScrollbar() && GetTrackRect().Inflated(2.0f).Contains(Position)) {
            Result.Add(*this, Position);
            return true;
        }
        return RenderInteractive::HitTest(Result, Position);
    }

    bool RenderScrollable::HandlePointer(const PointerEvent& Event) {
        switch (Event.Phase) {
            case PointerPhase::Wheel: {
                if (!CanScroll()) { return false; }
                const bool Shifted = HasFlags(Event.Modifiers, KeyModifiers::Shift);
                float Notches      = 0.0f;
                if (IsVertical()) {
                    Notches = Shifted ? 0.0f : Event.WheelDelta.Y;
                } else {
                    Notches = Event.WheelDelta.X != 0.0f ? -Event.WheelDelta.X : (Shifted ? Event.WheelDelta.Y : 0.0f);
                }
                if (Notches == 0.0f) { return false; }
                const float Before = _Controller->PeekOffset();
                _Controller->ScrollBy(-Notches * kWheelStep);
                return _Controller->PeekOffset() != Before;
            }
            case PointerPhase::Down: {
                if (!HasScrollbar() || Event.Button != MouseButton::Primary ||
                    !GetTrackRect().Inflated(2.0f).Contains(Event.LocalPosition)) {
                    return false;
                }
                const Rect Thumb      = GetThumbRect();
                const float Pointer   = GetMainPosition(Event.LocalPosition);
                const float ThumbFrom = IsVertical() ? Thumb.Top() : Thumb.Left();
                const float ThumbTo   = IsVertical() ? Thumb.Bottom() : Thumb.Right();
                if (Pointer >= ThumbFrom && Pointer < ThumbTo) {
                    _ThumbGrab = Pointer - ThumbFrom;
                } else {
                    const float Page = GetMain(GetSize()) * 0.9f;
                    _Controller->ScrollBy(Pointer < ThumbFrom ? -Page : Page);
                }
                return true;
            }
            case PointerPhase::Move: {
                if (!_ThumbGrab) { return false; }
                const Rect Track     = GetTrackRect();
                const Rect Thumb     = GetThumbRect();
                const float Travel   = GetMain(Track.GetSize()) - GetMain(Thumb.GetSize());
                const float TrackTop = IsVertical() ? Track.Top() : Track.Left();
                const float Start    = GetMainPosition(Event.LocalPosition) - *_ThumbGrab - TrackTop;
                if (Travel > 0.0f) { _Controller->SetOffset(Start / Travel * _Controller->GetMaxOffset()); }
                return true;
            }
            case PointerPhase::Up:
            case PointerPhase::Cancel:
                _ThumbGrab.reset();
                return false;
            default:
                return false;
        }
    }

    void RenderScrollable::Paint(PaintContext& Context) {
        float Offset = 0.0f;
        {
            TrackingScope Scope(*this);
            Offset = _Controller->GetOffset();
        }
        const float Clamped = std::clamp(Offset, 0.0f, std::max(0.0f, _ContentExtent - GetMain(GetSize())));
        if (Clamped != Offset) {
            _Controller->SetOffset(Clamped);
            Offset = Clamped;
        }

        PaintContent(Context, Offset);

        if (HasScrollbar()) {
            Painter& Canvas = Context.GetPainter();
            Canvas.FillRoundedRect(GetTrackRect(), kScrollbarWidth * 0.5f, _TrackColor);
            Canvas.FillRoundedRect(GetThumbRect(), kScrollbarWidth * 0.5f, _ThumbColor);
        }
        GetLayer()->SetClip(Rect {0.0f, 0.0f, GetSize().Width, GetSize().Height});
    }

    void RenderScrollable::OnDependencyChanged() {
        OnOffsetChanged();
    }

    void RenderScrollable::PerformLayout() {
        const BoxConstraints& Constraints = GetConstraints();
        const bool BoundedMain  = IsVertical() ? Constraints.HasBoundedHeight() : Constraints.HasBoundedWidth();
        const bool BoundedCross = IsVertical() ? Constraints.HasBoundedWidth() : Constraints.HasBoundedHeight();
        const float MainLimit   = IsVertical() ? Constraints.MaxHeight : Constraints.MaxWidth;
        const float CrossLimit  = IsVertical() ? Constraints.MaxWidth : Constraints.MaxHeight;

        const auto MakeSize = [&](float Main, float Cross) {
            return IsVertical() ? Size {Cross, Main} : Size {Main, Cross};
        };
        const float CrossExtent = BoundedCross ? CrossLimit : kUnbounded;

        const float UsedOffset = _Controller->PeekOffset();
        const Size Content     = LayoutContent(MakeSize(BoundedMain ? MainLimit : kUnbounded, CrossExtent), UsedOffset);
        _ContentExtent         = GetMain(Content);

        const float MainExtent = BoundedMain ? MainLimit : _ContentExtent;
        SetSize(Constraints.Constrain(MakeSize(MainExtent, BoundedCross ? CrossLimit : GetCross(Content))));
        _Controller->UpdateMetrics(GetMain(GetSize()), _ContentExtent);

        if (_Controller->PeekOffset() != UsedOffset) {
            LayoutContent(MakeSize(GetMain(GetSize()), CrossExtent), _Controller->PeekOffset());
        }
    }

    bool RenderScrollable::CanScroll() const {
        return _ContentExtent > GetMain(GetSize()) + 0.5f;
    }

    bool RenderScrollable::HasScrollbar() const {
        return _ShowScrollbar && CanScroll();
    }

    Rect RenderScrollable::GetTrackRect() const {
        const Size& Extent = GetSize();
        if (IsVertical()) {
            return {Extent.Width - kScrollbarWidth - kScrollbarInset,
                    kScrollbarInset,
                    kScrollbarWidth,
                    std::max(0.0f, Extent.Height - kScrollbarInset * 2.0f)};
        }
        return {kScrollbarInset,
                Extent.Height - kScrollbarWidth - kScrollbarInset,
                std::max(0.0f, Extent.Width - kScrollbarInset * 2.0f),
                kScrollbarWidth};
    }

    Rect RenderScrollable::GetThumbRect() const {
        const Rect Track      = GetTrackRect();
        const float TrackMain = GetMain(Track.GetSize());
        const float Viewport  = GetMain(GetSize());
        const float Thumb = std::clamp(TrackMain * Viewport / std::max(_ContentExtent, 1.0f), kMinimumThumb, TrackMain);
        const float MaxOffset = std::max(_ContentExtent - Viewport, 1.0f);
        const float Position  = (TrackMain - Thumb) * std::clamp(_Controller->PeekOffset() / MaxOffset, 0.0f, 1.0f);
        return IsVertical() ? Rect {Track.X, Track.Y + Position, Track.Width, Thumb}
                            : Rect {Track.X + Position, Track.Y, Thumb, Track.Height};
    }

    Size RenderScrollView::LayoutContent(const Size& Viewport, float Offset) {
        RenderBox* Child = GetFirstChild();
        if (Child == nullptr) { return {}; }

        const float Cross                     = GetCross(Viewport);
        const float MinCross                  = std::isfinite(Cross) ? Cross : 0.0f;
        const BoxConstraints ChildConstraints = IsVertical() ? BoxConstraints {MinCross, Cross, 0.0f, kUnbounded}
                                                             : BoxConstraints {0.0f, kUnbounded, MinCross, Cross};
        Child->Layout(ChildConstraints, true);
        Child->SetOffset(MakeOffset(-Offset));
        return Child->GetSize();
    }

    void RenderScrollView::PaintContent(PaintContext& Context, float Offset) {
        if (RenderBox* Child = GetFirstChild()) {
            Child->SetOffset(MakeOffset(-Offset));
            Context.PaintChild(*Child);
        }
    }

    void RenderVirtualList::SetItemCount(int Count) {
        if (Count == _ItemCount) { return; }
        _ItemCount = std::max(Count, 0);
        InvalidateItems();
    }

    void RenderVirtualList::SetItemExtent(float Extent) {
        if (Extent == _ItemExtent) { return; }
        _ItemExtent = std::max(Extent, 1.0f);
        InvalidateItems();
    }

    void RenderVirtualList::InvalidateItems() {
        _ItemsDirty = true;
        MarkNeedsLayout();
    }

    Size RenderVirtualList::LayoutContent(const Size& Viewport, float Offset) {
        const float ContentExtent = static_cast<float>(_ItemCount) * _ItemExtent;
        const float ViewExtent    = std::isfinite(GetMain(Viewport)) ? GetMain(Viewport) : ContentExtent;

        const int First = std::clamp(static_cast<int>(std::floor(Offset / _ItemExtent)) - kOverscan, 0, _ItemCount);
        const int Last =
          std::clamp(static_cast<int>(std::ceil((Offset + ViewExtent) / _ItemExtent)) + kOverscan, First, _ItemCount);

        if (_ItemsDirty || First != _BuiltFirst || Last - First != _BuiltCount) {
            _BuiltFirst = First;
            _BuiltCount = Last - First;
            _ItemsDirty = false;
            if (_Builder) { _Builder(First, Last - First); }
        }

        const float Cross                    = GetCross(Viewport);
        const bool Bounded                   = std::isfinite(Cross);
        const float MinCross                 = Bounded ? Cross : 0.0f;
        const BoxConstraints ItemConstraints = IsVertical()
                                                 ? BoxConstraints {MinCross, Cross, _ItemExtent, _ItemExtent}
                                                 : BoxConstraints {_ItemExtent, _ItemExtent, MinCross, Cross};
        float WidestCross                    = MinCross;
        for (size_t i = 0; i < GetChildCount(); ++i) {
            RenderBox& Item = GetChild(i);
            Item.Layout(ItemConstraints, !Bounded);
            Item.SetOffset(MakeOffset(static_cast<float>(_BuiltFirst + static_cast<int>(i)) * _ItemExtent - Offset));
            WidestCross = std::max(WidestCross, GetCross(Item.GetSize()));
        }
        return IsVertical() ? Size {WidestCross, ContentExtent} : Size {ContentExtent, WidestCross};
    }

    void RenderVirtualList::PaintContent(PaintContext& Context, float Offset) {
        (void)Offset;
        for (size_t i = 0; i < GetChildCount(); ++i) {
            Context.PaintChild(GetChild(i));
        }
    }
}  // namespace XenUI
