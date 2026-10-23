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

#include "XenUI/Widgets/TreeView.hpp"
#include "XenUI/Widgets/Basic.hpp"
#include "XenUI/Widgets/Input.hpp"
#include "XenUI/Widgets/Scroll.hpp"
#include "XenUI/Widgets/Semantics.hpp"
#include "XenUI/Widgets/Theme.hpp"

#include <algorithm>

namespace XenUI {
    namespace {
        constexpr float kArrowWidth = 16.0f;

        /// @brief One visible row. Reads the selection itself, so selecting rebuilds only the rows involved.
        struct TreeRowView {
            TreeView::State* Tree = nullptr;
            int Index             = 0;

            class State : public ComponentState<TreeRowView> {
            protected:
                Node Build(BuildContext& Context) override {
                    const Theme& Style     = Theme::Of(Context);
                    TreeView::State* Owner = GetWidget().Tree;
                    const auto& Rows       = Owner->GetRows();
                    const int RowIndex     = GetWidget().Index;
                    if (RowIndex < 0 || static_cast<size_t>(RowIndex) >= Rows.size()) { return SizedBox {}; }
                    const TreeView::State::Row& Entry = Rows[static_cast<size_t>(RowIndex)];
                    const TreeView& Props             = Owner->GetWidget();
                    const std::string Id              = Entry.Item.Id;
                    const bool Selected               = Owner->GetSelection().Get() == Id;
                    const bool Open    = Entry.Item.HasChildren && Owner->GetExpansion().Peek().contains(Id);
                    const bool Hovered = _Hovered();

                    const Node Arrow =
                      Entry.Item.HasChildren
                        ? Node(ExcludeSemantics {
                            .Child =
                              GestureDetector {
                                .OnTap    = [Owner, Id, Open](const TapEvent&) { Owner->SetExpanded(Id, !Open); },
                                .Behavior = HitTestBehavior::Opaque,
                                .Child    = SizedBox {.Width = kArrowWidth,
                                                      .Child = Center {.Child = Text {.Content = Open ? "▾" : "▸",
                                                                                      .Font    = Style.SmallFont,
                                                                                      .Color   = Style.TextMuted}}},
                              }})
                        : Node(SizedBox {.Width = kArrowWidth});
                    const Node Content = Props.RowContent ? Props.RowContent(Entry.Item)
                                                          : Node(Text {.Content  = Entry.Item.Label,
                                                                       .Font     = Style.BodyFont,
                                                                       .Color    = Style.Text,
                                                                       .Wrapping = TextWrapping::NoWrap,
                                                                       .Trimming = TextTrimming::Ellipsis});

                    return Semantics {
                      .Role     = SemanticsRole::TreeItem,
                      .Label    = Entry.Item.Label,
                      .Expanded = Entry.Item.HasChildren ? std::optional<bool>(Open) : std::nullopt,
                      .Selected = Selected,
                      .OnInvoke = [Owner, Id] { Owner->Activate(Id); },
                      .OnExpand = [Owner, Id](bool Expand) { Owner->SetExpanded(Id, Expand); },
                      .OnSelect = [Owner, Id] { Owner->Select(Id, true); },
                      .Child =
                        MouseRegion {
                          .OnEnter = [this](const PointerEvent&) { _Hovered = true; },
                          .OnExit  = [this](const PointerEvent&) { _Hovered = false; },
                          .Child =
                            GestureDetector {
                              .OnTap =
                                [Owner, Id](const TapEvent&) {
                                    Owner->Select(Id, false);
                                    Owner->FocusTree();
                                },
                              .OnDoubleTap = [Owner, Id](const TapEvent&) { Owner->Activate(Id); },
                              .Behavior    = HitTestBehavior::Opaque,
                              .Child =
                                Container {
                                  .Height  = Props.RowHeight,
                                  .Padding = EdgeInsets {.Left  = 4.0f + static_cast<float>(Entry.Depth) * Props.Indent,
                                                         .Right = 8.0f},
                                  .Background   = Selected ? Style.Accent.WithAlpha(0.22f)
                                                           : (Hovered ? Style.Hover : Style.Hover.WithAlpha(0.0f)),
                                  .CornerRadius = Style.CornerRadius,
                                  .Child = Row {.Spacing = 2.0f, .Children = {Arrow, Expanded {.Child = Content}}},
                                },
                            },
                        },
                    };
                }

            private:
                Signal<bool> _Hovered = false;
            };
        };
    }  // namespace

    Signal<std::string>& TreeView::State::GetSelection() {
        Signal<std::string>* External = GetWidget().Selected;
        return External != nullptr ? *External : _Selected;
    }

    Signal<std::set<std::string>>& TreeView::State::GetExpansion() {
        Signal<std::set<std::string>>* External = GetWidget().Expanded;
        return External != nullptr ? *External : _Expanded;
    }

    void TreeView::State::Flatten(const std::string& Parent, int Depth, const std::set<std::string>& Open) {
        if (!GetWidget().ChildrenOf) { return; }
        for (TreeItem& Item : GetWidget().ChildrenOf(Parent)) {
            const bool Recurse = Item.HasChildren && Open.contains(Item.Id);
            std::string Id     = Item.Id;
            _Rows.push_back({std::move(Item), Depth, Parent});
            if (Recurse) { Flatten(Id, Depth + 1, Open); }
        }
    }

    int TreeView::State::IndexOf(const std::string& Id) const {
        const auto Found = std::ranges::find_if(_Rows, [&Id](const Row& Entry) { return Entry.Item.Id == Id; });
        return Found != _Rows.end() ? static_cast<int>(Found - _Rows.begin()) : -1;
    }

    void TreeView::State::Select(const std::string& Id, bool Reveal) {
        if (GetSelection().Peek() != Id) {
            GetSelection() = Id;
            if (const auto& Notify = GetWidget().OnSelected) { Notify(Id); }
        }
        if (!Reveal) { return; }
        if (const int Index = IndexOf(Id); Index >= 0) {
            const float Extent         = GetWidget().RowHeight;
            ScrollController& Scroller = GetWidget().Controller != nullptr ? *GetWidget().Controller : _Scroll;
            Scroller.EnsureVisible(static_cast<float>(Index) * Extent, static_cast<float>(Index + 1) * Extent);
        }
    }

    void TreeView::State::SetExpanded(const std::string& Id, bool Open) {
        if (GetExpansion().Peek().contains(Id) == Open) { return; }
        GetExpansion().Modify([&Id, Open](std::set<std::string>& Ids) {
            if (Open) {
                Ids.insert(Id);
            } else {
                Ids.erase(Id);
            }
        });
    }

    void TreeView::State::Activate(const std::string& Id) {
        Select(Id, false);
        const int Index = IndexOf(Id);
        if (Index >= 0 && _Rows[static_cast<size_t>(Index)].Item.HasChildren) {
            SetExpanded(Id, !GetExpansion().Peek().contains(Id));
        }
        if (const auto& Activated = GetWidget().OnActivated) { Activated(Id); }
    }

    bool TreeView::State::HandleKey(const KeyEvent& Event) {
        if (!Event.Pressed || Event.Modifiers != KeyModifiers::None || _Rows.empty()) { return false; }
        const std::string& Current = GetSelection().Peek();
        const int Index            = IndexOf(Current);
        const auto Go              = [this](int Target) {
            Select(_Rows[static_cast<size_t>(std::clamp(Target, 0, static_cast<int>(_Rows.size()) - 1))].Item.Id, true);
        };

        switch (Event.Code) {
            case KeyCode::Down:
                Go(Index < 0 ? 0 : Index + 1);
                return true;
            case KeyCode::Up:
                Go(Index < 0 ? 0 : Index - 1);
                return true;
            case KeyCode::Home:
                Go(0);
                return true;
            case KeyCode::End:
                Go(static_cast<int>(_Rows.size()) - 1);
                return true;
            case KeyCode::Enter:
                if (Index >= 0 && !Event.Repeat) { Activate(Current); }
                return true;
            case KeyCode::Right: {
                if (Index < 0) { return false; }
                const Row& Entry = _Rows[static_cast<size_t>(Index)];
                if (!Entry.Item.HasChildren) { return true; }
                if (!GetExpansion().Peek().contains(Entry.Item.Id)) {
                    SetExpanded(Entry.Item.Id, true);
                } else if (static_cast<size_t>(Index) + 1 < _Rows.size() &&
                           _Rows[static_cast<size_t>(Index) + 1].Depth > Entry.Depth) {
                    Go(Index + 1);
                }
                return true;
            }
            case KeyCode::Left: {
                if (Index < 0) { return false; }
                const Row& Entry = _Rows[static_cast<size_t>(Index)];
                if (Entry.Item.HasChildren && GetExpansion().Peek().contains(Entry.Item.Id)) {
                    SetExpanded(Entry.Item.Id, false);
                } else if (!Entry.Parent.empty()) {
                    Select(Entry.Parent, true);
                }
                return true;
            }
            default:
                return false;
        }
    }

    Node TreeView::State::Build(BuildContext& Context) {
        (void)Context;
        const TreeView& Props = GetWidget();
        _Rows.clear();
        Flatten({}, 0, GetExpansion().Get());

        const std::string& Current = GetSelection().Get();
        if (Current != _Revealed) {
            _Revealed = Current;
            if (const int Index = IndexOf(Current); Index >= 0) {
                _Reveal = Timer::After(0.0, [this, Index] {
                    const TreeView& Latest     = GetWidget();
                    ScrollController& Scroller = Latest.Controller != nullptr ? *Latest.Controller : _Scroll;
                    Scroller.RequestVisible(static_cast<float>(Index) * Latest.RowHeight,
                                            static_cast<float>(Index + 1) * Latest.RowHeight);
                });
            }
        }

        return Focus {
          .Target = &_Focus,
          .OnKey  = [this](const KeyEvent& Event) { return HandleKey(Event); },
          .Child =
            Semantics {
              .Role  = SemanticsRole::Tree,
              .Label = Props.Label,
              .Child =
                ListView {
                  .ItemCount   = static_cast<int>(_Rows.size()),
                  .ItemExtent  = Props.RowHeight,
                  .ItemBuilder = [this](int Index) { return Node(TreeRowView {this, Index}); },
                  .Controller  = Props.Controller != nullptr ? Props.Controller : &_Scroll,
                },
            },
        };
    }
}  // namespace XenUI
