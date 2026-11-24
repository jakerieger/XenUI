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

namespace XenUI {
    Path::Path() = default;

    Path::Path(std::shared_ptr<const PathData> Data) : _Data(std::move(Data)) {}

    PathBuilder& PathBuilder::MoveTo(const Vec2& Point) {
        _Segments.push_back({.Kind = SegmentKind::Move, .Points = {Point}});
        return *this;
    }

    PathBuilder& PathBuilder::LineTo(const Vec2& Point) {
        _Segments.push_back({.Kind = SegmentKind::Line, .Points = {Point}});
        return *this;
    }

    PathBuilder& PathBuilder::QuadTo(const Vec2& Control, const Vec2& End) {
        _Segments.push_back({.Kind = SegmentKind::Quad, .Points = {Control, End}});
        return *this;
    }

    PathBuilder& PathBuilder::CubicTo(const Vec2& Control1, const Vec2& Control2, const Vec2& End) {
        _Segments.push_back({.Kind = SegmentKind::Cubic, .Points = {Control1, Control2, End}});
        return *this;
    }

    PathBuilder&
    PathBuilder::ArcTo(const Vec2& End, const Vec2& Radius, float RotationDegrees, ArcSweep Sweep, ArcSize Extent) {
        _Segments.push_back({
          .Kind     = SegmentKind::Arc,
          .Points   = {End},
          .Radius   = Radius,
          .Rotation = RotationDegrees,
          .Sweep    = Sweep,
          .Extent   = Extent,
        });
        return *this;
    }

    PathBuilder& PathBuilder::Close() {
        _Segments.push_back({.Kind = SegmentKind::Close});
        return *this;
    }

    PathBuilder& PathBuilder::SetFillRule(FillRule Rule) {
        _FillRule = Rule;
        return *this;
    }
}  // namespace XenUI
