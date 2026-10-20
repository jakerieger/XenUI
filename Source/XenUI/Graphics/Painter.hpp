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
#include "XenUI/Graphics/DisplayList.hpp"

#include <optional>
#include <vector>

namespace XenUI {
    /// @brief Records draw commands into a DisplayList and tracks their bounds. Pushes and pops must be balanced.
    class XENUI_API Painter {
    public:
        /// @brief Appends to Target, which must outlive the painter.
        explicit Painter(DisplayList& Target);

        Painter(const Painter&)            = delete;
        Painter& operator=(const Painter&) = delete;

        void PushTransform(const Matrix3x2& Transform);
        void PopTransform();
        void PushClip(const Rect& Bounds);
        void PopClip();
        void PushOpacity(float Opacity);
        void PopOpacity();

        void FillRect(const Rect& Bounds, const Brush& Paint);
        void StrokeRect(const Rect& Bounds, const Brush& Paint, const StrokeStyle& Stroke = {});
        void FillRoundedRect(const Rect& Bounds, float Radius, const Brush& Paint);
        void StrokeRoundedRect(const Rect& Bounds, float Radius, const Brush& Paint, const StrokeStyle& Stroke = {});
        void FillEllipse(const Vec2& Center, const Vec2& Radius, const Brush& Paint);
        void StrokeEllipse(const Vec2& Center, const Vec2& Radius, const Brush& Paint, const StrokeStyle& Stroke = {});
        void FillCircle(const Vec2& Center, float Radius, const Brush& Paint);
        void StrokeCircle(const Vec2& Center, float Radius, const Brush& Paint, const StrokeStyle& Stroke = {});
        void DrawLine(const Vec2& Start, const Vec2& End, const Brush& Paint, const StrokeStyle& Stroke = {});
        void FillPath(const Path& Geometry, const Brush& Paint);
        void StrokePath(const Path& Geometry, const Brush& Paint, const StrokeStyle& Stroke = {});
        void DrawTextLayout(const TextLayout& Layout, const Vec2& Origin, const Brush& Paint);
        void DrawImage(const Image& Source, const Rect& Destination, const ImageDrawOptions& Options = {});

        [[nodiscard]] const Matrix3x2& GetTransform() const { return _Transform; }

    private:
        void IncludeBounds(const Rect& LocalBounds);

        DisplayList& _Target;
        Matrix3x2 _Transform;
        std::optional<Rect> _Clip;
        std::vector<Matrix3x2> _TransformStack;
        std::vector<std::optional<Rect>> _ClipStack;
    };
}  // namespace XenUI
