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

#include "XenUI/Graphics/Path.hpp"
#include "XenUI/AppContext.hpp"
#include "XenUI/Graphics/D2D/D2DConvert.hpp"
#include "XenUI/Graphics/D2D/PathData.hpp"
#include "XenUI/Platform/Win32/HResult.hpp"

namespace XenUI {
    Rect Path::GetBounds() const {
        return _Data ? _Data->Bounds : Rect {};
    }

    Path PathBuilder::Build() const {
        if (_Segments.empty()) { return {}; }

        ID2D1Factory1* Factory = AppContext::Get().GetGraphicsDevice().GetD2DFactory();
        auto Data              = std::make_shared<PathData>();
        ThrowIfFailed(Factory->CreatePathGeometry(&Data->Geometry), "ID2D1Factory::CreatePathGeometry");

        Microsoft::WRL::ComPtr<ID2D1GeometrySink> Sink;
        ThrowIfFailed(Data->Geometry->Open(&Sink), "ID2D1PathGeometry::Open");
        Sink->SetFillMode(_FillRule == FillRule::EvenOdd ? D2D1_FILL_MODE_ALTERNATE : D2D1_FILL_MODE_WINDING);

        bool FigureOpen = false;
        Vec2 Current;
        Vec2 FigureStart;

        const auto EnsureFigure = [&] {
            if (FigureOpen) { return; }
            Sink->BeginFigure(ToD2D(Current), D2D1_FIGURE_BEGIN_FILLED);
            FigureStart = Current;
            FigureOpen  = true;
        };

        for (const Segment& Entry : _Segments) {
            switch (Entry.Kind) {
                case SegmentKind::Move:
                    if (FigureOpen) {
                        Sink->EndFigure(D2D1_FIGURE_END_OPEN);
                        FigureOpen = false;
                    }
                    Current = Entry.Points[0];
                    break;
                case SegmentKind::Line:
                    EnsureFigure();
                    Sink->AddLine(ToD2D(Entry.Points[0]));
                    Current = Entry.Points[0];
                    break;
                case SegmentKind::Quad:
                    EnsureFigure();
                    Sink->AddQuadraticBezier(
                      D2D1::QuadraticBezierSegment(ToD2D(Entry.Points[0]), ToD2D(Entry.Points[1])));
                    Current = Entry.Points[1];
                    break;
                case SegmentKind::Cubic:
                    EnsureFigure();
                    Sink->AddBezier(
                      D2D1::BezierSegment(ToD2D(Entry.Points[0]), ToD2D(Entry.Points[1]), ToD2D(Entry.Points[2])));
                    Current = Entry.Points[2];
                    break;
                case SegmentKind::Arc:
                    EnsureFigure();
                    Sink->AddArc(
                      D2D1::ArcSegment(ToD2D(Entry.Points[0]),
                                       D2D1::SizeF(Entry.Radius.X, Entry.Radius.Y),
                                       Entry.Rotation,
                                       Entry.Sweep == ArcSweep::Clockwise ? D2D1_SWEEP_DIRECTION_CLOCKWISE
                                                                          : D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,
                                       Entry.Extent == ArcSize::Large ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL));
                    Current = Entry.Points[0];
                    break;
                case SegmentKind::Close:
                    if (FigureOpen) {
                        Sink->EndFigure(D2D1_FIGURE_END_CLOSED);
                        FigureOpen = false;
                        Current    = FigureStart;
                    }
                    break;
            }
        }

        if (FigureOpen) { Sink->EndFigure(D2D1_FIGURE_END_OPEN); }
        ThrowIfFailed(Sink->Close(), "ID2D1GeometrySink::Close");

        D2D1_RECT_F Bounds {};
        ThrowIfFailed(Data->Geometry->GetBounds(nullptr, &Bounds), "ID2D1Geometry::GetBounds");
        Data->Bounds = Bounds.left <= Bounds.right ? FromD2D(Bounds) : Rect {};

        return Path(std::move(Data));
    }
}  // namespace XenUI
