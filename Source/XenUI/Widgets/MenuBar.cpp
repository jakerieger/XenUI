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

#include "XenUI/Widgets/MenuBar.hpp"
#include "XenUI/Widgets/Basic.hpp"
#include "XenUI/Widgets/Input.hpp"
#include "XenUI/Widgets/Semantics.hpp"
#include "XenUI/Widgets/Theme.hpp"

namespace XenUI {
    namespace {
        /// @brief Seconds during which a click on the menu that just closed does not reopen it. Clicking an open
        /// menu's button closes the menu as a click outside it, and the same click then taps the button.
        constexpr double kReopenGuard = 0.3;

        struct MenuBarButton {
            std::string Label;
            int Index           = 0;
            bool Open           = false;
            float Height        = 28.0f;
            FocusNode* Keyboard = nullptr;
            MenuBar::State* Bar = nullptr;
            std::function<void()> OnActivate;
            std::function<void()> OnTap;
            std::function<void()> OnHover;
            std::function<void(int)> OnStep;

            class State : public ComponentState<MenuBarButton> {
            protected:
                Node Build(BuildContext& Context) override {
                    const Theme& Style         = Theme::Of(Context);
                    const MenuBarButton& Props = GetWidget();
                    if (Props.Bar != nullptr && static_cast<size_t>(Props.Index) < Props.Bar->Anchors.size()) {
                        Props.Bar->Anchors[static_cast<size_t>(Props.Index)] = [this] {
                            return GetGlobalBounds(GetElement());
                        };
                    }
                    const bool Highlighted = Props.Open || _Hovered() || Props.Keyboard->HasFocus();

                    return Semantics {
                      .Role     = SemanticsRole::MenuItem,
                      .Label    = Props.Label,
                      .Expanded = Props.Open,
                      .OnInvoke = Props.OnActivate,
                      .OnExpand =
                        [this](bool Expand) {
                            if (Expand != GetWidget().Open) { GetWidget().OnActivate(); }
                        },
                      .Child =
                        Focus {
                          .Target     = Props.Keyboard,
                          .FocusOnTap = false,
                          .OnKey =
                            [this](const KeyEvent& Event) {
                                if (!Event.Pressed || Event.Modifiers != KeyModifiers::None) { return false; }
                                const MenuBarButton& Current = GetWidget();
                                switch (Event.Code) {
                                    case KeyCode::Enter:
                                    case KeyCode::Space:
                                    case KeyCode::Down:
                                        if (!Event.Repeat && !Current.Open) { Current.OnActivate(); }
                                        return true;
                                    case KeyCode::Left:
                                        Current.OnStep(-1);
                                        return true;
                                    case KeyCode::Right:
                                        Current.OnStep(1);
                                        return true;
                                    default:
                                        return false;
                                }
                            },
                          .Child = MouseRegion {
                            .OnEnter =
                              [this](const PointerEvent&) {
                                  _Hovered = true;
                                  GetWidget().OnHover();
                              },
                            .OnExit = [this](const PointerEvent&) { _Hovered = false; },
                            .Child =
                              GestureDetector {
                                .OnTap    = [this](const TapEvent&) { GetWidget().OnTap(); },
                                .Behavior = HitTestBehavior::Opaque,
                                .Child =
                                  Container {
                                    .Height       = Props.Height,
                                    .Padding      = EdgeInsets::Symmetric(10.0f, 0.0f),
                                    .Background   = Highlighted ? Style.Hover : Style.Hover.WithAlpha(0.0f),
                                    .CornerRadius = Style.CornerRadius,
                                    .Child =
                                      Align {
                                        .Child       = Text {.Content  = Props.Label,
                                                             .Font     = Style.BodyFont,
                                                             .Color    = Style.Text,
                                                             .Wrapping = TextWrapping::NoWrap},
                                        .WidthFactor = 1.0f,
                                      },
                                  },
                              },
                          },
                        },
                    };
                }

                void Dispose() override {
                    const MenuBarButton& Props = GetWidget();
                    if (Props.Bar != nullptr && static_cast<size_t>(Props.Index) < Props.Bar->Anchors.size()) {
                        Props.Bar->Anchors[static_cast<size_t>(Props.Index)] = {};
                    }
                }

            private:
                Signal<bool> _Hovered = false;
            };
        };
    }  // namespace

    Node MenuBar::State::Build(BuildContext& Context) {
        _Overlay             = OverlayController::Of(Context);
        _Style               = PopupStyle::Capture(Context);
        const MenuBar& Props = GetWidget();
        const int OpenIndex  = _Open();
        Anchors.resize(Props.Menus.size());
        while (_Focus.size() < Props.Menus.size()) {
            _Focus.push_back(std::make_unique<FocusNode>());
        }

        NodeList Buttons;
        for (size_t i = 0; i < Props.Menus.size(); ++i) {
            const int Index = static_cast<int>(i);
            Buttons.push_back(MenuBarButton {
              .Label      = Props.Menus[i].Label,
              .Index      = Index,
              .Open       = Index == OpenIndex,
              .Height     = Props.Height,
              .Keyboard   = _Focus[i].get(),
              .Bar        = this,
              .OnActivate = [this, Index] { Activate(Index); },
              .OnTap =
                [this, Index] {
                    if (_JustClosed == Index) {
                        _JustClosed = -1;
                        return;
                    }
                    Activate(Index);
                },
              .OnHover = [this, Index] { Hover(Index); },
              .OnStep  = [this, Index](int Direction) { Step(Index, Direction); },
            });
        }
        return Semantics {.Role = SemanticsRole::MenuBar,
                          .Child =
                            Row {.Spacing = 2.0f, .MainSize = MainAxisSize::Min, .Children = std::move(Buttons)}};
    }

    void MenuBar::State::Dispose() {
        _CloseGuard.Cancel();
        _Entry.Reset();
    }

    void MenuBar::State::Open(int Index) {
        const MenuBar& Props = GetWidget();
        if (_Overlay == nullptr || Index < 0 || static_cast<size_t>(Index) >= Props.Menus.size()) { return; }

        _Entry.Reset();
        const std::function<Rect()>& Anchor = Anchors[static_cast<size_t>(Index)];
        _Open                               = Index;
        _Entry                              = ShowMenu(
          *_Overlay,
          Anchor ? Anchor() : Rect {},
          PopupPlacement::Below,
          Props.Menus[static_cast<size_t>(Index)].Items,
          [this, Index] {
              if (_Open.Peek() != Index) { return; }
              _JustClosed = Index;
              _CloseGuard = Timer::After(kReopenGuard, [this] { _JustClosed = -1; });
              Close();
          },
          _Style,
          Props.MenuWidth,
          [this, Index](int Direction) { Step(Index, Direction); });
    }

    void MenuBar::State::Close() {
        _Entry.Reset();
        _Open = -1;
    }

    void MenuBar::State::Activate(int Index) {
        if (_Open.Peek() == Index) {
            Close();
            return;
        }
        Open(Index);
    }

    void MenuBar::State::Hover(int Index) {
        if (_Open.Peek() >= 0 && _Open.Peek() != Index) { Open(Index); }
    }

    void MenuBar::State::Step(int Index, int Direction) {
        const int Count = static_cast<int>(GetWidget().Menus.size());
        if (Count == 0) { return; }
        const int Next = (Index + Direction + Count) % Count;
        if (_Open.Peek() >= 0) {
            Open(Next);
        } else {
            _Focus[static_cast<size_t>(Next)]->RequestFocus();
        }
    }
}  // namespace XenUI
