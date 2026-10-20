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
#include "XenUI/Graphics/Brush.hpp"
#include "XenUI/Graphics/Image.hpp"
#include "XenUI/Graphics/Text.hpp"
#include "XenUI/Reactive/Reactive.hpp"
#include "XenUI/Render/RenderBox.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace XenUI {
    struct BoxDecoration {
        std::optional<Brush> Background;
        float CornerRadius = 0.0f;
        std::optional<Color> BorderColor;
        float BorderWidth = 1.0f;
    };

    /// @brief Paints a background and border behind its child.
    class XENUI_API RenderDecoratedBox final : public RenderBox {
    public:
        void SetDecoration(BoxDecoration Decoration);
        void Paint(PaintContext& Context) override;

    protected:
        [[nodiscard]] bool HitTestSelf(const Vec2& Position) const override;

    private:
        BoxDecoration _Decoration;
    };

    struct ParagraphStyle {
        Font TextFont;
        Color Foreground;
        TextOptions Options;

        bool operator==(const ParagraphStyle&) const = default;
    };

    /// @brief Lays out and paints a block of text, wrapping at the available width when enabled.
    class XENUI_API RenderParagraph final : public RenderBox {
    public:
        void SetText(std::string Content);
        void SetStyle(const ParagraphStyle& Style);
        void Paint(PaintContext& Context) override;

        [[nodiscard]] const TextLayout& GetTextLayout() const { return _Layout; }
        [[nodiscard]] const std::string& GetText() const { return _Content; }
        [[nodiscard]] bool DescribeSemantics(SemanticsProperties& Properties) const override;

    protected:
        void PerformLayout() override;
        [[nodiscard]] bool HitTestSelf(const Vec2& Position) const override;

    private:
        void InvalidateText();

        std::string _Content;
        ParagraphStyle _Style;
        TextLayout _Layout;
        float _LayoutWidth = -1.0f;
    };

    enum class ImageFit : uint8_t { Fill, Contain, Cover, ScaleDown, None };

    struct ImageStyle {
        ImageFit Fit = ImageFit::Contain;
        std::optional<float> Width;
        std::optional<float> Height;
        ImageInterpolation Interpolation = ImageInterpolation::Linear;
        float Opacity                    = 1.0f;

        bool operator==(const ImageStyle&) const = default;
    };

    /// @brief Draws an image fitted into its box. Sizes to the image's pixel dimensions unless told otherwise.
    class XENUI_API RenderImage final : public RenderBox {
    public:
        void SetImage(const Image& Source);
        void SetStyle(const ImageStyle& Style);
        void Paint(PaintContext& Context) override;

    protected:
        void PerformLayout() override;
        [[nodiscard]] bool HitTestSelf(const Vec2& Position) const override;

    private:
        Image _Source;
        ImageStyle _Style;
    };

    using PaintCallback = std::function<void(Painter& Canvas, const Size& Extent)>;

    /// @brief Paints with a callback. Signals read while painting are tracked: changing one repaints this box
    /// without rebuilding or relaying out anything.
    class XENUI_API RenderCustomPaint final : public RenderBox,
                                              public Observer {
    public:
        void SetCallback(PaintCallback Callback);
        void SetPreferredSize(std::optional<float> Width, std::optional<float> Height);
        void Paint(PaintContext& Context) override;
        void OnDependencyChanged() override;

    protected:
        void PerformLayout() override;
        [[nodiscard]] bool HitTestSelf(const Vec2& Position) const override;

    private:
        PaintCallback _Callback;
        std::optional<float> _PreferredWidth;
        std::optional<float> _PreferredHeight;
    };
}  // namespace XenUI
