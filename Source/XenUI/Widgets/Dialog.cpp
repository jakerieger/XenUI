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

#include "XenUI/Widgets/Dialog.hpp"
#include "XenUI/Widgets/Basic.hpp"
#include "XenUI/Widgets/Input.hpp"
#include "XenUI/Widgets/Semantics.hpp"
#include "XenUI/Widgets/Theme.hpp"

namespace XenUI {
    namespace {
        constexpr float kBarrierAlpha = 0.45f;
    }  // namespace

    Node Dialog::Build(BuildContext& Context) const {
        const Theme& Style = Theme::Of(Context);

        NodeList Buttons;
        std::function<void()> DefaultAction;
        for (const DialogAction& Action : Actions) {
            Buttons.push_back(Button {.Label = Action.Label, .OnPressed = Action.OnPressed, .Variant = Action.Variant});
            if (!DefaultAction && Action.Variant == ButtonVariant::Primary) { DefaultAction = Action.OnPressed; }
        }

        NodeList Children {
          Text {.Content = Title,
                .Font    = Font {.Family = Style.BodyFont.Family,
                                 .Size   = Style.BodyFont.Size + 3.0f,
                                 .Weight = FontWeight::SemiBold}},
        };
        if (!Content.IsEmpty()) { Children.push_back(Content); }
        if (!Buttons.empty()) {
            Children.push_back(
              Row {.Spacing = 8.0f, .MainAlignment = MainAxisAlignment::End, .Children = std::move(Buttons)});
        }

        return Focus {
          .AutoFocus = true,
          .OnKey =
            [Dismiss = OnDismiss, DefaultAction](const KeyEvent& Event) {
                if (!Event.Pressed || Event.Repeat || Event.Modifiers != KeyModifiers::None) { return false; }
                if (Event.Code == KeyCode::Escape && Dismiss) {
                    Dismiss();
                    return true;
                }
                if (Event.Code == KeyCode::Enter && DefaultAction) {
                    DefaultAction();
                    return true;
                }
                return false;
            },
          .Behavior   = HitTestBehavior::Opaque,
          .TrapsFocus = true,
          .Child      = Semantics {.Role  = SemanticsRole::Dialog,
                                   .Label = Title,
                                   .Child =
                                     Container {
                                       .Width        = Width,
                                       .Padding      = EdgeInsets::All(20.0f),
                                       .Background   = Style.SurfaceRaised,
                                       .CornerRadius = Style.CornerRadius * 2.0f,
                                       .BorderColor  = Style.Border,
                                       .Child =
                                         Column {
                                           .Spacing        = 16.0f,
                                           .CrossAlignment = CrossAxisAlignment::Stretch,
                                           .MainSize       = MainAxisSize::Min,
                                           .Children       = std::move(Children),
                                         },
                                     }},
        };
    }

    OverlayEntry
    ShowDialog(OverlayController& Overlay, const PopupStyle& Style, Dialog Description, bool DismissOnBarrier) {
        return Overlay.Show([Style, Description = std::move(Description), DismissOnBarrier] {
            return Style.Wrap(Stack {
              .Fit = StackFit::Expand,
              .Children =
                {
                  ModalBarrier {
                    .Tint      = Color::FromHex(0x000000, kBarrierAlpha),
                    .OnDismiss = DismissOnBarrier ? Description.OnDismiss : std::function<void()> {},
                  },
                  Center {.Child = Description},
                },
            });
        });
    }
}  // namespace XenUI
