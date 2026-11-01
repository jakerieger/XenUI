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
#include "XenUI/Core/Color.hpp"
#include "XenUI/Core/Math.hpp"

#include <memory>
#include <string_view>

namespace XenUI {
    class Painter;
    struct SvgIconData;

    /// @brief A single-color vector icon read from SVG, such as the icons of Lucide, Material Symbols, Fluent System
    /// Icons or Heroicons. Every painted shape is drawn in the color the icon is drawn with, so one icon follows the
    /// theme and its states; opacity attributes are kept, so duotone icons still work.
    ///
    /// Supported: the viewBox; path, rect, circle, ellipse, line, polyline and polygon elements, inside nested groups;
    /// the fill, stroke, stroke-width, stroke-linecap, stroke-linejoin, stroke-miterlimit, fill-rule, opacity,
    /// fill-opacity and stroke-opacity attributes, also inside a style attribute; and transform. Gradients, patterns,
    /// masks, clip paths, text and images are ignored.
    ///
    /// Parsing needs no App; the geometry is created the first time the icon is drawn. Cheap to copy; copies share
    /// the parsed icon.
    class XENUI_API SvgIcon {
    public:
        SvgIcon();

        /// @brief Parses an SVG document. Throws Error if it isn't SVG or its path data is malformed.
        [[nodiscard]] static SvgIcon Parse(std::string_view Markup);
        /// @brief Reads and parses an .svg file. Throws Error if it can't be read or parsed.
        [[nodiscard]] static SvgIcon FromFile(std::string_view FilePath);
        /// @brief Parses an RCDATA resource of the executable, such as `IDI_PLAY RCDATA "Icons/play.svg"` in an .rc
        /// file. Throws Error if there is no such resource or it can't be parsed.
        [[nodiscard]] static SvgIcon FromResource(int ResourceId);
        /// @brief An icon made of one filled path, from the value of a path's d attribute.
        [[nodiscard]] static SvgIcon FromPathData(std::string_view PathData, const Size& ViewBox = {24.0f, 24.0f});

        [[nodiscard]] bool IsEmpty() const;
        /// @brief The icon's coordinate space, which is scaled to fit wherever it is drawn.
        [[nodiscard]] Rect GetViewBox() const;

        /// @brief Draws the icon in Tint, scaled uniformly to fit Bounds and centered in it.
        void Draw(Painter& Canvas, const Rect& Bounds, const Color& Tint) const;

        /// @internal Identity of the parsed icon, for change detection.
        [[nodiscard]] const SvgIconData* GetData() const { return _Data.get(); }

        bool operator==(const SvgIcon& Other) const { return _Data == Other._Data; }

    private:
        explicit SvgIcon(std::shared_ptr<const SvgIconData> Data);

        std::shared_ptr<const SvgIconData> _Data;
    };
}  // namespace XenUI
