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

#include "XenUI/Graphics/Painter.hpp"
#include "XenUI/AppContext.hpp"
#include "XenUI/Graphics/D2DConvert.hpp"
#include "XenUI/Graphics/D2DRenderer.hpp"
#include "XenUI/Graphics/PathData.hpp"

#include <algorithm>

namespace XenUI {
    namespace {
        constexpr float kAntialiasPadding = 1.0f;

        constexpr float kSqrtTwo = 1.4143f;

        float GetShapeStrokeExtent(const StrokeStyle& Stroke) {
            return Stroke.Width * 0.5f * kSqrtTwo + kAntialiasPadding;
        }

        Rect GetStrokedPathBounds(const Path& Geometry, const StrokeStyle& Stroke) {
            GraphicsDevice& Device  = AppContext::Get().GetGraphicsDevice();
            ID2D1StrokeStyle* Style = Device.GetResources().GetStrokeStyle(*Device.GetD2DFactory(), Stroke);

            D2D1_RECT_F Bounds {};
            const HRESULT Result =
              Geometry.GetData()->Geometry->GetWidenedBounds(Stroke.Width, Style, nullptr, &Bounds);
            if (FAILED(Result) || Bounds.left > Bounds.right) {
                const float JoinFactor =
                  Stroke.Join == LineJoin::Miter ? std::max(Stroke.MiterLimit, kSqrtTwo) : kSqrtTwo;
                return Geometry.GetBounds().Inflated(Stroke.Width * 0.5f * JoinFactor);
            }
            return FromD2D(Bounds);
        }

        Rect GetEllipseBounds(const Vec2& Center, const Vec2& Radius) {
            return {Center.X - Radius.X, Center.Y - Radius.Y, Radius.X * 2.0f, Radius.Y * 2.0f};
        }
    }  // namespace

    Painter::Painter(DisplayList& Target) : _Target(Target) {}

    void Painter::PushTransform(const Matrix3x2& Transform) {
        _Target._Commands.emplace_back(PushTransformCommand {Transform});
        _TransformStack.push_back(_Transform);
        _Transform = Transform * _Transform;
    }

    void Painter::PopTransform() {
        if (_TransformStack.empty()) { return; }
        _Target._Commands.emplace_back(PopTransformCommand {});
        _Transform = _TransformStack.back();
        _TransformStack.pop_back();
    }

    void Painter::PushClip(const Rect& Bounds) {
        _Target._Commands.emplace_back(PushClipCommand {Bounds});
        _ClipStack.push_back(_Clip);
        const Rect ListBounds = _Transform.TransformRect(Bounds);
        _Clip                 = _Clip ? _Clip->Intersection(ListBounds) : ListBounds;
    }

    void Painter::PopClip() {
        if (_ClipStack.empty()) { return; }
        _Target._Commands.emplace_back(PopClipCommand {});
        _Clip = _ClipStack.back();
        _ClipStack.pop_back();
    }

    void Painter::PushOpacity(float Opacity) {
        _Target._Commands.emplace_back(PushOpacityCommand {Opacity});
    }

    void Painter::PopOpacity() {
        _Target._Commands.emplace_back(PopOpacityCommand {});
    }

    void Painter::FillRect(const Rect& Bounds, const Brush& Paint) {
        _Target._Commands.emplace_back(FillRectCommand {Bounds, Paint});
        IncludeBounds(Bounds.Inflated(kAntialiasPadding));
    }

    void Painter::StrokeRect(const Rect& Bounds, const Brush& Paint, const StrokeStyle& Stroke) {
        _Target._Commands.emplace_back(StrokeRectCommand {Bounds, Paint, Stroke});
        IncludeBounds(Bounds.Inflated(GetShapeStrokeExtent(Stroke)));
    }

    void Painter::FillRoundedRect(const Rect& Bounds, float Radius, const Brush& Paint) {
        _Target._Commands.emplace_back(FillRoundedRectCommand {Bounds, Radius, Paint});
        IncludeBounds(Bounds.Inflated(kAntialiasPadding));
    }

    void Painter::StrokeRoundedRect(const Rect& Bounds, float Radius, const Brush& Paint, const StrokeStyle& Stroke) {
        _Target._Commands.emplace_back(StrokeRoundedRectCommand {Bounds, Radius, Paint, Stroke});
        IncludeBounds(Bounds.Inflated(GetShapeStrokeExtent(Stroke)));
    }

    void Painter::FillEllipse(const Vec2& Center, const Vec2& Radius, const Brush& Paint) {
        _Target._Commands.emplace_back(FillEllipseCommand {Center, Radius, Paint});
        IncludeBounds(GetEllipseBounds(Center, Radius).Inflated(kAntialiasPadding));
    }

    void Painter::StrokeEllipse(const Vec2& Center, const Vec2& Radius, const Brush& Paint, const StrokeStyle& Stroke) {
        _Target._Commands.emplace_back(StrokeEllipseCommand {Center, Radius, Paint, Stroke});
        IncludeBounds(GetEllipseBounds(Center, Radius).Inflated(GetShapeStrokeExtent(Stroke)));
    }

    void Painter::FillCircle(const Vec2& Center, float Radius, const Brush& Paint) {
        FillEllipse(Center, {Radius, Radius}, Paint);
    }

    void Painter::StrokeCircle(const Vec2& Center, float Radius, const Brush& Paint, const StrokeStyle& Stroke) {
        StrokeEllipse(Center, {Radius, Radius}, Paint, Stroke);
    }

    void Painter::DrawLine(const Vec2& Start, const Vec2& End, const Brush& Paint, const StrokeStyle& Stroke) {
        _Target._Commands.emplace_back(DrawLineCommand {Start, End, Paint, Stroke});
        const Rect Span = Rect::FromEdges(std::min(Start.X, End.X),
                                          std::min(Start.Y, End.Y),
                                          std::max(Start.X, End.X),
                                          std::max(Start.Y, End.Y));
        IncludeBounds(Span.Inflated(GetShapeStrokeExtent(Stroke)));
    }

    void Painter::FillPath(const Path& Geometry, const Brush& Paint) {
        if (Geometry.IsEmpty()) { return; }
        _Target._Commands.emplace_back(FillPathCommand {Geometry, Paint});
        IncludeBounds(Geometry.GetBounds().Inflated(kAntialiasPadding));
    }

    void Painter::StrokePath(const Path& Geometry, const Brush& Paint, const StrokeStyle& Stroke) {
        if (Geometry.IsEmpty()) { return; }
        _Target._Commands.emplace_back(StrokePathCommand {Geometry, Paint, Stroke});
        IncludeBounds(GetStrokedPathBounds(Geometry, Stroke).Inflated(kAntialiasPadding));
    }

    void Painter::DrawTextLayout(const TextLayout& Layout, const Vec2& Origin, const Brush& Paint) {
        if (Layout.IsEmpty()) { return; }
        _Target._Commands.emplace_back(DrawTextLayoutCommand {Layout, Origin, Paint});
        IncludeBounds(Layout.GetInkBounds().Translated(Origin).Inflated(kAntialiasPadding));
    }

    void Painter::DrawImage(const Image& Source, const Rect& Destination, const ImageDrawOptions& Options) {
        if (Source.IsEmpty()) { return; }
        _Target._Commands.emplace_back(DrawImageCommand {Source, Destination, Options});
        IncludeBounds(Destination.Inflated(kAntialiasPadding));
    }

    void Painter::IncludeBounds(const Rect& LocalBounds) {
        Rect ListBounds = _Transform.TransformRect(LocalBounds);
        if (_Clip) { ListBounds = ListBounds.Intersection(*_Clip); }
        _Target._Bounds = _Target._Bounds.Union(ListBounds);
    }
}  // namespace XenUI
