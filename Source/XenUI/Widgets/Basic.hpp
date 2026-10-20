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

#include "XenUI/Graphics/Brush.hpp"
#include "XenUI/Graphics/Image.hpp"
#include "XenUI/Graphics/Text.hpp"
#include "XenUI/Layout/BoxConstraints.hpp"
#include "XenUI/Render/RenderLayout.hpp"
#include "XenUI/Render/RenderVisual.hpp"
#include "XenUI/Tree/Widget.hpp"

#include <memory>
#include <optional>
#include <string>

namespace XenUI {
    struct Padding {
        EdgeInsets Insets;
        Node Child;

        [[nodiscard]] std::unique_ptr<RenderPadding> CreateRenderObject() const {
            auto Result = std::make_unique<RenderPadding>();
            UpdateRenderObject(*Result);
            return Result;
        }

        void UpdateRenderObject(RenderPadding& Target) const { Target.SetInsets(Insets); }
    };

    /// @brief Forces its child to a width and/or height. Unset axes pass the parent's constraints through.
    struct SizedBox {
        std::optional<float> Width;
        std::optional<float> Height;
        Node Child;

        [[nodiscard]] std::unique_ptr<RenderConstrainedBox> CreateRenderObject() const {
            auto Result = std::make_unique<RenderConstrainedBox>();
            UpdateRenderObject(*Result);
            return Result;
        }

        void UpdateRenderObject(RenderConstrainedBox& Target) const {
            Target.SetAdditionalConstraints(BoxConstraints::TightFor(Width, Height));
        }
    };

    struct ConstrainedBox {
        BoxConstraints Limits;
        Node Child;

        [[nodiscard]] std::unique_ptr<RenderConstrainedBox> CreateRenderObject() const {
            auto Result = std::make_unique<RenderConstrainedBox>();
            UpdateRenderObject(*Result);
            return Result;
        }

        void UpdateRenderObject(RenderConstrainedBox& Target) const { Target.SetAdditionalConstraints(Limits); }
    };

    struct Align {
        Alignment Position = Alignment::Center();
        Node Child;
        /// @brief Sizes the width to the child's times this factor, instead of filling the available width.
        std::optional<float> WidthFactor;
        /// @brief Sizes the height to the child's times this factor, instead of filling the available height.
        std::optional<float> HeightFactor;

        [[nodiscard]] std::unique_ptr<RenderAlign> CreateRenderObject() const {
            auto Result = std::make_unique<RenderAlign>();
            UpdateRenderObject(*Result);
            return Result;
        }

        void UpdateRenderObject(RenderAlign& Target) const {
            Target.SetAlignment(Position);
            Target.SetFactors(WidthFactor, HeightFactor);
        }
    };

    struct Center {
        Node Child;

        [[nodiscard]] Node Build(BuildContext& Context) const {
            (void)Context;
            return Align {.Position = Alignment::Center(), .Child = Child};
        }
    };

    struct Flex {
        Axis Direction                    = Axis::Horizontal;
        float Spacing                     = 0.0f;
        MainAxisAlignment MainAlignment   = MainAxisAlignment::Start;
        CrossAxisAlignment CrossAlignment = CrossAxisAlignment::Center;
        MainAxisSize MainSize             = MainAxisSize::Max;
        NodeList Children;

        [[nodiscard]] std::unique_ptr<RenderFlex> CreateRenderObject() const {
            auto Result = std::make_unique<RenderFlex>();
            UpdateRenderObject(*Result);
            return Result;
        }

        void UpdateRenderObject(RenderFlex& Target) const {
            Target.SetDirection(Direction);
            Target.SetSpacing(Spacing);
            Target.SetMainAxisAlignment(MainAlignment);
            Target.SetCrossAxisAlignment(CrossAlignment);
            Target.SetMainAxisSize(MainSize);
        }
    };

    /// @brief Horizontal Flex. Wrap children in Expanded or Flexible to share the remaining width.
    struct Row {
        float Spacing                     = 0.0f;
        MainAxisAlignment MainAlignment   = MainAxisAlignment::Start;
        CrossAxisAlignment CrossAlignment = CrossAxisAlignment::Center;
        MainAxisSize MainSize             = MainAxisSize::Max;
        NodeList Children;

        [[nodiscard]] Node Build(BuildContext& Context) const {
            (void)Context;
            return Flex {Axis::Horizontal, Spacing, MainAlignment, CrossAlignment, MainSize, Children};
        }
    };

    /// @brief Vertical Flex. Wrap children in Expanded or Flexible to share the remaining height.
    struct Column {
        float Spacing                     = 0.0f;
        MainAxisAlignment MainAlignment   = MainAxisAlignment::Start;
        CrossAxisAlignment CrossAlignment = CrossAxisAlignment::Center;
        MainAxisSize MainSize             = MainAxisSize::Max;
        NodeList Children;

        [[nodiscard]] Node Build(BuildContext& Context) const {
            (void)Context;
            return Flex {Axis::Vertical, Spacing, MainAlignment, CrossAlignment, MainSize, Children};
        }
    };

    /// @brief Flex child that may take up to its share of the free space.
    struct Flexible {
        int Flex    = 1;
        FlexFit Fit = FlexFit::Loose;
        Node Child;

        [[nodiscard]] std::unique_ptr<RenderFlexible> CreateRenderObject() const {
            auto Result = std::make_unique<RenderFlexible>();
            UpdateRenderObject(*Result);
            return Result;
        }

        void UpdateRenderObject(RenderFlexible& Target) const { Target.SetFlex(Flex, Fit); }
    };

    /// @brief Flex child that fills exactly its share of the free space.
    struct Expanded {
        int Flex = 1;
        Node Child;

        [[nodiscard]] std::unique_ptr<RenderFlexible> CreateRenderObject() const {
            auto Result = std::make_unique<RenderFlexible>();
            UpdateRenderObject(*Result);
            return Result;
        }

        void UpdateRenderObject(RenderFlexible& Target) const { Target.SetFlex(Flex, FlexFit::Tight); }
    };

    /// @brief Empty flex child that absorbs free space.
    struct Spacer {
        int Flex = 1;

        [[nodiscard]] Node Build(BuildContext& Context) const {
            (void)Context;
            return Expanded {.Flex = Flex, .Child = SizedBox {}};
        }
    };

    struct Stack {
        Alignment DefaultAlignment = Alignment::TopLeft();
        StackFit Fit               = StackFit::Loose;
        NodeList Children;

        [[nodiscard]] std::unique_ptr<RenderStack> CreateRenderObject() const {
            auto Result = std::make_unique<RenderStack>();
            UpdateRenderObject(*Result);
            return Result;
        }

        void UpdateRenderObject(RenderStack& Target) const {
            Target.SetDefaultAlignment(DefaultAlignment);
            Target.SetFit(Fit);
        }
    };

    /// @brief Stack child placed by distances from the stack's edges.
    struct Positioned {
        std::optional<float> Left;
        std::optional<float> Top;
        std::optional<float> Right;
        std::optional<float> Bottom;
        std::optional<float> Width;
        std::optional<float> Height;
        Node Child;

        [[nodiscard]] std::unique_ptr<RenderPositioned> CreateRenderObject() const {
            auto Result = std::make_unique<RenderPositioned>();
            UpdateRenderObject(*Result);
            return Result;
        }

        void UpdateRenderObject(RenderPositioned& Target) const {
            Target.SetPlacement({Left, Top, Right, Bottom, Width, Height});
        }
    };

    /// @brief Isolates its subtree into a separate layer. Use around content that repaints often (meters,
    /// playheads) or rarely (static panels next to animated ones). Cached additionally rasterizes the subtree.
    /// @brief Draws its child with uniform transparency, from 0 (invisible) to 1.
    struct Opacity {
        float Value = 1.0f;
        Node Child;

        [[nodiscard]] std::unique_ptr<RenderOpacity> CreateRenderObject() const {
            auto Result = std::make_unique<RenderOpacity>();
            UpdateRenderObject(*Result);
            return Result;
        }

        void UpdateRenderObject(RenderOpacity& Target) const { Target.SetOpacity(Value); }
    };

    struct RepaintBoundary {
        bool Cached = false;
        Node Child;

        [[nodiscard]] std::unique_ptr<RenderRepaintBoundary> CreateRenderObject() const {
            auto Result = std::make_unique<RenderRepaintBoundary>();
            UpdateRenderObject(*Result);
            return Result;
        }

        void UpdateRenderObject(RenderRepaintBoundary& Target) const { Target.SetCached(Cached); }
    };

    struct DecoratedBox {
        std::optional<Brush> Background;
        float CornerRadius = 0.0f;
        std::optional<Color> BorderColor;
        float BorderWidth = 1.0f;
        Node Child;

        [[nodiscard]] std::unique_ptr<RenderDecoratedBox> CreateRenderObject() const {
            auto Result = std::make_unique<RenderDecoratedBox>();
            UpdateRenderObject(*Result);
            return Result;
        }

        void UpdateRenderObject(RenderDecoratedBox& Target) const {
            Target.SetDecoration({Background, CornerRadius, BorderColor, BorderWidth});
        }
    };

    /// @brief Convenience combination of margin, size, decoration, padding and alignment around a child.
    struct Container {
        std::optional<float> Width;
        std::optional<float> Height;
        EdgeInsets Margin;
        EdgeInsets Padding;
        std::optional<Brush> Background;
        float CornerRadius = 0.0f;
        std::optional<Color> BorderColor;
        float BorderWidth = 1.0f;
        std::optional<Alignment> ChildAlignment;
        Node Child;

        [[nodiscard]] Node Build(BuildContext& Context) const {
            (void)Context;
            Node Current = Child;
            if (ChildAlignment && !Current.IsEmpty()) {
                Current = Align {.Position = *ChildAlignment, .Child = Current};
            }
            if (!Padding.IsZero()) { Current = XenUI::Padding {.Insets = Padding, .Child = Current}; }
            if (Background || BorderColor) {
                Current = DecoratedBox {
                  .Background   = Background,
                  .CornerRadius = CornerRadius,
                  .BorderColor  = BorderColor,
                  .BorderWidth  = BorderWidth,
                  .Child        = Current,
                };
            }
            if (Width || Height || Current.IsEmpty()) {
                Current = SizedBox {.Width = Width, .Height = Height, .Child = Current};
            }
            if (!Margin.IsZero()) { Current = XenUI::Padding {.Insets = Margin, .Child = Current}; }
            return Current;
        }
    };

    /// @brief Default font and color for Text widgets below it. Provide with `Provide<TextStyle>`.
    struct TextStyle {
        XenUI::Font Font;
        XenUI::Color Color = XenUI::Color::FromHex(0xE6E6EB);

        bool operator==(const TextStyle&) const = default;
    };

    struct Paragraph {
        std::string Content;
        ParagraphStyle Style;

        [[nodiscard]] std::unique_ptr<RenderParagraph> CreateRenderObject() const {
            auto Result = std::make_unique<RenderParagraph>();
            UpdateRenderObject(*Result);
            return Result;
        }

        void UpdateRenderObject(RenderParagraph& Target) const {
            Target.SetText(Content);
            Target.SetStyle(Style);
        }
    };

    /// @brief Text using the nearest TextStyle for anything not set here.
    struct Text {
        std::string Content;
        std::optional<XenUI::Font> Font;
        std::optional<XenUI::Color> Color;
        TextAlignment Alignment = TextAlignment::Leading;
        TextWrapping Wrapping   = TextWrapping::Wrap;
        TextTrimming Trimming   = TextTrimming::None;

        [[nodiscard]] Node Build(BuildContext& Context) const {
            const TextStyle* Inherited = Context.Find<TextStyle>();
            const TextStyle Defaults;
            const TextStyle& Base = Inherited != nullptr ? *Inherited : Defaults;
            return Paragraph {
              .Content = Content,
              .Style =
                {
                  .TextFont   = Font.value_or(Base.Font),
                  .Foreground = Color.value_or(Base.Color),
                  .Options    = {.Alignment = Alignment, .Wrapping = Wrapping, .Trimming = Trimming},
                },
            };
        }
    };

    struct ImageView {
        XenUI::Image Source;
        ImageFit Fit = ImageFit::Contain;
        std::optional<float> Width;
        std::optional<float> Height;
        ImageInterpolation Interpolation = ImageInterpolation::Linear;
        float Opacity                    = 1.0f;

        [[nodiscard]] std::unique_ptr<RenderImage> CreateRenderObject() const {
            auto Result = std::make_unique<RenderImage>();
            UpdateRenderObject(*Result);
            return Result;
        }

        void UpdateRenderObject(RenderImage& Target) const {
            Target.SetImage(Source);
            Target.SetStyle({Fit, Width, Height, Interpolation, Opacity});
        }
    };

    /// @brief Paints with a callback in local coordinates. Signals read inside OnPaint repaint just this widget.
    struct CustomPaint {
        PaintCallback OnPaint;
        std::optional<float> Width;
        std::optional<float> Height;
        Node Child;

        [[nodiscard]] std::unique_ptr<RenderCustomPaint> CreateRenderObject() const {
            auto Result = std::make_unique<RenderCustomPaint>();
            UpdateRenderObject(*Result);
            return Result;
        }

        void UpdateRenderObject(RenderCustomPaint& Target) const {
            Target.SetCallback(OnPaint);
            Target.SetPreferredSize(Width, Height);
        }
    };
}  // namespace XenUI
