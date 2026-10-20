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

#include "XenUI/Render/RenderVisual.hpp"
#include "XenUI/Render/Semantics.hpp"

#include <algorithm>
#include <cmath>

namespace XenUI {
    void RenderDecoratedBox::SetDecoration(BoxDecoration Decoration) {
        _Decoration = std::move(Decoration);
        MarkNeedsPaint();
    }

    void RenderDecoratedBox::Paint(PaintContext& Context) {
        const Rect Bounds {0.0f, 0.0f, GetSize().Width, GetSize().Height};
        const float Radius = _Decoration.CornerRadius;

        if (_Decoration.Background || _Decoration.BorderColor) {
            Painter& Canvas = Context.GetPainter();
            if (_Decoration.Background) {
                if (Radius > 0.0f) {
                    Canvas.FillRoundedRect(Bounds, Radius, *_Decoration.Background);
                } else {
                    Canvas.FillRect(Bounds, *_Decoration.Background);
                }
            }
            if (_Decoration.BorderColor && _Decoration.BorderWidth > 0.0f) {
                const Rect Inset         = Bounds.Inflated(-_Decoration.BorderWidth * 0.5f);
                const StrokeStyle Stroke = {.Width = _Decoration.BorderWidth};
                if (Radius > 0.0f) {
                    Canvas.StrokeRoundedRect(Inset,
                                             std::max(0.0f, Radius - _Decoration.BorderWidth * 0.5f),
                                             *_Decoration.BorderColor,
                                             Stroke);
                } else {
                    Canvas.StrokeRect(Inset, *_Decoration.BorderColor, Stroke);
                }
            }
        }
        RenderBox::Paint(Context);
    }

    bool RenderDecoratedBox::HitTestSelf(const Vec2& Position) const {
        (void)Position;
        return _Decoration.Background.has_value();
    }

    void RenderParagraph::SetText(std::string Content) {
        if (Content == _Content) { return; }
        _Content = std::move(Content);
        InvalidateText();
    }

    void RenderParagraph::SetStyle(const ParagraphStyle& Style) {
        if (Style == _Style) { return; }
        const bool OnlyColorChanged = Style.TextFont == _Style.TextFont && Style.Options == _Style.Options;
        _Style                      = Style;
        if (OnlyColorChanged) {
            MarkNeedsPaint();
        } else {
            InvalidateText();
        }
    }

    bool RenderParagraph::DescribeSemantics(SemanticsProperties& Properties) const {
        if (_Content.empty()) { return false; }
        Properties = {.Role = SemanticsRole::Text, .Label = _Content};
        return true;
    }

    void RenderParagraph::Paint(PaintContext& Context) {
        if (!_Layout.IsEmpty()) { Context.GetPainter().DrawTextLayout(_Layout, {}, _Style.Foreground); }
    }

    void RenderParagraph::PerformLayout() {
        const BoxConstraints& Constraints = GetConstraints();
        const bool UsesWidth              = _Style.Options.Wrapping == TextWrapping::Wrap ||
                               _Style.Options.Trimming != TextTrimming::None ||
                               _Style.Options.Alignment != TextAlignment::Leading;
        const float AvailableWidth = UsesWidth && Constraints.HasBoundedWidth() ? Constraints.MaxWidth : kUnbounded;

        if (_Layout.IsEmpty() || _LayoutWidth != AvailableWidth) {
            _Layout      = TextLayout(_Content, _Style.TextFont, _Style.Options, {AvailableWidth, kUnbounded});
            _LayoutWidth = AvailableWidth;
        }

        const Size Measured = _Layout.GetMeasuredSize();
        SetSize(Constraints.Constrain({std::ceil(Measured.Width), std::ceil(Measured.Height)}));

        const bool NeedsAlignedBox =
          _Style.Options.Alignment != TextAlignment::Leading && _Layout.GetBoxSize().Width != GetSize().Width;
        if (NeedsAlignedBox) {
            _Layout = TextLayout(_Content, _Style.TextFont, _Style.Options, {GetSize().Width, kUnbounded});
        }
    }

    bool RenderParagraph::HitTestSelf(const Vec2& Position) const {
        (void)Position;
        return true;
    }

    void RenderParagraph::InvalidateText() {
        _Layout      = {};
        _LayoutWidth = -1.0f;
        MarkNeedsLayout();
    }

    void RenderImage::SetImage(const Image& Source) {
        if (Source.GetData() == _Source.GetData()) { return; }
        _Source = Source;
        MarkNeedsLayout();
    }

    void RenderImage::SetStyle(const ImageStyle& Style) {
        if (Style == _Style) { return; }
        _Style = Style;
        MarkNeedsLayout();
    }

    void RenderImage::PerformLayout() {
        const float IntrinsicWidth  = static_cast<float>(_Source.GetPixelWidth());
        const float IntrinsicHeight = static_cast<float>(_Source.GetPixelHeight());
        const float Aspect          = IntrinsicHeight > 0.0f ? IntrinsicWidth / IntrinsicHeight : 1.0f;

        Size Desired {IntrinsicWidth, IntrinsicHeight};
        if (_Style.Width && _Style.Height) {
            Desired = {*_Style.Width, *_Style.Height};
        } else if (_Style.Width) {
            Desired = {*_Style.Width, *_Style.Width / Aspect};
        } else if (_Style.Height) {
            Desired = {*_Style.Height * Aspect, *_Style.Height};
        }
        SetSize(GetConstraints().Constrain(Desired));
    }

    bool RenderImage::HitTestSelf(const Vec2& Position) const {
        (void)Position;
        return !_Source.IsEmpty();
    }

    void RenderImage::Paint(PaintContext& Context) {
        if (_Source.IsEmpty() || GetSize().IsEmpty()) { return; }

        const Size Box = GetSize();
        const Size Intrinsic {static_cast<float>(_Source.GetPixelWidth()),
                              static_cast<float>(_Source.GetPixelHeight())};
        const float ScaleX = Box.Width / Intrinsic.Width;
        const float ScaleY = Box.Height / Intrinsic.Height;

        Size Drawn = Box;
        switch (_Style.Fit) {
            case ImageFit::Contain:
                Drawn = Intrinsic * std::min(ScaleX, ScaleY);
                break;
            case ImageFit::Cover:
                Drawn = Intrinsic * std::max(ScaleX, ScaleY);
                break;
            case ImageFit::ScaleDown:
                Drawn = Intrinsic * std::min({ScaleX, ScaleY, 1.0f});
                break;
            case ImageFit::None:
                Drawn = Intrinsic;
                break;
            default:
                break;
        }

        const Vec2 Origin = Alignment::Center().Place(Drawn, Box);
        const Rect Destination {Origin.X, Origin.Y, Drawn.Width, Drawn.Height};
        const bool Overflows = Drawn.Width > Box.Width || Drawn.Height > Box.Height;

        Painter& Canvas = Context.GetPainter();
        if (Overflows) { Canvas.PushClip({0.0f, 0.0f, Box.Width, Box.Height}); }
        Canvas.DrawImage(_Source, Destination, {.Opacity = _Style.Opacity, .Interpolation = _Style.Interpolation});
        if (Overflows) { Canvas.PopClip(); }
    }

    void RenderCustomPaint::SetCallback(PaintCallback Callback) {
        _Callback = std::move(Callback);
        MarkNeedsPaint();
    }

    void RenderCustomPaint::SetPreferredSize(std::optional<float> Width, std::optional<float> Height) {
        if (Width == _PreferredWidth && Height == _PreferredHeight) { return; }
        _PreferredWidth  = Width;
        _PreferredHeight = Height;
        MarkNeedsLayout();
    }

    void RenderCustomPaint::Paint(PaintContext& Context) {
        if (_Callback) {
            TrackingScope Scope(*this);
            _Callback(Context.GetPainter(), GetSize());
        }
        RenderBox::Paint(Context);
    }

    bool RenderCustomPaint::HitTestSelf(const Vec2& Position) const {
        (void)Position;
        return true;
    }

    void RenderCustomPaint::OnDependencyChanged() {
        MarkNeedsPaint();
    }

    void RenderCustomPaint::PerformLayout() {
        const BoxConstraints& Constraints = GetConstraints();
        if (RenderBox* Child = GetFirstChild()) {
            Child->Layout(Constraints, true);
            Child->SetOffset({});
            SetSize(Child->GetSize());
            return;
        }

        const Size Biggest = Constraints.GetBiggest();
        SetSize(
          Constraints.Constrain({_PreferredWidth.value_or(Biggest.Width), _PreferredHeight.value_or(Biggest.Height)}));
    }
}  // namespace XenUI
