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

#include "XenUI/Widgets/Hyperlink.hpp"
#include "XenUI/Platform/Shell.hpp"
#include "XenUI/Widgets/Basic.hpp"
#include "XenUI/Widgets/Input.hpp"
#include "XenUI/Widgets/Semantics.hpp"
#include "XenUI/Widgets/Theme.hpp"

namespace XenUI {
    void Hyperlink::State::Activate() const {
        const Hyperlink& Props = GetWidget();
        if (!Props.Enabled) { return; }
        if (Props.OnPressed) {
            Props.OnPressed();
        } else {
            Shell::Open(Props.Url);
        }
    }

    Node Hyperlink::State::Build(BuildContext& Context) {
        const Theme& Style     = Theme::Of(Context);
        const Hyperlink& Props = GetWidget();
        const std::string& Shown = Props.Label.empty() ? Props.Url : Props.Label;
        const bool Hovered       = Props.Enabled && _Hovered();
        const bool Focused       = _Focus.HasFocus();
        const Color Ink          = !Props.Enabled ? Style.TextDisabled
                                   : Hovered      ? Style.Accent.Lerp(Style.Text, 0.25f)
                                                  : Style.Accent;

        NodeList Layers {Text {.Content  = Shown,
                               .Font     = Props.Font.value_or(Style.BodyFont),
                               .Color    = Ink,
                               .Wrapping = TextWrapping::NoWrap,
                               .Trimming = TextTrimming::Ellipsis}};
        if (Hovered || Focused) {
            Layers.push_back(Positioned {.Left   = 0.0f,
                                         .Right  = 0.0f,
                                         .Bottom = 0.0f,
                                         .Height = 1.0f,
                                         .Child  = Container {.Background = Ink}});
        }
        const Node Face = Stack {.Children = std::move(Layers)};

        if (!Props.Enabled) {
            return Semantics {.Role = SemanticsRole::Link, .Label = Shown, .Enabled = false, .Child = Face};
        }
        return Semantics {
          .Role        = SemanticsRole::Link,
          .Label       = Shown,
          .Description = Props.OnPressed ? std::string() : Props.Url,
          .OnInvoke    = [this] { Activate(); },
          .Child =
            Focus {
              .Target = &_Focus,
              .OnKey =
                [this](const KeyEvent& Event) {
                    const bool Activation = Event.Pressed && !Event.Repeat &&
                                            (Event.Code == KeyCode::Enter || Event.Code == KeyCode::Space) &&
                                            Event.Modifiers == KeyModifiers::None;
                    if (!Activation) { return false; }
                    Activate();
                    return true;
                },
              .Child =
                MouseRegion {
                  .OnEnter = [this](const PointerEvent&) { _Hovered = true; },
                  .OnExit  = [this](const PointerEvent&) { _Hovered = false; },
                  .Cursor  = CursorShape::Hand,
                  .Child   = GestureDetector {.OnTap    = [this](const TapEvent&) { Activate(); },
                                              .Behavior = HitTestBehavior::Opaque,
                                              .Child    = Face},
                },
            },
        };
    }
}  // namespace XenUI
