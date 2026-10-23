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

#include "XenUI/Widgets/Tabs.hpp"
#include "XenUI/Widgets/Basic.hpp"
#include "XenUI/Widgets/Input.hpp"
#include "XenUI/Widgets/Semantics.hpp"
#include "XenUI/Widgets/Theme.hpp"

#include <algorithm>

namespace XenUI {
    namespace {
        constexpr float kTabHeight     = 32.0f;
        constexpr float kIndicatorSize = 2.0f;

        /// @brief One tab header. Stateful only to track hover.
        struct TabHeader {
            std::string Label;
            bool Selected       = false;
            FocusNode* Keyboard = nullptr;
            std::function<void()> OnChoose;
            std::function<void(int)> OnStep;

            class State : public ComponentState<TabHeader> {
            protected:
                Node Build(BuildContext& Context) override {
                    const Theme& Style     = Theme::Of(Context);
                    const TabHeader& Props = GetWidget();
                    const bool Hovered     = _Hovered();
                    const bool Focused     = Props.Keyboard->HasFocus();
                    const Color Ink        = Props.Selected ? Style.Text : (Hovered ? Style.Text : Style.TextMuted);
                    const Color Indicator =
                      Props.Selected ? Style.Accent : (Focused ? Style.Border : Style.Accent.WithAlpha(0.0f));

                    return Semantics {
                      .Role     = SemanticsRole::TabItem,
                      .Label    = Props.Label,
                      .Selected = Props.Selected,
                      .OnSelect = Props.OnChoose,
                      .Child =
                        Focus {
                          .Target = Props.Keyboard,
                          .OnKey =
                            [this](const KeyEvent& Event) {
                                if (!Event.Pressed || Event.Modifiers != KeyModifiers::None) { return false; }
                                const TabHeader& Current = GetWidget();
                                switch (Event.Code) {
                                    case KeyCode::Left:
                                        Current.OnStep(-1);
                                        return true;
                                    case KeyCode::Right:
                                        Current.OnStep(1);
                                        return true;
                                    case KeyCode::Enter:
                                    case KeyCode::Space:
                                        if (!Event.Repeat) { Current.OnChoose(); }
                                        return true;
                                    default:
                                        return false;
                                }
                            },
                          .Child = MouseRegion {
                            .OnEnter = [this](const PointerEvent&) { _Hovered = true; },
                            .OnExit  = [this](const PointerEvent&) { _Hovered = false; },
                            .Cursor  = CursorShape::Hand,
                            .Child =
                              GestureDetector {
                                .OnTap    = [this](const TapEvent&) { GetWidget().OnChoose(); },
                                .Behavior = HitTestBehavior::Opaque,
                                .Child =
                                  Column {
                                    .MainSize = MainAxisSize::Min,
                                    .Children =
                                      {
                                        Container {
                                          .Height  = kTabHeight - kIndicatorSize,
                                          .Padding = EdgeInsets::Symmetric(14.0f, 0.0f),
                                          .Child =
                                            Align {
                                              .Child       = Text {.Content  = Props.Label,
                                                                   .Font     = Style.BodyFont,
                                                                   .Color    = Ink,
                                                                   .Wrapping = TextWrapping::NoWrap},
                                              .WidthFactor = 1.0f,
                                            },
                                        },
                                        Container {.Height       = kIndicatorSize,
                                                   .Background   = Indicator,
                                                   .CornerRadius = 1.0f},
                                      },
                                  },
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

    void TabView::State::Init() {
        _Selected = GetWidget().InitialIndex;
    }

    Signal<int>& TabView::State::GetSelection() {
        Signal<int>* External = GetWidget().Selected;
        return External != nullptr ? *External : _Selected;
    }

    void TabView::State::Choose(int Index, bool MoveFocus) {
        const TabView& Props = GetWidget();
        const int Count      = static_cast<int>(Props.Tabs.size());
        if (Count == 0) { return; }
        Index = std::clamp(Index, 0, Count - 1);
        if (MoveFocus && static_cast<size_t>(Index) < _Focus.size()) {
            _Focus[static_cast<size_t>(Index)]->RequestFocus();
        }
        if (Index == GetSelection().Peek()) { return; }
        GetSelection() = Index;
        if (Props.OnChanged) { Props.OnChanged(Index); }
    }

    Node TabView::State::Build(BuildContext& Context) {
        const Theme& Style   = Theme::Of(Context);
        const TabView& Props = GetWidget();
        const int Count      = static_cast<int>(Props.Tabs.size());
        const int Current    = Count > 0 ? std::clamp(GetSelection().Get(), 0, Count - 1) : -1;
        while (_Focus.size() < Props.Tabs.size()) {
            _Focus.push_back(std::make_unique<FocusNode>());
        }

        NodeList Headers;
        for (int i = 0; i < Count; ++i) {
            Headers.push_back(TabHeader {
              .Label    = Props.Tabs[static_cast<size_t>(i)].Label,
              .Selected = i == Current,
              .Keyboard = _Focus[static_cast<size_t>(i)].get(),
              .OnChoose = [this, i] { Choose(i, false); },
              .OnStep =
                [this, i](int Delta) {
                    Choose((i + Delta + static_cast<int>(GetWidget().Tabs.size())) %
                             static_cast<int>(GetWidget().Tabs.size()),
                           true);
                },
            });
        }

        const auto Cycle = [this](int Delta) {
            const int Total = static_cast<int>(GetWidget().Tabs.size());
            if (Total > 0) { Choose((GetSelection().Peek() + Delta + Total) % Total, false); }
        };

        return Shortcuts {
          .Bindings =
            {
              {{KeyModifiers::Control + KeyCode::Tab}, [Cycle] { Cycle(1); }},
              {{KeyModifiers::Control | (KeyModifiers::Shift + KeyCode::Tab)}, [Cycle] { Cycle(-1); }},
            },
          .Child =
            Column {
              .CrossAlignment = CrossAxisAlignment::Stretch,
              .Children =
                {
                  Semantics {.Role  = SemanticsRole::TabList,
                             .Child = Row {.MainSize = MainAxisSize::Min, .Children = std::move(Headers)}},
                  Container {.Height = 1.0f, .Background = Style.Border},
                  Flexible {.Child = Current >= 0 && !Props.Tabs[static_cast<size_t>(Current)].Content.IsEmpty()
                                       ? Keyed(Current, Props.Tabs[static_cast<size_t>(Current)].Content)
                                       : Node {}},
                },
            },
        };
    }
}  // namespace XenUI
