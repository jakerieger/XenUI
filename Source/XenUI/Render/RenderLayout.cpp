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

#include "XenUI/Render/RenderLayout.hpp"

#include <algorithm>
#include <cmath>

namespace XenUI {
    namespace {
        void MarkParentNeedsLayout(RenderBox& Box) {
            if (auto* Parent = dynamic_cast<RenderBox*>(Box.GetParent())) { Parent->MarkNeedsLayout(); }
        }

        struct AxisMapper {
            bool Horizontal;

            [[nodiscard]] float Main(const Size& Value) const { return Horizontal ? Value.Width : Value.Height; }
            [[nodiscard]] float Cross(const Size& Value) const { return Horizontal ? Value.Height : Value.Width; }

            [[nodiscard]] Size MakeSize(float MainExtent, float CrossExtent) const {
                return Horizontal ? Size {MainExtent, CrossExtent} : Size {CrossExtent, MainExtent};
            }

            [[nodiscard]] Vec2 MakeOffset(float MainPosition, float CrossPosition) const {
                return Horizontal ? Vec2 {MainPosition, CrossPosition} : Vec2 {CrossPosition, MainPosition};
            }

            [[nodiscard]] BoxConstraints
            MakeConstraints(float MinMain, float MaxMain, float MinCross, float MaxCross) const {
                return Horizontal ? BoxConstraints {MinMain, MaxMain, MinCross, MaxCross}
                                  : BoxConstraints {MinCross, MaxCross, MinMain, MaxMain};
            }
        };
    }  // namespace

    void RenderPadding::SetInsets(const EdgeInsets& Insets) {
        if (Insets == _Insets) { return; }
        _Insets = Insets;
        MarkNeedsLayout();
    }

    void RenderPadding::PerformLayout() {
        const BoxConstraints& Constraints = GetConstraints();
        const float Horizontal            = _Insets.GetHorizontal();
        const float Vertical              = _Insets.GetVertical();

        RenderBox* Child = GetFirstChild();
        if (Child == nullptr) {
            SetSize(Constraints.Constrain({Horizontal, Vertical}));
            return;
        }

        Child->Layout(Constraints.Deflate(_Insets), true);
        Child->SetOffset({_Insets.Left, _Insets.Top});
        const Size& ChildSize = Child->GetSize();
        SetSize(Constraints.Constrain({ChildSize.Width + Horizontal, ChildSize.Height + Vertical}));
    }

    void RenderConstrainedBox::SetAdditionalConstraints(const BoxConstraints& Additional) {
        if (Additional == _Additional) { return; }
        _Additional = Additional;
        MarkNeedsLayout();
    }

    void RenderConstrainedBox::PerformLayout() {
        const BoxConstraints Effective = _Additional.Enforce(GetConstraints());
        if (RenderBox* Child = GetFirstChild()) {
            Child->Layout(Effective, true);
            Child->SetOffset({});
            SetSize(Child->GetSize());
            return;
        }
        SetSize(Effective.Constrain({}));
    }

    void RenderAlign::SetAlignment(const Alignment& Position) {
        if (Position == _Alignment) { return; }
        _Alignment = Position;
        MarkNeedsLayout();
    }

    void RenderAlign::SetFactors(std::optional<float> WidthFactor, std::optional<float> HeightFactor) {
        if (WidthFactor == _WidthFactor && HeightFactor == _HeightFactor) { return; }
        _WidthFactor  = WidthFactor;
        _HeightFactor = HeightFactor;
        MarkNeedsLayout();
    }

    void RenderAlign::PerformLayout() {
        const BoxConstraints& Constraints = GetConstraints();
        RenderBox* Child                  = GetFirstChild();
        if (Child == nullptr) {
            SetSize(Constraints.Constrain(Constraints.GetBiggest()));
            return;
        }

        Child->Layout(Constraints.Loosen(), true);
        const Size& ChildSize = Child->GetSize();
        const float Width     = _WidthFactor ? ChildSize.Width * *_WidthFactor
                                             : (Constraints.HasBoundedWidth() ? Constraints.MaxWidth : ChildSize.Width);
        const float Height    = _HeightFactor
                                  ? ChildSize.Height * *_HeightFactor
                                  : (Constraints.HasBoundedHeight() ? Constraints.MaxHeight : ChildSize.Height);
        SetSize(Constraints.Constrain({Width, Height}));
        Child->SetOffset(_Alignment.Place(ChildSize, GetSize()));
    }

    void RenderFlexible::SetFlex(int Flex, FlexFit Fit) {
        if (Flex == _Flex && Fit == _Fit) { return; }
        _Flex = Flex;
        _Fit  = Fit;
        MarkParentNeedsLayout(*this);
    }

    void RenderFlex::SetDirection(Axis Direction) {
        if (Direction == _Direction) { return; }
        _Direction = Direction;
        MarkNeedsLayout();
    }

    void RenderFlex::SetMainAxisAlignment(MainAxisAlignment Alignment) {
        if (Alignment == _MainAxisAlignment) { return; }
        _MainAxisAlignment = Alignment;
        MarkNeedsLayout();
    }

    void RenderFlex::SetCrossAxisAlignment(CrossAxisAlignment Alignment) {
        if (Alignment == _CrossAxisAlignment) { return; }
        _CrossAxisAlignment = Alignment;
        MarkNeedsLayout();
    }

    void RenderFlex::SetMainAxisSize(MainAxisSize Sizing) {
        if (Sizing == _MainAxisSize) { return; }
        _MainAxisSize = Sizing;
        MarkNeedsLayout();
    }

    void RenderFlex::SetSpacing(float Spacing) {
        if (Spacing == _Spacing) { return; }
        _Spacing = Spacing;
        MarkNeedsLayout();
    }

    void RenderFlex::PerformLayout() {
        const BoxConstraints& Constraints = GetConstraints();
        const AxisMapper Axes {_Direction == Axis::Horizontal};

        const float MinMain   = Axes.Horizontal ? Constraints.MinWidth : Constraints.MinHeight;
        const float MaxMain   = Axes.Horizontal ? Constraints.MaxWidth : Constraints.MaxHeight;
        const float MinCross  = Axes.Horizontal ? Constraints.MinHeight : Constraints.MinWidth;
        const float MaxCross  = Axes.Horizontal ? Constraints.MaxHeight : Constraints.MaxWidth;
        const bool CanFlex    = std::isfinite(MaxMain);
        const bool Stretch    = _CrossAxisAlignment == CrossAxisAlignment::Stretch && std::isfinite(MaxCross);
        const size_t Count    = GetChildCount();
        const float TotalGaps = Count > 1 ? _Spacing * static_cast<float>(Count - 1) : 0.0f;

        const float ChildMinCross = Stretch ? MaxCross : 0.0f;
        const auto GetFlex        = [&](RenderBox& Child) {
            const auto* Flexible = dynamic_cast<RenderFlexible*>(&Child);
            return Flexible != nullptr && CanFlex ? std::max(Flexible->GetFlex(), 0) : 0;
        };

        float Allocated    = 0.0f;
        float LargestCross = 0.0f;
        int TotalFlex      = 0;
        for (size_t i = 0; i < Count; ++i) {
            RenderBox& Child = GetChild(i);
            if (const int Flex = GetFlex(Child); Flex > 0) {
                TotalFlex += Flex;
                continue;
            }
            Child.Layout(Axes.MakeConstraints(0.0f, kUnbounded, ChildMinCross, MaxCross), true);
            Allocated += Axes.Main(Child.GetSize());
            LargestCross = std::max(LargestCross, Axes.Cross(Child.GetSize()));
        }

        if (TotalFlex > 0) {
            const float FreeSpace = std::max(0.0f, MaxMain - Allocated - TotalGaps);
            const float PerFlex   = FreeSpace / static_cast<float>(TotalFlex);
            for (size_t i = 0; i < Count; ++i) {
                RenderBox& Child = GetChild(i);
                const int Flex   = GetFlex(Child);
                if (Flex == 0) { continue; }

                const float Share  = PerFlex * static_cast<float>(Flex);
                const bool IsTight = static_cast<RenderFlexible&>(Child).GetFit() == FlexFit::Tight;
                Child.Layout(Axes.MakeConstraints(IsTight ? Share : 0.0f, Share, ChildMinCross, MaxCross), true);
                Allocated += Axes.Main(Child.GetSize());
                LargestCross = std::max(LargestCross, Axes.Cross(Child.GetSize()));
            }
        }

        const float DesiredMain  = _MainAxisSize == MainAxisSize::Max && CanFlex ? MaxMain : Allocated + TotalGaps;
        const float DesiredCross = Stretch ? MaxCross : LargestCross;
        SetSize(Constraints.Constrain(Axes.MakeSize(std::max(DesiredMain, MinMain), std::max(DesiredCross, MinCross))));

        const float MainExtent  = Axes.Main(GetSize());
        const float CrossExtent = Axes.Cross(GetSize());
        const float Remaining   = std::max(0.0f, MainExtent - Allocated - TotalGaps);
        const float CountValue  = static_cast<float>(Count);

        float Leading = 0.0f;
        float Between = _Spacing;
        switch (_MainAxisAlignment) {
            case MainAxisAlignment::End:
                Leading = Remaining;
                break;
            case MainAxisAlignment::Center:
                Leading = Remaining * 0.5f;
                break;
            case MainAxisAlignment::SpaceBetween:
                Between += Count > 1 ? Remaining / (CountValue - 1.0f) : 0.0f;
                break;
            case MainAxisAlignment::SpaceAround:
                Leading = Count > 0 ? Remaining / CountValue * 0.5f : 0.0f;
                Between += Count > 0 ? Remaining / CountValue : 0.0f;
                break;
            case MainAxisAlignment::SpaceEvenly:
                Leading = Remaining / (CountValue + 1.0f);
                Between += Leading;
                break;
            default:
                break;
        }

        float Cursor = Leading;
        for (size_t i = 0; i < Count; ++i) {
            RenderBox& Child       = GetChild(i);
            const float ChildCross = Axes.Cross(Child.GetSize());
            float CrossPosition    = 0.0f;
            if (_CrossAxisAlignment == CrossAxisAlignment::End) {
                CrossPosition = CrossExtent - ChildCross;
            } else if (_CrossAxisAlignment == CrossAxisAlignment::Center) {
                CrossPosition = (CrossExtent - ChildCross) * 0.5f;
            }
            Child.SetOffset(Axes.MakeOffset(Cursor, CrossPosition));
            Cursor += Axes.Main(Child.GetSize()) + Between;
        }
    }

    void RenderPositioned::SetPlacement(const Placement& NewPlacement) {
        if (NewPlacement == _Placement) { return; }
        _Placement = NewPlacement;
        MarkParentNeedsLayout(*this);
    }

    void RenderStack::SetDefaultAlignment(const Alignment& DefaultAlignment) {
        if (DefaultAlignment == _DefaultAlignment) { return; }
        _DefaultAlignment = DefaultAlignment;
        MarkNeedsLayout();
    }

    void RenderStack::SetFit(StackFit Fit) {
        if (Fit == _Fit) { return; }
        _Fit = Fit;
        MarkNeedsLayout();
    }

    void RenderStack::PerformLayout() {
        const BoxConstraints& Constraints = GetConstraints();
        const BoxConstraints Unpositioned =
          _Fit == StackFit::Expand ? BoxConstraints::Tight(Constraints.GetBiggest()) : Constraints.Loosen();

        Size Largest;
        bool HasUnpositioned = false;
        for (size_t i = 0; i < GetChildCount(); ++i) {
            RenderBox& Child = GetChild(i);
            if (dynamic_cast<RenderPositioned*>(&Child) != nullptr) { continue; }
            Child.Layout(Unpositioned, true);
            Largest.Width   = std::max(Largest.Width, Child.GetSize().Width);
            Largest.Height  = std::max(Largest.Height, Child.GetSize().Height);
            HasUnpositioned = true;
        }
        SetSize(Constraints.Constrain(HasUnpositioned ? Largest : Constraints.GetBiggest()));
        const Size& Extent = GetSize();

        for (size_t i = 0; i < GetChildCount(); ++i) {
            RenderBox& Child       = GetChild(i);
            const auto* Positioned = dynamic_cast<RenderPositioned*>(&Child);
            if (Positioned == nullptr) {
                Child.SetOffset(_DefaultAlignment.Place(Child.GetSize(), Extent));
                continue;
            }

            const RenderPositioned::Placement& Place = Positioned->GetPlacement();
            BoxConstraints ChildConstraints;
            if (Place.Left && Place.Right) {
                const float Width         = std::max(0.0f, Extent.Width - *Place.Left - *Place.Right);
                ChildConstraints.MinWidth = ChildConstraints.MaxWidth = Width;
            } else if (Place.Width) {
                ChildConstraints.MinWidth = ChildConstraints.MaxWidth = *Place.Width;
            }
            if (Place.Top && Place.Bottom) {
                const float Height         = std::max(0.0f, Extent.Height - *Place.Top - *Place.Bottom);
                ChildConstraints.MinHeight = ChildConstraints.MaxHeight = Height;
            } else if (Place.Height) {
                ChildConstraints.MinHeight = ChildConstraints.MaxHeight = *Place.Height;
            }
            Child.Layout(ChildConstraints, true);

            const Size& ChildSize = Child.GetSize();
            const Vec2 Aligned    = _DefaultAlignment.Place(ChildSize, Extent);
            const float X         = Place.Left    ? *Place.Left
                                    : Place.Right ? Extent.Width - *Place.Right - ChildSize.Width
                                                  : Aligned.X;
            const float Y         = Place.Top      ? *Place.Top
                                    : Place.Bottom ? Extent.Height - *Place.Bottom - ChildSize.Height
                                                   : Aligned.Y;
            Child.SetOffset({X, Y});
        }
    }

    void RenderRepaintBoundary::SetCached(bool Cached) {
        GetLayer()->SetCached(Cached);
    }

    void RenderOpacity::SetOpacity(float Opacity) {
        GetLayer()->SetOpacity(std::clamp(Opacity, 0.0f, 1.0f));
    }
}  // namespace XenUI
