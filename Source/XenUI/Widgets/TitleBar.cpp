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

#include "XenUI/Widgets/TitleBar.hpp"
#include "XenUI/Widgets/Basic.hpp"
#include "XenUI/Widgets/Input.hpp"
#include "XenUI/Widgets/Theme.hpp"
#include "XenUI/Widgets/WindowChrome.hpp"

namespace XenUI {
    namespace {
        constexpr float kSpacing      = 10.0f;
        constexpr float kMenuInset    = 6.0f;
        constexpr float kTitleInset   = 12.0f;
        constexpr float kMenuBarInset = 8.0f;
    }  // namespace

    Node TitleBar::Build(BuildContext& Context) const {
        const Theme& Style         = Theme::Of(Context);
        const WindowChrome* Chrome = WindowChrome::Of(Context);
        // Like the system title bar, the title dims while the window is inactive.
        const bool Active  = Chrome == nullptr || Chrome->Active();
        const Node Caption = Text {.Content  = Title,
                                   .Font     = Style.BodyFont,
                                   .Color    = Active ? Style.TextMuted : Style.TextDisabled,
                                   .Wrapping = TextWrapping::NoWrap,
                                   .Trimming = TextTrimming::Ellipsis};

        NodeList Items;
        const bool Centered = !Menus.empty();
        if (Centered) {
            Items.push_back(
              Padding {.Insets = {.Left = kMenuInset},
                       .Child  = MenuBar {.Menus = Menus, .Height = Height - kMenuBarInset, .MenuWidth = MenuWidth}});
            Items.push_back(Spacer {});
        } else {
            Items.push_back(Expanded {.Child = Padding {.Insets = {.Left = kTitleInset}, .Child = Caption}});
        }
        Items.insert(Items.end(), Actions.begin(), Actions.end());
        Items.push_back(WindowControls {.Height = Height});

        Node Content = Row {.Spacing = kSpacing, .Children = std::move(Items)};
        if (Centered) {
            // The title is centered on the whole bar, not on the space between the menus and the actions, so it stays
            // put as they change. It ignores the pointer, so the bar under it still drags the window.
            Content = Stack {.Fit      = StackFit::Expand,
                             .Children = {IgnorePointer {.Child = Center {.Child = Caption}}, Content}};
        }

        return Container {
          .Height     = Height,
          .Background = Background.value_or(Style.Chrome),
          .Child      = WindowDragArea {.Child = Content},
        };
    }
}  // namespace XenUI
