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

#include "XenUI/Core/Math.hpp"
#include "XenUI/Core/Matrix.hpp"
#include "XenUI/Graphics/Brush.hpp"
#include "XenUI/Graphics/Image.hpp"
#include "XenUI/Graphics/Path.hpp"
#include "XenUI/Graphics/StrokeStyle.hpp"
#include "XenUI/Graphics/Text.hpp"

#include <optional>
#include <variant>
#include <vector>

namespace XenUI {
    struct ImageDrawOptions {
        /// @brief Region of the image to draw, in image pixels. The whole image when unset.
        std::optional<Rect> SourceRect;
        float Opacity                    = 1.0f;
        ImageInterpolation Interpolation = ImageInterpolation::Linear;
    };

    struct PushTransformCommand {
        Matrix3x2 Transform;
    };

    struct PopTransformCommand {};

    struct PushClipCommand {
        Rect Bounds;
    };

    struct PopClipCommand {};

    struct PushOpacityCommand {
        float Opacity = 1.0f;
    };

    struct PopOpacityCommand {};

    struct FillRectCommand {
        Rect Bounds;
        Brush Paint;
    };

    struct StrokeRectCommand {
        Rect Bounds;
        Brush Paint;
        StrokeStyle Stroke;
    };

    struct FillRoundedRectCommand {
        Rect Bounds;
        float Radius = 0.0f;
        Brush Paint;
    };

    struct StrokeRoundedRectCommand {
        Rect Bounds;
        float Radius = 0.0f;
        Brush Paint;
        StrokeStyle Stroke;
    };

    struct FillEllipseCommand {
        Vec2 Center;
        Vec2 Radius;
        Brush Paint;
    };

    struct StrokeEllipseCommand {
        Vec2 Center;
        Vec2 Radius;
        Brush Paint;
        StrokeStyle Stroke;
    };

    struct DrawLineCommand {
        Vec2 Start;
        Vec2 End;
        Brush Paint;
        StrokeStyle Stroke;
    };

    struct FillPathCommand {
        Path Geometry;
        Brush Paint;
    };

    struct StrokePathCommand {
        Path Geometry;
        Brush Paint;
        StrokeStyle Stroke;
    };

    struct DrawTextLayoutCommand {
        TextLayout Layout;
        Vec2 Origin;
        Brush Paint;
    };

    struct DrawImageCommand {
        Image Source;
        Rect Destination;
        ImageDrawOptions Options;
    };

    using DrawCommand = std::variant<PushTransformCommand,
                                     PopTransformCommand,
                                     PushClipCommand,
                                     PopClipCommand,
                                     PushOpacityCommand,
                                     PopOpacityCommand,
                                     FillRectCommand,
                                     StrokeRectCommand,
                                     FillRoundedRectCommand,
                                     StrokeRoundedRectCommand,
                                     FillEllipseCommand,
                                     StrokeEllipseCommand,
                                     DrawLineCommand,
                                     FillPathCommand,
                                     StrokePathCommand,
                                     DrawTextLayoutCommand,
                                     DrawImageCommand>;

    /// @brief Recorded sequence of draw commands in device-independent pixels. Record with Painter.
    class DisplayList {
    public:
        [[nodiscard]] const std::vector<DrawCommand>& GetCommands() const { return _Commands; }

        /// @brief Conservative bounds of everything the list draws, in its own coordinate space.
        [[nodiscard]] const Rect& GetBounds() const { return _Bounds; }

        [[nodiscard]] bool IsEmpty() const { return _Commands.empty(); }

        void Clear() {
            _Commands.clear();
            _Bounds = {};
        }

    private:
        friend class Painter;

        std::vector<DrawCommand> _Commands;
        Rect _Bounds;
    };
}  // namespace XenUI
