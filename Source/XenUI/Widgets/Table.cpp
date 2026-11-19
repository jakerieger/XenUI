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

#include "XenUI/Widgets/Table.hpp"
#include "XenUI/Widgets/Basic.hpp"
#include "XenUI/Widgets/Input.hpp"
#include "XenUI/Widgets/Scroll.hpp"
#include "XenUI/Widgets/Semantics.hpp"
#include "XenUI/Widgets/Theme.hpp"

#include <algorithm>

namespace XenUI {
    namespace {
        constexpr float kCellPadding  = 8.0f;
        constexpr float kDividerWidth = 7.0f;

        Alignment CellAlignment(TextAlignment Column) {
            switch (Column) {
                case TextAlignment::Trailing:
                    return Alignment::CenterRight();
                case TextAlignment::Center:
                    return Alignment::Center();
                default:
                    return Alignment::CenterLeft();
            }
        }

        /// @brief Lays cells out at the table's column widths, the last one filling the rest.
        Node CellRow(Table::State& Owner, size_t Count, const std::function<Node(size_t Column)>& Cell) {
            NodeList Cells;
            for (size_t i = 0; i < Count; ++i) {
                if (i + 1 == Count) {
                    Cells.push_back(Expanded {.Child = Cell(i)});
                } else {
                    Cells.push_back(SizedBox {.Width = Owner.GetWidth(i), .Child = Cell(i)});
                }
            }
            return Row {.CrossAlignment = CrossAxisAlignment::Stretch, .Children = std::move(Cells)};
        }

        /// @brief One column header: the title, the sort arrow, and a divider to drag on its right edge.
        struct TableHeaderCell {
            Table::State* Parent = nullptr;
            size_t Column        = 0;
            bool Last            = false;
            /// @brief -1 when not sorted by this column, 1 ascending, 0 descending.
            int SortState = -1;

            class State : public ComponentState<TableHeaderCell> {
            protected:
                Node Build(BuildContext& Context) override {
                    const Theme& Style           = Theme::Of(Context);
                    const TableHeaderCell& Props = GetWidget();
                    Table::State* Owner          = Props.Parent;
                    const TableColumn& Spec      = Owner->GetWidget().Columns[Props.Column];
                    const int Index              = static_cast<int>(Props.Column);

                    NodeList Children {
                      Expanded {.Child = Text {.Content   = Spec.Title,
                                               .Font      = XenUI::Font {.Family = Style.SmallFont.Family,
                                                                         .Size   = Style.SmallFont.Size,
                                                                         .Weight = FontWeight::SemiBold},
                                               .Color     = Style.TextMuted,
                                               .Alignment = Spec.Alignment,
                                               .Wrapping  = TextWrapping::NoWrap,
                                               .Trimming  = TextTrimming::Ellipsis}},
                    };
                    if (Props.SortState >= 0) {
                        Children.push_back(ExcludeSemantics {.Child = Text {.Content = Props.SortState == 1 ? "▲" : "▼",
                                                                            .Font    = Font {.Size = 8.0f},
                                                                            .Color   = Style.TextMuted}});
                    }

                    const Node Title = Container {
                      .Padding    = EdgeInsets::Symmetric(kCellPadding, 0.0f),
                      .Background = _Hovered() && Spec.Sortable ? Style.Hover : Style.Hover.WithAlpha(0.0f),
                      .Child      = Row {.Spacing = 4.0f, .Children = std::move(Children)},
                    };
                    NodeList Layers {
                      Semantics {
                        .Role  = SemanticsRole::ColumnHeader,
                        .Label = Spec.Title,
                        .OnInvoke =
                          Spec.Sortable ? std::function<void()>([Owner, Index] { Owner->ToggleSort(Index); }) : nullptr,
                        .Child = MouseRegion {
                          .OnEnter = [this](const PointerEvent&) { _Hovered = true; },
                          .OnExit  = [this](const PointerEvent&) { _Hovered = false; },
                          .Cursor  = Spec.Sortable ? std::optional<CursorShape>(CursorShape::Hand) : std::nullopt,
                          .Child =
                            GestureDetector {
                              .OnTap =
                                [Owner, Index, Sortable = Spec.Sortable](const TapEvent&) {
                                    if (Sortable) { Owner->ToggleSort(Index); }
                                },
                              .Behavior = HitTestBehavior::Opaque,
                              .Child    = Title,
                            },
                        },
                      },
                    };
                    if (!Props.Last) {
                        const size_t ColumnIndex = Props.Column;
                        Layers.push_back(Positioned {
                          .Top    = 0.0f,
                          .Right  = 0.0f,
                          .Bottom = 0.0f,
                          .Width  = kDividerWidth,
                          .Child =
                            ExcludeSemantics {.Child =
                                                MouseRegion {
                                                  .Cursor = CursorShape::ResizeHorizontal,
                                                  .Child =
                                                    GestureDetector {
                                                      .OnDragUpdate =
                                                        [Owner, ColumnIndex](const DragEvent& Drag) {
                                                            Owner->Resize(ColumnIndex, Drag.Delta.X);
                                                        },
                                                      .Behavior = HitTestBehavior::Opaque,
                                                      .Child    = Center {.Child = Container {.Width      = 1.0f,
                                                                                              .Height     = 14.0f,
                                                                                              .Background = Style.Border}},
                                                    },
                                                }},
                        });
                    }
                    return Stack {.Fit = StackFit::Expand, .Children = std::move(Layers)};
                }

            private:
                Signal<bool> _Hovered = false;
            };
        };

        /// @brief One visible row. Reads the selection and column widths itself.
        struct TableRowView {
            Table::State* Parent = nullptr;
            int Index            = 0;

            class State : public ComponentState<TableRowView> {
            protected:
                Node Build(BuildContext& Context) override {
                    const Theme& Style  = Theme::Of(Context);
                    Table::State* Owner = GetWidget().Parent;
                    const Table& Props  = Owner->GetWidget();
                    const int RowIndex  = GetWidget().Index;
                    const bool Selected = Owner->IsRowSelected(RowIndex);
                    const bool Hovered  = _Hovered();
                    (void)Owner->ReadWidths();

                    const Color Background = Selected            ? Style.Accent.WithAlpha(0.22f)
                                             : Hovered           ? Style.Hover
                                             : RowIndex % 2 == 1 ? Style.Text.WithAlpha(0.03f)
                                                                 : Style.Text.WithAlpha(0.0f);
                    const Node Cells       = CellRow(*Owner, Props.Columns.size(), [&](size_t Column) -> Node {
                        const Node Content =
                          Props.CellBuilder
                                  ? Props.CellBuilder(RowIndex, static_cast<int>(Column))
                                  : Node(Text {.Content = Props.CellText ? Props.CellText(RowIndex, static_cast<int>(Column))
                                                                         : std::string(),
                                               .Font    = Style.BodyFont,
                                               .Color   = Style.Text,
                                               .Alignment = Props.Columns[Column].Alignment,
                                               .Wrapping  = TextWrapping::NoWrap,
                                               .Trimming  = TextTrimming::Ellipsis});
                        return Padding {.Insets = EdgeInsets::Symmetric(kCellPadding, 0.0f),
                                              .Child  = Align {.Position = CellAlignment(Props.Columns[Column].Alignment),
                                                               .Child    = Content}};
                    });

                    return Semantics {
                      .Role     = SemanticsRole::TableRow,
                      .Selected = Selected,
                      .OnInvoke = [Owner, RowIndex] { Owner->Activate(RowIndex); },
                      .OnSelect = [Owner, RowIndex] { Owner->Select(RowIndex, true); },
                      .Child =
                        MouseRegion {
                          .OnEnter = [this](const PointerEvent&) { _Hovered = true; },
                          .OnExit  = [this](const PointerEvent&) { _Hovered = false; },
                          .Child =
                            GestureDetector {
                              .OnTap =
                                [Owner, RowIndex](const TapEvent& Tap) {
                                    Owner->Click(RowIndex, Tap.Modifiers);
                                    Owner->FocusTable();
                                },
                              .OnDoubleTap = [Owner, RowIndex](const TapEvent&) { Owner->Activate(RowIndex); },
                              .OnSecondaryTap =
                                [this, Owner, RowIndex](const TapEvent& Tap) {
                                    Owner->FocusTable();
                                    Owner->OpenRowMenu(RowIndex,
                                                       GetGlobalBounds(GetElement()).Origin() + Tap.LocalPosition);
                                },
                              .Behavior    = HitTestBehavior::Opaque,
                              .Child = Container {.Height = Props.RowHeight, .Background = Background, .Child = Cells},
                            },
                        },
                    };
                }

            private:
                Signal<bool> _Hovered = false;
            };
        };
    }  // namespace

    void Table::State::Init() {
        ResetWidths();
    }

    void Table::State::WidgetUpdated(const Table& Previous) {
        if (Previous.Columns.size() != GetWidget().Columns.size()) { ResetWidths(); }
    }

    void Table::State::ResetWidths() {
        std::vector<float> Widths;
        for (const TableColumn& Column : GetWidget().Columns) {
            Widths.push_back(std::max(Column.Width, Column.MinWidth));
        }
        _Widths = std::move(Widths);
    }

    Signal<int>& Table::State::GetSelection() {
        Signal<int>* External = GetWidget().Selected;
        return External != nullptr ? *External : _Selected;
    }

    Signal<TableSort>& Table::State::GetSort() {
        Signal<TableSort>* External = GetWidget().Sort;
        return External != nullptr ? *External : _Sort;
    }

    float Table::State::GetWidth(size_t Column) const {
        const std::vector<float>& Widths = _Widths.Get();
        return Column < Widths.size() ? Widths[Column] : 0.0f;
    }

    void Table::State::Resize(size_t Column, float Delta) {
        if (Column >= _Widths.Peek().size()) { return; }
        const float Minimum = GetWidget().Columns[Column].MinWidth;
        _Widths.Modify([Column, Delta, Minimum](std::vector<float>& Widths) {
            Widths[Column] = std::max(Minimum, Widths[Column] + Delta);
        });
    }

    void Table::State::ToggleSort(int Column) {
        const TableSort Current = GetSort().Peek();
        const TableSort Next =
          Current.Column == Column ? TableSort {Column, !Current.Ascending} : TableSort {Column, true};
        GetSort() = Next;
        if (GetWidget().OnSort) { GetWidget().OnSort(Next); }
    }

    Signal<std::vector<int>>& Table::State::GetRows() {
        Signal<std::vector<int>>* External = GetWidget().SelectedRows;
        return External != nullptr ? *External : _Rows;
    }

    bool Table::State::IsRowSelected(int Row) {
        if (!IsMultiple()) { return GetSelection().Get() == Row; }
        const std::vector<int>& Rows = GetRows().Get();
        return std::binary_search(Rows.begin(), Rows.end(), Row);
    }

    void Table::State::SetRows(std::vector<int> Rows) {
        if (!IsMultiple()) { return; }
        std::sort(Rows.begin(), Rows.end());
        Rows.erase(std::unique(Rows.begin(), Rows.end()), Rows.end());
        if (GetRows().Peek() == Rows) { return; }
        GetRows() = Rows;
        if (GetWidget().OnSelectionChanged) { GetWidget().OnSelectionChanged(Rows); }
    }

    void Table::State::SelectRange(int From, int To, bool Keep) {
        std::vector<int> Rows = Keep ? GetRows().Peek() : std::vector<int> {};
        for (int Row = std::min(From, To); Row <= std::max(From, To); ++Row) {
            Rows.push_back(Row);
        }
        SetRows(std::move(Rows));
    }

    void Table::State::Select(int Row, bool Reveal) {
        const int Count = GetWidget().RowCount;
        if (Count <= 0) { return; }
        Row     = std::clamp(Row, 0, Count - 1);
        _Anchor = Row;
        SetRows({Row});
        SetLead(Row, Reveal);
    }

    void Table::State::Click(int Row, KeyModifiers Modifiers) {
        if (!IsMultiple() || GetWidget().RowCount <= 0) {
            Select(Row, false);
            return;
        }
        const bool Control = HasFlags(Modifiers, KeyModifiers::Control);
        if (HasFlags(Modifiers, KeyModifiers::Shift) && _Anchor >= 0) {
            SelectRange(_Anchor, Row, Control);
            SetLead(Row, false);
        } else if (Control) {
            std::vector<int> Rows = GetRows().Peek();
            const auto Found      = std::find(Rows.begin(), Rows.end(), Row);
            if (Found != Rows.end()) {
                Rows.erase(Found);
            } else {
                Rows.push_back(Row);
            }
            _Anchor = Row;
            SetRows(std::move(Rows));
            SetLead(Row, false);
        } else {
            Select(Row, false);
        }
    }

    void Table::State::OpenRowMenu(int Row, const Vec2& GlobalPosition) {
        if (!IsRowSelected(Row)) { Select(Row, false); }
        if (!GetWidget().RowMenu || _Overlay == nullptr) { return; }
        std::vector<MenuItem> Items = GetWidget().RowMenu(Row);
        if (Items.empty()) { return; }
        _Menu = ShowMenu(*_Overlay,
                         {GlobalPosition.X, GlobalPosition.Y, 0.0f, 0.0f},
                         PopupPlacement::Below,
                         std::move(Items),
                         [this] { _Menu.Reset(); },
                         _Style);
    }

    void Table::State::SetLead(int Row, bool Reveal) {
        const int Count = GetWidget().RowCount;
        if (Count <= 0) { return; }
        Row = std::clamp(Row, 0, Count - 1);
        if (GetSelection().Peek() != Row) {
            GetSelection() = Row;
            if (GetWidget().OnSelected) { GetWidget().OnSelected(Row); }
        }
        if (Reveal) {
            const float Extent         = GetWidget().RowHeight;
            ScrollController& Scroller = GetWidget().Controller != nullptr ? *GetWidget().Controller : _Scroll;
            Scroller.EnsureVisible(static_cast<float>(Row) * Extent, static_cast<float>(Row + 1) * Extent);
        }
    }

    void Table::State::Activate(int Row) {
        Select(Row, false);
        if (GetWidget().OnActivated) { GetWidget().OnActivated(Row); }
    }

    bool Table::State::HandleKey(const KeyEvent& Event) {
        const Table& Props = GetWidget();
        if (!Event.Pressed || Props.RowCount <= 0) { return false; }
        const int Current = GetSelection().Peek();
        if (IsMultiple() && Event.Modifiers == KeyModifiers::Control) {
            if (Event.Code == KeyCode::A) {
                SelectRange(0, Props.RowCount - 1, false);
                return true;
            }
            if (Event.Code == KeyCode::Space && Current >= 0) {
                Click(Current, KeyModifiers::Control);
                return true;
            }
            return false;
        }
        const bool Extend = IsMultiple() && Event.Modifiers == KeyModifiers::Shift;
        if (Event.Modifiers != KeyModifiers::None && !Extend) { return false; }
        const ScrollController& Scroller = Props.Controller != nullptr ? *Props.Controller : _Scroll;
        const int Page = std::max(1, static_cast<int>(Scroller.GetViewportExtent() / Props.RowHeight) - 1);
        const auto MoveTo = [this, Extend, Count = Props.RowCount](int Row) {
            Row = std::clamp(Row, 0, Count - 1);
            if (Extend) {
                if (_Anchor < 0) { _Anchor = std::max(GetSelection().Peek(), 0); }
                SelectRange(_Anchor, Row, false);
                SetLead(Row, true);
            } else {
                Select(Row, true);
            }
            return true;
        };
        switch (Event.Code) {
            case KeyCode::Down:
                return MoveTo(Current < 0 ? 0 : Current + 1);
            case KeyCode::Up:
                return MoveTo(Current < 0 ? 0 : Current - 1);
            case KeyCode::PageDown:
                return MoveTo(std::max(Current, 0) + Page);
            case KeyCode::PageUp:
                return MoveTo(std::max(Current, 0) - Page);
            case KeyCode::Home:
                return MoveTo(0);
            case KeyCode::End:
                return MoveTo(Props.RowCount - 1);
            case KeyCode::Enter:
                if (Extend) { return false; }
                if (Current >= 0 && !Event.Repeat) { Activate(Current); }
                return true;
            default:
                return false;
        }
    }

    Node Table::State::Build(BuildContext& Context) {
        _Overlay           = OverlayController::Of(Context);
        _Style             = PopupStyle::Capture(Context);
        const Theme& Style = Theme::Of(Context);
        const Table& Props = GetWidget();
        (void)_Widths.Get();
        const TableSort Sorting = GetSort().Get();

        const Node Header =
          CellRow(*this, Props.Columns.size(), [this, Sorting, Count = Props.Columns.size()](size_t Column) -> Node {
              const int SortState = Sorting.Column == static_cast<int>(Column) ? (Sorting.Ascending ? 1 : 0) : -1;
              return TableHeaderCell {this, Column, Column + 1 == Count, SortState};
          });

        return Focus {
          .Target = &_Focus,
          .OnKey  = [this](const KeyEvent& Event) { return HandleKey(Event); },
          .Child =
            Semantics {
              .Role  = SemanticsRole::Table,
              .Label = Props.Label,
              .Child =
                ClipRect {
                  .Child =
                    Column {
                      .CrossAlignment = CrossAxisAlignment::Stretch,
                      .Children =
                        {
                          Container {.Height = Props.HeaderHeight, .Background = Style.SurfaceRaised, .Child = Header},
                          Container {.Height = 1.0f, .Background = Style.Border},
                          Expanded {.Child =
                                      ListView {
                                        .ItemCount   = Props.RowCount,
                                        .ItemExtent  = Props.RowHeight,
                                        .ItemBuilder = [this](int Index) { return Node(TableRowView {this, Index}); },
                                        .Controller  = Props.Controller != nullptr ? Props.Controller : &_Scroll,
                                      }},
                        },
                    },
                },
            },
        };
    }
}  // namespace XenUI
