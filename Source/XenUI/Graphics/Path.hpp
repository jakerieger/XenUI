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
#include "XenUI/Core/Math.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace XenUI {
    class PathData;

    enum class FillRule : uint8_t { NonZero, EvenOdd };
    enum class ArcSweep : uint8_t { CounterClockwise, Clockwise };
    enum class ArcSize : uint8_t { Small, Large };

    /// @brief Immutable vector geometry. Cheap to copy; copies share the same geometry. Build with PathBuilder.
    class XENUI_API Path {
    public:
        Path();

        [[nodiscard]] bool IsEmpty() const { return _Data == nullptr; }
        [[nodiscard]] Rect GetBounds() const;

        /// @internal
        [[nodiscard]] const PathData* GetData() const { return _Data.get(); }

    private:
        friend class PathBuilder;

        explicit Path(std::shared_ptr<const PathData> Data);

        std::shared_ptr<const PathData> _Data;
    };

    class XENUI_API PathBuilder {
    public:
        PathBuilder& MoveTo(const Vec2& Point);
        PathBuilder& LineTo(const Vec2& Point);
        PathBuilder& QuadTo(const Vec2& Control, const Vec2& End);
        PathBuilder& CubicTo(const Vec2& Control1, const Vec2& Control2, const Vec2& End);
        PathBuilder& ArcTo(const Vec2& End,
                           const Vec2& Radius,
                           float RotationDegrees = 0.0f,
                           ArcSweep Sweep        = ArcSweep::Clockwise,
                           ArcSize Extent        = ArcSize::Small);
        PathBuilder& Close();
        PathBuilder& SetFillRule(FillRule Rule);

        /// @brief Creates the geometry. Requires an App to exist.
        [[nodiscard]] Path Build() const;

    private:
        enum class SegmentKind : uint8_t { Move, Line, Quad, Cubic, Arc, Close };

        struct Segment {
            SegmentKind Kind = SegmentKind::Move;
            Vec2 Points[3] {};
            Vec2 Radius;
            float Rotation = 0.0f;
            ArcSweep Sweep = ArcSweep::Clockwise;
            ArcSize Extent = ArcSize::Small;
        };

        std::vector<Segment> _Segments;
        FillRule _FillRule = FillRule::NonZero;
    };
}  // namespace XenUI
