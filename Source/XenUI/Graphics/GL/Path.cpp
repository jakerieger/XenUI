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
#include "XenUI/Graphics/GL/PathData.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <numbers>

namespace XenUI {
    namespace {
        constexpr int kMaxCurveSegments = 512;

        float Length(const Vec2& Value) {
            return std::hypot(Value.X, Value.Y);
        }

        int SegmentCount(float Estimate) {
            if (!std::isfinite(Estimate)) { return 1; }
            return std::clamp(static_cast<int>(std::ceil(Estimate)), 1, kMaxCurveSegments);
        }

        void AddQuad(std::vector<Vec2>& Out, const Vec2& P0, const Vec2& P1, const Vec2& P2) {
            // Uniform steps keep the error below |P0 - 2 P1 + P2| / (4 n^2).
            const float Bend = Length(P0 - P1 * 2.0f + P2);
            const int Steps  = SegmentCount(std::sqrt(Bend / (4.0f * PathData::kTolerance)));
            for (int Step = 1; Step <= Steps; ++Step) {
                const float T = static_cast<float>(Step) / static_cast<float>(Steps);
                const float U = 1.0f - T;
                Out.push_back(P0 * (U * U) + P1 * (2.0f * U * T) + P2 * (T * T));
            }
        }

        void AddCubic(std::vector<Vec2>& Out, const Vec2& P0, const Vec2& P1, const Vec2& P2, const Vec2& P3) {
            // Uniform steps keep the error below 3/4 of the largest second difference over n^2.
            const float Bend = std::max(Length(P0 - P1 * 2.0f + P2), Length(P1 - P2 * 2.0f + P3));
            const int Steps  = SegmentCount(std::sqrt(0.75f * Bend / PathData::kTolerance));
            for (int Step = 1; Step <= Steps; ++Step) {
                const float T = static_cast<float>(Step) / static_cast<float>(Steps);
                const float U = 1.0f - T;
                Out.push_back(P0 * (U * U * U) + P1 * (3.0f * U * U * T) + P2 * (3.0f * U * T * T) +
                              P3 * (T * T * T));
            }
        }

        /// @brief SVG elliptical arc from Start to End, converted to centre form as in SVG 1.1 appendix F.6.5. Screen
        /// coordinates point y down, so a clockwise sweep is the positive-angle direction.
        void AddArc(std::vector<Vec2>& Out,
                    const Vec2& Start,
                    const Vec2& End,
                    Vec2 Radius,
                    float RotationDegrees,
                    ArcSweep Sweep,
                    ArcSize Extent) {
            if (Start == End) { return; }
            Radius = {std::abs(Radius.X), std::abs(Radius.Y)};
            if (Radius.X <= 0.0f || Radius.Y <= 0.0f) {
                Out.push_back(End);
                return;
            }

            const double Phi    = static_cast<double>(RotationDegrees) * std::numbers::pi / 180.0;
            const double CosPhi = std::cos(Phi);
            const double SinPhi = std::sin(Phi);
            const double HalfX  = (static_cast<double>(Start.X) - End.X) * 0.5;
            const double HalfY  = (static_cast<double>(Start.Y) - End.Y) * 0.5;
            const double X1     = CosPhi * HalfX + SinPhi * HalfY;
            const double Y1     = -SinPhi * HalfX + CosPhi * HalfY;

            double Rx = Radius.X;
            double Ry = Radius.Y;
            // Radii too small to reach the end point scale up until they just do.
            const double Reach = (X1 * X1) / (Rx * Rx) + (Y1 * Y1) / (Ry * Ry);
            if (Reach > 1.0) {
                const double Scale = std::sqrt(Reach);
                Rx *= Scale;
                Ry *= Scale;
            }

            const bool Large     = Extent == ArcSize::Large;
            const bool Clockwise = Sweep == ArcSweep::Clockwise;
            const double Numer   = Rx * Rx * Ry * Ry - Rx * Rx * Y1 * Y1 - Ry * Ry * X1 * X1;
            const double Denom   = Rx * Rx * Y1 * Y1 + Ry * Ry * X1 * X1;
            double Factor        = Denom > 0.0 ? std::sqrt(std::max(0.0, Numer / Denom)) : 0.0;
            if (Large == Clockwise) { Factor = -Factor; }
            const double Cx1 = Factor * Rx * Y1 / Ry;
            const double Cy1 = -Factor * Ry * X1 / Rx;
            const double Cx  = CosPhi * Cx1 - SinPhi * Cy1 + (static_cast<double>(Start.X) + End.X) * 0.5;
            const double Cy  = SinPhi * Cx1 + CosPhi * Cy1 + (static_cast<double>(Start.Y) + End.Y) * 0.5;

            const auto Angle = [](double Ux, double Uy, double Vx, double Vy) {
                return std::atan2(Ux * Vy - Uy * Vx, Ux * Vx + Uy * Vy);
            };
            const double Theta1 = Angle(1.0, 0.0, (X1 - Cx1) / Rx, (Y1 - Cy1) / Ry);
            double Delta        = Angle((X1 - Cx1) / Rx, (Y1 - Cy1) / Ry, (-X1 - Cx1) / Rx, (-Y1 - Cy1) / Ry);
            if (!Clockwise && Delta > 0.0) { Delta -= 2.0 * std::numbers::pi; }
            if (Clockwise && Delta < 0.0) { Delta += 2.0 * std::numbers::pi; }

            // Each step of angle 2 acos(1 - tolerance / radius) keeps the chord within tolerance of the arc.
            const double Largest = std::max(Rx, Ry);
            const double Step    = 2.0 * std::acos(std::clamp(1.0 - PathData::kTolerance / Largest, -1.0, 1.0));
            const int Steps      = SegmentCount(static_cast<float>(std::abs(Delta) / std::max(Step, 1.0e-4)));
            for (int Index = 1; Index < Steps; ++Index) {
                const double Theta = Theta1 + Delta * Index / Steps;
                const double Ex    = Rx * std::cos(Theta);
                const double Ey    = Ry * std::sin(Theta);
                Out.push_back({static_cast<float>(CosPhi * Ex - SinPhi * Ey + Cx),
                               static_cast<float>(SinPhi * Ex + CosPhi * Ey + Cy)});
            }
            Out.push_back(End);
        }
    }  // namespace

    Rect Path::GetBounds() const {
        return _Data ? _Data->Bounds : Rect {};
    }

    Path PathBuilder::Build() const {
        if (_Segments.empty()) { return {}; }

        static std::atomic<uint64_t> NextId {1};
        auto Data  = std::make_shared<PathData>();
        Data->Id   = NextId.fetch_add(1, std::memory_order_relaxed);
        Data->Rule = _FillRule;

        PathData::Figure* Open = nullptr;
        Vec2 Current;
        Vec2 FigureStart;

        const auto EndFigure = [&](bool Closed) {
            if (Open == nullptr) { return; }
            Open->Closed = Closed;
            Open         = nullptr;
        };
        const auto EnsureFigure = [&]() -> std::vector<Vec2>& {
            if (Open == nullptr) {
                Open = &Data->Figures.emplace_back();
                Open->Points.push_back(Current);
                FigureStart = Current;
            }
            return Open->Points;
        };

        for (const Segment& Entry : _Segments) {
            switch (Entry.Kind) {
                case SegmentKind::Move:
                    EndFigure(false);
                    Current = Entry.Points[0];
                    break;
                case SegmentKind::Line:
                    EnsureFigure().push_back(Entry.Points[0]);
                    Current = Entry.Points[0];
                    break;
                case SegmentKind::Quad:
                    AddQuad(EnsureFigure(), Current, Entry.Points[0], Entry.Points[1]);
                    Current = Entry.Points[1];
                    break;
                case SegmentKind::Cubic:
                    AddCubic(EnsureFigure(), Current, Entry.Points[0], Entry.Points[1], Entry.Points[2]);
                    Current = Entry.Points[2];
                    break;
                case SegmentKind::Arc:
                    AddArc(EnsureFigure(),
                           Current,
                           Entry.Points[0],
                           Entry.Radius,
                           Entry.Rotation,
                           Entry.Sweep,
                           Entry.Extent);
                    Current = Entry.Points[0];
                    break;
                case SegmentKind::Close:
                    if (Open != nullptr) {
                        EndFigure(true);
                        Current = FigureStart;
                    }
                    break;
            }
        }
        EndFigure(false);

        float Left   = std::numeric_limits<float>::infinity();
        float Top    = Left;
        float Right  = -Left;
        float Bottom = -Left;
        for (const PathData::Figure& Shape : Data->Figures) {
            for (const Vec2& Point : Shape.Points) {
                Left   = std::min(Left, Point.X);
                Top    = std::min(Top, Point.Y);
                Right  = std::max(Right, Point.X);
                Bottom = std::max(Bottom, Point.Y);
            }
        }
        Data->Bounds = Left <= Right ? Rect::FromEdges(Left, Top, Right, Bottom) : Rect {};

        return Path(std::move(Data));
    }
}  // namespace XenUI
