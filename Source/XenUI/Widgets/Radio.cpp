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

#include "XenUI/Widgets/Radio.hpp"
#include "XenUI/Widgets/Basic.hpp"
#include "XenUI/Widgets/Input.hpp"
#include "XenUI/Widgets/Semantics.hpp"
#include "XenUI/Widgets/Theme.hpp"

namespace XenUI {
    namespace {
        constexpr float kIndicatorSize   = 16.0f;
        constexpr double kSelectDuration = 0.14;
    }  // namespace

    Node RadioButton::State::Build(BuildContext& Context) {
        const Theme& Style       = Theme::Of(Context);
        const RadioButton& Props = GetWidget();
        FocusNode& Keyboard      = Props.FocusTarget != nullptr ? *Props.FocusTarget : _Focus;
        const bool Focused       = Keyboard.HasFocus();

        if (_Shown) {
            _Dot.AnimateTo(Props.Selected ? 1.0f : 0.0f, kSelectDuration, Easing::EaseOutBack);
        } else {
            _Dot.Set(Props.Selected ? 1.0f : 0.0f);
            _Shown = true;
        }
        const float Amount = _Dot.Get();
        const Color Ring =
          !Props.Enabled ? Style.TextDisabled : (Props.Selected || Focused ? Style.Accent : Style.Border);
        const Color Dot = Props.Enabled ? Style.Accent : Style.TextDisabled;

        const Node Indicator = CustomPaint {
          .OnPaint =
            [Amount, Ring, Dot, Track = Style.Track](Painter& Canvas, const Size& Extent) {
                const Vec2 Center {Extent.Width * 0.5f, Extent.Height * 0.5f};
                const float Radius = Extent.Width * 0.5f;
                Canvas.FillCircle(Center, Radius - 0.5f, Track);
                Canvas.StrokeCircle(Center, Radius - 1.0f, Ring, {.Width = 1.5f});
                if (Amount > 0.01f) { Canvas.FillCircle(Center, (Radius - 4.5f) * Amount, Dot); }
            },
          .Width  = kIndicatorSize,
          .Height = kIndicatorSize,
        };

        NodeList Children {Indicator};
        if (!Props.Label.empty()) {
            Children.push_back(Text {.Content  = Props.Label,
                                     .Font     = Style.BodyFont,
                                     .Color    = Props.Enabled ? Style.Text : Style.TextDisabled,
                                     .Wrapping = TextWrapping::NoWrap});
        }
        const Node Content = Row {.Spacing = 8.0f, .MainSize = MainAxisSize::Min, .Children = std::move(Children)};

        const auto Choose = [this] {
            const RadioButton& Current = GetWidget();
            if (Current.Enabled && !Current.Selected && Current.OnSelected) { Current.OnSelected(); }
        };
        return Semantics {
          .Role     = SemanticsRole::RadioButton,
          .Label    = Props.Label,
          .Selected = Props.Selected,
          .Enabled  = Props.Enabled,
          .OnSelect = Props.Enabled ? std::function<void()>(Choose) : nullptr,
          .Child =
            Focus {
              .Target          = &Keyboard,
              .CanRequestFocus = Props.Enabled,
              .OnKey =
                [Choose](const KeyEvent& Event) {
                    const bool Activates = Event.Code == KeyCode::Space || Event.Code == KeyCode::Enter;
                    if (!Event.Pressed || Event.Repeat || !Activates || Event.Modifiers != KeyModifiers::None) {
                        return false;
                    }
                    Choose();
                    return true;
                },
              .Child =
                MouseRegion {
                  .Cursor = Props.Enabled ? std::optional<CursorShape>(CursorShape::Hand) : std::nullopt,
                  .Child  = GestureDetector {.OnTap    = [Choose](const TapEvent&) { Choose(); },
                                             .Behavior = HitTestBehavior::Opaque,
                                             .Child    = Content},
                },
            },
        };
    }

    Node RadioGroup::State::Build(BuildContext& Context) {
        (void)Context;
        const RadioGroup& Props = GetWidget();
        while (_Focus.size() < Props.Options.size()) {
            _Focus.push_back(std::make_unique<FocusNode>());
        }

        NodeList Buttons;
        for (size_t i = 0; i < Props.Options.size(); ++i) {
            const int Index = static_cast<int>(i);
            Buttons.push_back(RadioButton {
              .Selected = Index == Props.SelectedIndex,
              .OnSelected =
                [this, Index] {
                    if (const std::function<void(int)> Changed = GetWidget().OnChanged) { Changed(Index); }
                },
              .Label       = Props.Options[i],
              .Enabled     = Props.Enabled,
              .FocusTarget = _Focus[i].get(),
            });
        }

        const bool Vertical = Props.Direction == Axis::Vertical;
        return Semantics {
          .Role  = SemanticsRole::Group,
          .Label = Props.Label,
          .Child =
            Focus {
              .CanRequestFocus = false,
              .OnKey =
                [this, Vertical](const KeyEvent& Event) {
                    if (!Event.Pressed || Event.Modifiers != KeyModifiers::None) { return false; }
                    const bool Next     = Event.Code == (Vertical ? KeyCode::Down : KeyCode::Right);
                    const bool Previous = Event.Code == (Vertical ? KeyCode::Up : KeyCode::Left);
                    if (!Next && !Previous) { return false; }
                    Move(Next ? 1 : -1);
                    return true;
                },
              .Child =
                Flex {
                  .Direction      = Props.Direction,
                  .Spacing        = Props.Spacing,
                  .CrossAlignment = CrossAxisAlignment::Start,
                  .MainSize       = MainAxisSize::Min,
                  .Children       = std::move(Buttons),
                },
            },
        };
    }

    void RadioGroup::State::Move(int Delta) {
        const RadioGroup& Props = GetWidget();
        const int Count         = static_cast<int>(Props.Options.size());
        if (Count == 0 || !Props.Enabled) { return; }

        int Focused = -1;
        for (int i = 0; i < Count; ++i) {
            if (_Focus[static_cast<size_t>(i)]->HasFocus()) { Focused = i; }
        }
        const int From  = Focused >= 0 ? Focused : std::max(Props.SelectedIndex, 0);
        const int Index = (From + Delta + Count) % Count;
        _Focus[static_cast<size_t>(Index)]->RequestFocus();
        if (Index != Props.SelectedIndex && Props.OnChanged) { Props.OnChanged(Index); }
    }
}  // namespace XenUI
