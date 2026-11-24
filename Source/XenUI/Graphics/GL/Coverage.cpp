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

#include "XenUI/Graphics/GL/Coverage.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace XenUI {
    namespace {
        constexpr float kEpsilon      = 1e-6f;
        constexpr int kMaxArcSegments = 256;

        float Dot(const Vec2& A, const Vec2& B) {
            return A.X * B.X + A.Y * B.Y;
        }

        float Cross(const Vec2& A, const Vec2& B) {
            return A.X * B.Y - A.Y * B.X;
        }

        float Length(const Vec2& V) {
            return std::sqrt(Dot(V, V));
        }

        Vec2 Normalized(const Vec2& V) {
            const float L = Length(V);
            return L > kEpsilon ? V / L : Vec2 {1.0f, 0.0f};
        }

        Vec2 Perpendicular(const Vec2& V) {
            return {-V.Y, V.X};
        }

        Vec2 Rotate(const Vec2& V, float Angle) {
            const float C = std::cos(Angle);
            const float S = std::sin(Angle);
            return {V.X * C - V.Y * S, V.X * S + V.Y * C};
        }

        float SignedArea(const Contour& Points) {
            float Area = 0.0f;
            for (size_t I = 0, J = Points.size() - 1; I < Points.size(); J = I++) {
                Area += Cross(Points[J], Points[I]);
            }
            return Area * 0.5f;
        }

        /// @brief Segments needed for an arc of Sweep radians on a circle of Radius to stay within Tolerance.
        int ArcSegments(float Radius, float Sweep, float Tolerance) {
            if (Radius <= Tolerance) { return 2; }
            const float Step = 2.0f * std::acos(std::clamp(1.0f - Tolerance / Radius, -1.0f, 1.0f));
            const int Count  = static_cast<int>(std::ceil(std::abs(Sweep) / std::max(Step, 1e-3f)));
            return std::clamp(Count, 1, kMaxArcSegments);
        }

        /// @brief A polyline to stroke as one piece, with no joins to anything else.
        struct Run {
            std::vector<Vec2> Points;
            bool Closed = false;
            /// @brief The direction to orient end caps along when the run has no length.
            Vec2 Direction {1.0f, 0.0f};
        };

        void AppendDeduplicated(std::vector<Vec2>& Points, const Vec2& Point) {
            if (Points.empty() || Length(Point - Points.back()) > kEpsilon) { Points.push_back(Point); }
        }

        /// @brief Splits Figures into the "on" pieces of Stroke's dash pattern.
        std::vector<Run> ApplyDashes(const std::vector<PathData::Figure>& Figures, const StrokeStyle& Stroke) {
            std::vector<float> Pattern;
            float PatternLength = 0.0f;
            for (const float Dash : Stroke.Dashes) {
                Pattern.push_back(std::max(0.0f, Dash) * Stroke.Width);
                PatternLength += Pattern.back();
            }
            if (Pattern.size() % 2 == 1) {
                Pattern.insert(Pattern.end(), Pattern.begin(), Pattern.end());
                PatternLength *= 2.0f;
            }

            std::vector<Run> Runs;
            if (Pattern.empty() || PatternLength <= kEpsilon) {
                for (const PathData::Figure& Shape : Figures) {
                    Run Piece;
                    for (const Vec2& Point : Shape.Points) {
                        AppendDeduplicated(Piece.Points, Point);
                    }
                    Piece.Closed = Shape.Closed && Piece.Points.size() > 2;
                    if (Piece.Closed && Length(Piece.Points.front() - Piece.Points.back()) <= kEpsilon) {
                        Piece.Points.pop_back();
                    }
                    if (!Piece.Points.empty()) { Runs.push_back(std::move(Piece)); }
                }
                return Runs;
            }

            for (const PathData::Figure& Shape : Figures) {
                std::vector<Vec2> Points = Shape.Points;
                if (Shape.Closed && !Points.empty()) { Points.push_back(Points.front()); }
                if (Points.empty()) { continue; }

                // Where in the pattern the figure starts, and whether that's a dash.
                float Phase = std::fmod(Stroke.DashOffset * Stroke.Width, PatternLength);
                if (Phase < 0.0f) { Phase += PatternLength; }
                size_t Index = 0;
                while (Phase >= Pattern[Index]) {
                    Phase -= Pattern[Index];
                    Index = (Index + 1) % Pattern.size();
                }
                float Remaining = Pattern[Index] - Phase;
                bool On         = Index % 2 == 0;

                Run Current;
                if (On) { Current.Points.push_back(Points.front()); }
                for (size_t I = 1; I < Points.size(); ++I) {
                    Vec2 From           = Points[I - 1];
                    const Vec2 To       = Points[I];
                    float SegmentLength = Length(To - From);
                    const Vec2 Along    = Normalized(To - From);
                    if (SegmentLength <= kEpsilon) { continue; }
                    while (SegmentLength >= Remaining) {
                        From = From + Along * Remaining;
                        SegmentLength -= Remaining;
                        if (On) {
                            AppendDeduplicated(Current.Points, From);
                            Current.Direction = Along;
                            Runs.push_back(std::move(Current));
                            Current = {};
                        } else {
                            Current.Points.push_back(From);
                            Current.Direction = Along;
                        }
                        On        = !On;
                        Index     = (Index + 1) % Pattern.size();
                        Remaining = Pattern[Index];
                    }
                    Remaining -= SegmentLength;
                    if (On) {
                        AppendDeduplicated(Current.Points, To);
                        Current.Direction = Along;
                    }
                }
                if (On && !Current.Points.empty()) { Runs.push_back(std::move(Current)); }
            }
            return Runs;
        }

        class Stroker {
        public:
            Stroker(const StrokeStyle& Stroke, float Tolerance)
                : _Stroke(Stroke), _HalfWidth(Stroke.Width * 0.5f), _Tolerance(std::max(Tolerance, 1e-4f)) {}

            void Add(const Run& Piece) {
                const std::vector<Vec2>& P = Piece.Points;
                if (P.size() == 1) {
                    AddDot(P.front(), Piece.Direction);
                    return;
                }

                const size_t SegmentCount = Piece.Closed ? P.size() : P.size() - 1;
                for (size_t I = 0; I < SegmentCount; ++I) {
                    Vec2 A           = P[I];
                    Vec2 B           = P[(I + 1) % P.size()];
                    const Vec2 Along = Normalized(B - A);
                    if (!Piece.Closed && _Stroke.Cap == LineCap::Square) {
                        if (I == 0) { A = A - Along * _HalfWidth; }
                        if (I == SegmentCount - 1) { B = B + Along * _HalfWidth; }
                    }
                    const Vec2 Offset = Perpendicular(Along) * _HalfWidth;
                    _Output.push_back({A + Offset, B + Offset, B - Offset, A - Offset});
                }

                const size_t First = Piece.Closed ? 0 : 1;
                const size_t Last  = Piece.Closed ? P.size() : P.size() - 1;
                for (size_t I = First; I < Last; ++I) {
                    const Vec2& Vertex = P[I];
                    const Vec2& Before = P[(I + P.size() - 1) % P.size()];
                    const Vec2& After  = P[(I + 1) % P.size()];
                    AddJoin(Vertex, Normalized(Vertex - Before), Normalized(After - Vertex));
                }

                if (!Piece.Closed) {
                    AddCap(P.front(), Normalized(P.front() - P[1]));
                    AddCap(P.back(), Normalized(P.back() - P[P.size() - 2]));
                }
            }

            std::vector<Contour> Take() { return std::move(_Output); }

        private:
            void AddDot(const Vec2& Center, const Vec2& Direction) {
                switch (_Stroke.Cap) {
                    case LineCap::Round:
                        AddCircle(Center);
                        break;
                    case LineCap::Square: {
                        const Vec2 Along  = Normalized(Direction) * _HalfWidth;
                        const Vec2 Across = Perpendicular(Along);
                        _Output.push_back({Center - Along + Across,
                                           Center + Along + Across,
                                           Center + Along - Across,
                                           Center - Along - Across});
                        break;
                    }
                    default:
                        break;
                }
            }

            /// @brief A cap on an open end. Outward points away from the line, past the end.
            void AddCap(const Vec2& End, const Vec2& Outward) {
                const Vec2 Across = Perpendicular(Outward) * _HalfWidth;
                switch (_Stroke.Cap) {
                    case LineCap::Round:
                        AddArc(End, Across, std::numbers::pi_v<float>);
                        break;
                    case LineCap::Triangle:
                        _Output.push_back({End + Across, End + Outward * _HalfWidth, End - Across});
                        break;
                    default:
                        break;
                }
            }

            void AddJoin(const Vec2& Vertex, const Vec2& In, const Vec2& Out) {
                const float Turn = Cross(In, Out);
                const float Cos  = std::clamp(Dot(In, Out), -1.0f, 1.0f);
                if (std::abs(Turn) <= kEpsilon && Cos > 0.0f) { return; }

                // The joint opens on the side away from the turn.
                const float Side  = Turn > 0.0f ? -1.0f : 1.0f;
                const Vec2 Before = Perpendicular(In) * (_HalfWidth * Side);
                const Vec2 After  = Perpendicular(Out) * (_HalfWidth * Side);

                if (_Stroke.Join == LineJoin::Round) {
                    const float Sweep = std::atan2(Cross(Before, After), Dot(Before, After));
                    AddArc(Vertex, Before, Sweep);
                    return;
                }

                if (_Stroke.Join == LineJoin::Bevel || Cos <= -1.0f + kEpsilon) {
                    _Output.push_back({Vertex, Vertex + Before, Vertex + After});
                    return;
                }

                const Vec2 Miter       = (Before + After) / (1.0f + Cos);
                const float MiterRatio = Length(Miter) / _HalfWidth;
                if (MiterRatio <= _Stroke.MiterLimit) {
                    _Output.push_back({Vertex, Vertex + Before, Vertex + Miter, Vertex + After});
                    return;
                }
                if (_Stroke.Join == LineJoin::MiterOrBevel || _Stroke.MiterLimit < 1.0f) {
                    _Output.push_back({Vertex, Vertex + Before, Vertex + After});
                    return;
                }

                // Direct2D's plain miter clips the spike at the limit instead of falling back to a bevel.
                const Vec2 Bisector = Normalized(Miter);
                const float Limit   = _Stroke.MiterLimit * _HalfWidth;
                const float AlongIn = Dot(In, Bisector);
                const float AlongOut = Dot(-Out, Bisector);
                if (AlongIn <= kEpsilon || AlongOut <= kEpsilon) {
                    _Output.push_back({Vertex, Vertex + Before, Vertex + After});
                    return;
                }
                const Vec2 ClipBefore = Vertex + Before + In * ((Limit - Dot(Before, Bisector)) / AlongIn);
                const Vec2 ClipAfter  = Vertex + After - Out * ((Limit - Dot(After, Bisector)) / AlongOut);
                _Output.push_back({Vertex, Vertex + Before, ClipBefore, ClipAfter, Vertex + After});
            }

            /// @brief A pie slice from Center, starting at Center + From and sweeping Sweep radians.
            void AddArc(const Vec2& Center, const Vec2& From, float Sweep) {
                const int Segments = ArcSegments(_HalfWidth, Sweep, _Tolerance);
                Contour Slice;
                Slice.reserve(static_cast<size_t>(Segments) + 2);
                Slice.push_back(Center);
                for (int I = 0; I <= Segments; ++I) {
                    Slice.push_back(Center + Rotate(From, Sweep * static_cast<float>(I) / static_cast<float>(Segments)));
                }
                _Output.push_back(std::move(Slice));
            }

            void AddCircle(const Vec2& Center) {
                const float Full   = 2.0f * std::numbers::pi_v<float>;
                const int Segments = std::max(8, ArcSegments(_HalfWidth, Full, _Tolerance));
                Contour Circle;
                Circle.reserve(static_cast<size_t>(Segments));
                for (int I = 0; I < Segments; ++I) {
                    Circle.push_back(Center + Rotate({_HalfWidth, 0.0f}, Full * static_cast<float>(I) / Segments));
                }
                _Output.push_back(std::move(Circle));
            }

            const StrokeStyle& _Stroke;
            float _HalfWidth;
            float _Tolerance;
            std::vector<Contour> _Output;
        };

        /// @brief Adds the signed area a line from P0 to P1 covers in each cell of one row band, following the
        /// accumulation scheme of font-rs: each row's running sum of Accumulator is the winding-weighted coverage.
        void AccumulateLine(std::vector<float>& Accumulator, int32_t Width, int32_t Height, Vec2 P0, Vec2 P1) {
            if (P0.Y == P1.Y) { return; }
            float Direction = 1.0f;
            if (P0.Y > P1.Y) {
                std::swap(P0, P1);
                Direction = -1.0f;
            }
            if (P1.Y <= 0.0f || P0.Y >= static_cast<float>(Height)) { return; }

            const float Stride = static_cast<float>(Width + 2);
            const float Slope  = (P1.X - P0.X) / (P1.Y - P0.Y);
            const int32_t RowStart = std::max(0, static_cast<int32_t>(std::floor(P0.Y)));
            const int32_t RowEnd   = std::min(Height, static_cast<int32_t>(std::ceil(P1.Y)));
            float X                = P0.X + (std::max(P0.Y, 0.0f) - P0.Y) * Slope;

            for (int32_t Row = RowStart; Row < RowEnd; ++Row) {
                const auto Line   = static_cast<size_t>(static_cast<float>(Row) * Stride);
                const float Top   = std::max(static_cast<float>(Row), P0.Y);
                const float Dy    = std::min(static_cast<float>(Row + 1), P1.Y) - Top;
                const float XNext = X + Slope * Dy;
                const float D     = Dy * Direction;
                const float X0    = std::min(X, XNext);
                const float X1    = std::max(X, XNext);
                const float X0Floor = std::floor(X0);
                const auto X0i      = static_cast<int32_t>(X0Floor);
                const auto X1i      = static_cast<int32_t>(std::ceil(X1));

                if (X1i <= X0i + 1) {
                    const float Mid = 0.5f * (X + XNext) - X0Floor;
                    Accumulator[Line + static_cast<size_t>(X0i)] += D - D * Mid;
                    Accumulator[Line + static_cast<size_t>(X0i) + 1] += D * Mid;
                } else {
                    const float Inverse = 1.0f / (X1 - X0);
                    const float X0f     = X0 - X0Floor;
                    const float A0      = 0.5f * Inverse * (1.0f - X0f) * (1.0f - X0f);
                    const float X1f     = X1 - static_cast<float>(X1i) + 1.0f;
                    const float AEnd    = 0.5f * Inverse * X1f * X1f;
                    Accumulator[Line + static_cast<size_t>(X0i)] += D * A0;
                    if (X1i == X0i + 2) {
                        Accumulator[Line + static_cast<size_t>(X0i) + 1] += D * (1.0f - A0 - AEnd);
                    } else {
                        const float A1 = Inverse * (1.5f - X0f);
                        Accumulator[Line + static_cast<size_t>(X0i) + 1] += D * (A1 - A0);
                        for (int32_t Column = X0i + 2; Column < X1i - 1; ++Column) {
                            Accumulator[Line + static_cast<size_t>(Column)] += D * Inverse;
                        }
                        const float A2 = A1 + static_cast<float>(X1i - X0i - 3) * Inverse;
                        Accumulator[Line + static_cast<size_t>(X1i) - 1] += D * (1.0f - A2 - AEnd);
                    }
                    Accumulator[Line + static_cast<size_t>(X1i)] += D * AEnd;
                }
                X = XNext;
            }
        }

        /// @brief Splits the line at the region's left and right edges and pins the outside parts to those edges,
        /// which keeps their winding contribution to the pixels inside exact.
        void AddClippedLine(std::vector<float>& Accumulator, int32_t Width, int32_t Height, Vec2 P0, Vec2 P1) {
            const float Right = static_cast<float>(Width);
            float Cuts[4]     = {0.0f, 0.0f, 0.0f, 1.0f};
            int CutCount      = 1;
            const float Dx    = P1.X - P0.X;
            if (std::abs(Dx) > kEpsilon) {
                for (const float Edge : {0.0f, Right}) {
                    const float T = (Edge - P0.X) / Dx;
                    if (T > 0.0f && T < 1.0f) { Cuts[CutCount++] = T; }
                }
            }
            Cuts[CutCount++] = 1.0f;
            std::sort(Cuts, Cuts + CutCount);

            for (int I = 0; I + 1 < CutCount; ++I) {
                Vec2 A = P0 + (P1 - P0) * Cuts[I];
                Vec2 B = P0 + (P1 - P0) * Cuts[I + 1];
                A.X    = std::clamp(A.X, 0.0f, Right);
                B.X    = std::clamp(B.X, 0.0f, Right);
                AccumulateLine(Accumulator, Width, Height, A, B);
            }
        }
    }  // namespace

    std::vector<Contour>
    StrokeFigures(const std::vector<PathData::Figure>& Figures, const StrokeStyle& Stroke, float Tolerance) {
        if (Stroke.Width <= 0.0f) { return {}; }

        Stroker Builder(Stroke, Tolerance);
        for (const Run& Piece : ApplyDashes(Figures, Stroke)) {
            Builder.Add(Piece);
        }

        std::vector<Contour> Result = Builder.Take();
        for (Contour& Shape : Result) {
            if (SignedArea(Shape) < 0.0f) { std::reverse(Shape.begin(), Shape.end()); }
        }
        return Result;
    }

    void TransformContours(std::vector<Contour>& Contours, const Matrix3x2& Transform) {
        for (Contour& Shape : Contours) {
            for (Vec2& Point : Shape) {
                Point = Transform.TransformPoint(Point);
            }
        }
    }

    PixelBox GetContourBounds(const std::vector<Contour>& Contours) {
        float Left   = std::numeric_limits<float>::max();
        float Top    = std::numeric_limits<float>::max();
        float Right  = std::numeric_limits<float>::lowest();
        float Bottom = std::numeric_limits<float>::lowest();
        for (const Contour& Shape : Contours) {
            for (const Vec2& Point : Shape) {
                Left   = std::min(Left, Point.X);
                Top    = std::min(Top, Point.Y);
                Right  = std::max(Right, Point.X);
                Bottom = std::max(Bottom, Point.Y);
            }
        }
        if (Left > Right || !std::isfinite(Left) || !std::isfinite(Right) || !std::isfinite(Top) ||
            !std::isfinite(Bottom)) {
            return {};
        }
        constexpr float kLimit = 1 << 24;
        const auto X0          = static_cast<int32_t>(std::floor(std::clamp(Left, -kLimit, kLimit)));
        const auto Y0          = static_cast<int32_t>(std::floor(std::clamp(Top, -kLimit, kLimit)));
        const auto X1          = static_cast<int32_t>(std::ceil(std::clamp(Right, -kLimit, kLimit)));
        const auto Y1          = static_cast<int32_t>(std::ceil(std::clamp(Bottom, -kLimit, kLimit)));
        return {X0, Y0, X1 - X0, Y1 - Y0};
    }

    void RasterizeCoverage(const std::vector<Contour>& Contours,
                           FillRule Rule,
                           const PixelBox& Region,
                           std::vector<uint8_t>& Mask) {
        Mask.assign(static_cast<size_t>(std::max(0, Region.Width)) * static_cast<size_t>(std::max(0, Region.Height)),
                    0);
        if (Region.IsEmpty()) { return; }

        const size_t Stride = static_cast<size_t>(Region.Width) + 2;
        std::vector<float> Accumulator(Stride * static_cast<size_t>(Region.Height), 0.0f);
        const Vec2 Origin {static_cast<float>(Region.X), static_cast<float>(Region.Y)};
        for (const Contour& Shape : Contours) {
            if (Shape.size() < 2) { continue; }
            for (size_t I = 0, J = Shape.size() - 1; I < Shape.size(); J = I++) {
                AddClippedLine(Accumulator, Region.Width, Region.Height, Shape[J] - Origin, Shape[I] - Origin);
            }
        }

        for (int32_t Row = 0; Row < Region.Height; ++Row) {
            const float* Cells = Accumulator.data() + static_cast<size_t>(Row) * Stride;
            uint8_t* Out       = Mask.data() + static_cast<size_t>(Row) * static_cast<size_t>(Region.Width);
            float Sum          = 0.0f;
            for (int32_t Column = 0; Column < Region.Width; ++Column) {
                Sum += Cells[Column];
                float Coverage = std::abs(Sum);
                if (Rule == FillRule::EvenOdd) {
                    Coverage = std::fmod(Coverage, 2.0f);
                    if (Coverage > 1.0f) { Coverage = 2.0f - Coverage; }
                } else {
                    Coverage = std::min(Coverage, 1.0f);
                }
                Out[Column] = static_cast<uint8_t>(Coverage * 255.0f + 0.5f);
            }
        }
    }
}  // namespace XenUI
