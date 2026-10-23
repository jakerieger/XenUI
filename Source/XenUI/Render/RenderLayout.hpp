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
#include "XenUI/Render/RenderBox.hpp"

#include <cstdint>
#include <optional>

namespace XenUI {
    enum class Axis : uint8_t { Horizontal, Vertical };
    enum class MainAxisAlignment : uint8_t { Start, End, Center, SpaceBetween, SpaceAround, SpaceEvenly };
    enum class CrossAxisAlignment : uint8_t { Start, End, Center, Stretch };
    enum class MainAxisSize : uint8_t { Min, Max };
    enum class FlexFit : uint8_t { Tight, Loose };
    enum class StackFit : uint8_t { Loose, Expand };

    class XENUI_API RenderPadding final : public RenderBox {
    public:
        void SetInsets(const EdgeInsets& Insets);

    protected:
        void PerformLayout() override;

    private:
        EdgeInsets _Insets;
    };

    /// @brief Imposes additional constraints on its child, within those of its parent.
    class XENUI_API RenderConstrainedBox final : public RenderBox {
    public:
        void SetAdditionalConstraints(const BoxConstraints& Additional);

    protected:
        void PerformLayout() override;

    private:
        BoxConstraints _Additional;
    };

    /// @brief Positions its child within itself. Expands to fill bounded axes and shrink-wraps unbounded ones.
    class XENUI_API RenderAlign final : public RenderBox {
    public:
        void SetAlignment(const Alignment& Position);
        void SetFactors(std::optional<float> WidthFactor, std::optional<float> HeightFactor);

    protected:
        void PerformLayout() override;

    private:
        Alignment _Alignment;
        std::optional<float> _WidthFactor;
        std::optional<float> _HeightFactor;
    };

    /// @brief Marks a flex child as sharing the free space along the main axis.
    class XENUI_API RenderFlexible final : public RenderBox {
    public:
        void SetFlex(int Flex, FlexFit Fit);

        [[nodiscard]] int GetFlex() const { return _Flex; }
        [[nodiscard]] FlexFit GetFit() const { return _Fit; }

    private:
        int _Flex    = 1;
        FlexFit _Fit = FlexFit::Tight;
    };

    /// @brief Lays children out in a line. Inflexible children are sized first; flexible ones divide the rest.
    class XENUI_API RenderFlex final : public RenderBox {
    public:
        void SetDirection(Axis Direction);
        void SetMainAxisAlignment(MainAxisAlignment Alignment);
        void SetCrossAxisAlignment(CrossAxisAlignment Alignment);
        void SetMainAxisSize(MainAxisSize Sizing);
        void SetSpacing(float Spacing);

    protected:
        void PerformLayout() override;

    private:
        Axis _Direction                        = Axis::Horizontal;
        MainAxisAlignment _MainAxisAlignment   = MainAxisAlignment::Start;
        CrossAxisAlignment _CrossAxisAlignment = CrossAxisAlignment::Center;
        MainAxisSize _MainAxisSize             = MainAxisSize::Max;
        float _Spacing                         = 0.0f;
    };

    /// @brief Places a stack child relative to the stack's edges.
    class XENUI_API RenderPositioned final : public RenderBox {
    public:
        struct Placement {
            std::optional<float> Left;
            std::optional<float> Top;
            std::optional<float> Right;
            std::optional<float> Bottom;
            std::optional<float> Width;
            std::optional<float> Height;

            bool operator==(const Placement&) const = default;
        };

        void SetPlacement(const Placement& NewPlacement);
        [[nodiscard]] const Placement& GetPlacement() const { return _Placement; }

    private:
        Placement _Placement;
    };

    /// @brief Overlays children. Unpositioned children determine the size and are aligned; positioned children
    /// are placed relative to the edges.
    class XENUI_API RenderStack final : public RenderBox {
    public:
        void SetDefaultAlignment(const Alignment& DefaultAlignment);
        void SetFit(StackFit Fit);

    protected:
        void PerformLayout() override;

    private:
        Alignment _DefaultAlignment = Alignment::TopLeft();
        StackFit _Fit               = StackFit::Loose;
    };

    enum class WrapAlignment : uint8_t { Start, Center, End, SpaceBetween };

    /// @brief Lays children out left to right, starting a new run below whenever the next child would not fit.
    class XENUI_API RenderWrap final : public RenderBox {
    public:
        void SetSpacing(float Spacing);
        void SetRunSpacing(float RunSpacing);
        void SetAlignment(WrapAlignment Alignment);
        void SetCrossAlignment(CrossAxisAlignment Alignment);

    protected:
        void PerformLayout() override;

    private:
        float _Spacing                     = 0.0f;
        float _RunSpacing                  = 0.0f;
        WrapAlignment _Alignment           = WrapAlignment::Start;
        CrossAxisAlignment _CrossAlignment = CrossAxisAlignment::Start;
    };

    /// @brief Lays children out in equal-width columns, row by row. Each row is as tall as its tallest child, or
    /// RowHeight when set.
    class XENUI_API RenderGrid final : public RenderBox {
    public:
        struct Configuration {
            int Columns          = 0;
            float MinColumnWidth = 120.0f;
            float Spacing        = 0.0f;
            float RunSpacing     = 0.0f;
            std::optional<float> RowHeight;

            bool operator==(const Configuration&) const = default;
        };

        void SetConfiguration(const Configuration& NewConfiguration);
        /// @brief The number of columns used by the last layout.
        [[nodiscard]] int GetColumnCount() const { return _ColumnCount; }

    protected:
        void PerformLayout() override;

    private:
        Configuration _Configuration;
        int _ColumnCount = 1;
    };

    class XENUI_API RenderRepaintBoundary final : public RenderBox {
    public:
        /// @brief Rasterizes the subtree into a cached bitmap, reused while nothing inside it changes.
        void SetCached(bool Cached);

        [[nodiscard]] bool IsRepaintBoundary() const override { return true; }
    };

    /// @brief Draws its subtree with uniform transparency. A repaint boundary, so changing the opacity only
    /// recomposites the existing layer.
    class XENUI_API RenderOpacity final : public RenderBox {
    public:
        void SetOpacity(float Opacity);

        [[nodiscard]] bool IsRepaintBoundary() const override { return true; }
    };
}  // namespace XenUI
