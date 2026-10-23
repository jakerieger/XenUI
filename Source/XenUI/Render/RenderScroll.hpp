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
#include "XenUI/Reactive/Signal.hpp"
#include "XenUI/Render/RenderInput.hpp"
#include "XenUI/Render/RenderLayout.hpp"

#include <algorithm>
#include <functional>
#include <optional>

namespace XenUI {
    /// @brief Scroll position of a scrollable widget. Own one to read or drive scrolling from a component;
    /// the offset and extents are tracked signals.
    class XENUI_API ScrollController {
    public:
        [[nodiscard]] float GetOffset() const { return _Offset.Get(); }
        [[nodiscard]] float PeekOffset() const { return _Offset.Peek(); }
        [[nodiscard]] float GetMaxOffset() const { return _MaxOffset.Get(); }
        [[nodiscard]] float GetViewportExtent() const { return _ViewportExtent.Get(); }

        /// @brief Scrolls to Offset, clamped to the scrollable range.
        void SetOffset(float Offset);
        void ScrollBy(float Delta) { SetOffset(_Offset.Peek() + Delta); }

        /// @brief Scrolls the minimum distance needed to bring [Start, End) into view.
        void EnsureVisible(float Start, float End);

        /// @brief Like EnsureVisible, for content that is about to change size: the offset is clamped to the new
        /// range at the next layout instead of now.
        void RequestVisible(float Start, float End);

        /// @brief Sets the offset without clamping it, for when the content is about to change size (for example,
        /// while zooming). The next layout clamps it to the new range.
        void RequestOffset(float Offset) { _Offset = std::max(Offset, 0.0f); }

        /// @internal
        void UpdateMetrics(float ViewportExtent, float ContentExtent);

    private:
        Signal<float> _Offset         = 0.0f;
        Signal<float> _MaxOffset      = 0.0f;
        Signal<float> _ViewportExtent = 0.0f;
    };

    /// @brief Clips content to its bounds. A repaint boundary, so the clip applies to any nested layers.
    class XENUI_API RenderClipRect final : public RenderBox {
    public:
        [[nodiscard]] bool IsRepaintBoundary() const override { return true; }
        [[nodiscard]] bool ClipsChildren() const override { return true; }
        void Paint(PaintContext& Context) override;
    };

    /// @brief Base for clipped, scrollable viewports: owns the scroll position, wheel and scrollbar handling.
    /// Vertical viewports scroll with the wheel; horizontal ones with a horizontal wheel or Shift and the wheel, so
    /// the two nest. A viewport with an unbounded cross axis sizes to its content across.
    class XENUI_API RenderScrollable : public RenderInteractive,
                                       public Observer {
    public:
        static constexpr float kWheelStep      = 48.0f;
        static constexpr float kScrollbarWidth = 8.0f;

        RenderScrollable();

        /// @brief Uses an externally owned controller, or an internal one when nullptr.
        void SetController(ScrollController* External);
        void SetDirection(Axis Direction);
        void SetShowScrollbar(bool ShowScrollbar);
        void SetScrollbarColors(const Color& Thumb, const Color& Track);

        [[nodiscard]] ScrollController& GetController() const { return *_Controller; }
        [[nodiscard]] bool ClipsChildren() const override { return true; }
        [[nodiscard]] bool IsVertical() const { return _Direction == Axis::Vertical; }

        [[nodiscard]] bool IsRepaintBoundary() const override { return true; }
        bool HitTest(HitTestResult& Result, const Vec2& Position) override;
        bool HandlePointer(const PointerEvent& Event) override;
        void Paint(PaintContext& Context) override;
        void OnDependencyChanged() override;

    protected:
        void PerformLayout() final;

        /// @brief Lays out the content for a viewport at Offset and returns the content's size. Either axis of the
        /// viewport is unbounded when the parent does not bound it.
        virtual Size LayoutContent(const Size& Viewport, float Offset) = 0;
        virtual void PaintContent(PaintContext& Context, float Offset) = 0;
        virtual void OnOffsetChanged() { MarkNeedsPaint(); }

        [[nodiscard]] float GetMain(const Size& Value) const { return IsVertical() ? Value.Height : Value.Width; }
        [[nodiscard]] float GetCross(const Size& Value) const { return IsVertical() ? Value.Width : Value.Height; }
        [[nodiscard]] Vec2 MakeOffset(float Main) const { return IsVertical() ? Vec2 {0.0f, Main} : Vec2 {Main, 0.0f}; }

    private:
        [[nodiscard]] bool CanScroll() const;
        [[nodiscard]] bool HasScrollbar() const;
        [[nodiscard]] Rect GetTrackRect() const;
        [[nodiscard]] Rect GetThumbRect() const;
        [[nodiscard]] float GetMainPosition(const Vec2& Point) const { return IsVertical() ? Point.Y : Point.X; }

        ScrollController _InternalController;
        ScrollController* _Controller = &_InternalController;
        Axis _Direction               = Axis::Vertical;
        bool _ShowScrollbar           = true;
        Color _ThumbColor             = Color::FromHex(0xFFFFFF, 0.28f);
        Color _TrackColor             = Color::FromHex(0xFFFFFF, 0.05f);
        float _ContentExtent          = 0.0f;
        std::optional<float> _ThumbGrab;
    };

    class XENUI_API RenderScrollView final : public RenderScrollable {
    protected:
        Size LayoutContent(const Size& Viewport, float Offset) override;
        void PaintContent(PaintContext& Context, float Offset) override;
    };

    /// @brief Scrollable list of equally sized items, of which only the visible ones (plus a small margin) exist.
    /// Items are requested from the element that owns this list while laying out.
    class XENUI_API RenderVirtualList final : public RenderScrollable {
    public:
        static constexpr int kOverscan = 2;

        using RangeBuilder = std::function<void(int First, int Count)>;

        void SetRangeBuilder(RangeBuilder Builder) { _Builder = std::move(Builder); }
        void SetItemCount(int Count);
        void SetItemExtent(float Extent);

        /// @brief Requests the visible items again on the next layout.
        void InvalidateItems();

        [[nodiscard]] int GetFirstBuiltIndex() const { return _BuiltFirst; }

    protected:
        Size LayoutContent(const Size& Viewport, float Offset) override;
        void PaintContent(PaintContext& Context, float Offset) override;
        void OnOffsetChanged() override { MarkNeedsLayout(); }

    private:
        RangeBuilder _Builder;
        int _ItemCount    = 0;
        float _ItemExtent = 28.0f;
        int _BuiltFirst   = 0;
        int _BuiltCount   = -1;
        bool _ItemsDirty  = true;
    };
}  // namespace XenUI
