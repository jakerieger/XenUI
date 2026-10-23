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

#include "XenUI/Widgets/NumberField.hpp"
#include "XenUI/Widgets/Basic.hpp"
#include "XenUI/Widgets/Input.hpp"
#include "XenUI/Widgets/Semantics.hpp"
#include "XenUI/Widgets/TextField.hpp"
#include "XenUI/Widgets/Theme.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <format>

namespace XenUI {
    namespace {
        constexpr float kStepperWidth = 18.0f;
        constexpr float kCoarseFactor = 10.0f;

        std::string_view Trim(std::string_view Text) {
            while (!Text.empty() && std::isspace(static_cast<unsigned char>(Text.front()))) {
                Text.remove_prefix(1);
            }
            while (!Text.empty() && std::isspace(static_cast<unsigned char>(Text.back()))) {
                Text.remove_suffix(1);
            }
            return Text;
        }

        /// @brief One half of the step buttons.
        Node StepButton(const Theme& Style, const char* Glyph, bool Enabled, std::function<void()> Press) {
            const Node Face = Container {
              .Width          = kStepperWidth,
              .Height         = Style.ControlHeight * 0.5f,
              .ChildAlignment = Alignment::Center(),
              .Child          = Text {.Content = Glyph,
                                      .Font    = Font {.Size = 9.0f},
                                      .Color   = Enabled ? Style.TextMuted : Style.TextDisabled},
            };
            if (!Enabled) { return ExcludeSemantics {.Child = Face}; }
            return ExcludeSemantics {.Child = MouseRegion {
                                       .Cursor = CursorShape::Hand,
                                       .Child  = GestureDetector {.OnTap    = [Press](const TapEvent&) { Press(); },
                                                                  .Behavior = HitTestBehavior::Opaque,
                                                                  .Child    = Face},
                                     }};
        }
    }  // namespace

    BoundValue NumberField::State::GetSource() const {
        const NumberField& Props = GetWidget();
        return {Props.Value, Props.Bind, Props.OnChanged};
    }

    float NumberField::State::Clamp(float Candidate) const {
        const NumberField& Props = GetWidget();
        return std::clamp(Candidate, std::min(Props.Min, Props.Max), std::max(Props.Min, Props.Max));
    }

    std::string NumberField::State::Format(float Number) const {
        const NumberField& Props = GetWidget();
        return std::format("{:.{}f}", Number, std::max(Props.Decimals, 0)) + Props.Suffix;
    }

    std::optional<float> NumberField::State::Parse(const std::string& Text) const {
        std::string_view Number     = Trim(Text);
        const std::string_view Unit = Trim(GetWidget().Suffix);
        if (!Unit.empty() && Number.ends_with(Unit)) { Number = Trim(Number.substr(0, Number.size() - Unit.size())); }
        if (!Number.empty() && Number.front() == '+') { Number.remove_prefix(1); }

        float Result            = 0.0f;
        const auto [End, Error] = std::from_chars(Number.data(), Number.data() + Number.size(), Result);
        if (Number.empty() || Error != std::errc() || End != Number.data() + Number.size() || !std::isfinite(Result)) {
            return std::nullopt;
        }
        return Result;
    }

    void NumberField::State::ShowValue(float Number) {
        const std::string Text = Format(Number);
        if (_Editor.PeekText() != Text) { _Editor.SetText(Text); }
    }

    void NumberField::State::Init() {
        _Sync.emplace([this] {
            const float Current = GetSource().Get();
            if (!_Focus.HasFocus()) { ShowValue(Current); }
        });
    }

    void NumberField::State::WidgetUpdated(const NumberField& Previous) {
        (void)Previous;
        if (!_Focus.HasFocus()) { ShowValue(GetSource().Peek()); }
    }

    void NumberField::State::Apply() {
        const BoundValue Source = GetSource();
        if (const std::optional<float> Typed = Parse(_Editor.PeekText())) {
            const float Result = Clamp(*Typed);
            if (Result != Source.Peek()) { Source.Commit(Result); }
            ShowValue(Result);
            return;
        }
        ShowValue(Source.Peek());
    }

    void NumberField::State::Step(int Direction, bool Coarse) {
        const NumberField& Props = GetWidget();
        if (!Props.Enabled) { return; }
        const BoundValue Source = GetSource();
        const bool Edited       = _Focus.HasFocus() && _Editor.PeekText() != Format(Source.Peek());
        const float Base        = Edited ? Parse(_Editor.PeekText()).value_or(Source.Peek()) : Source.Peek();
        const float Result = Clamp(Base + static_cast<float>(Direction) * Props.Step * (Coarse ? kCoarseFactor : 1.0f));
        if (Result != Source.Peek()) { Source.Commit(Result); }
        ShowValue(Result);
    }

    Node NumberField::State::Build(BuildContext& Context) {
        const Theme& Style       = Theme::Of(Context);
        const NumberField& Props = GetWidget();
        const BoundValue Source  = GetSource();
        const float Low          = std::min(Props.Min, Props.Max);
        const float High         = std::max(Props.Min, Props.Max);
        const float Current      = Source.Get();

        const Node Field = Listener {
          .OnWheel =
            [this](const PointerEvent& Event) {
                if (!_Focus.HasFocus() || Event.WheelDelta.Y == 0.0f) { return false; }
                Step(Event.WheelDelta.Y > 0.0f ? 1 : -1, HasFlags(Event.Modifiers, KeyModifiers::Shift));
                return true;
            },
          .Child = TextField {
            .Controller = &_Editor,
            .OnSubmit   = [this](const std::string&) { Apply(); },
            .OnFocusChanged =
              [this](bool Focused) {
                  if (!Focused) { Apply(); }
              },
            .OnKey =
              [this](const KeyEvent& Event) {
                  if (!Event.Pressed) { return false; }
                  const bool Coarse = HasFlags(Event.Modifiers, KeyModifiers::Shift);
                  switch (Event.Code) {
                      case KeyCode::Up:
                          Step(1, Coarse);
                          return true;
                      case KeyCode::Down:
                          Step(-1, Coarse);
                          return true;
                      case KeyCode::Escape:
                          ShowValue(GetSource().Peek());
                          return true;
                      default:
                          return false;
                  }
              },
            .FocusTarget      = &_Focus,
            .Enabled          = Props.Enabled,
            .SelectAllOnFocus = true,
          },
        };

        return Semantics {
          .Role      = SemanticsRole::Spinner,
          .Label     = Props.Label,
          .Value     = Format(Current),
          .Range     = SemanticsRange {Current, Low, High, Props.Step},
          .Enabled   = Props.Enabled,
          .ReadRange = [Source] { return Source.Peek(); },
          .OnSetRange =
            [this](float Number) {
                const float Result = Clamp(Number);
                GetSource().Commit(Result);
                ShowValue(Result);
            },
          .Child =
            SizedBox {
              .Width = Props.Width,
              .Child =
                Row {
                  .Spacing = 2.0f,
                  .Children =
                    {
                      Expanded {.Child = Field},
                      Column {
                        .MainSize = MainAxisSize::Min,
                        .Children =
                          {
                            StepButton(Style, "▲", Props.Enabled, [this] { Step(1, false); }),
                            StepButton(Style, "▼", Props.Enabled, [this] { Step(-1, false); }),
                          },
                      },
                    },
                },
            },
        };
    }
}  // namespace XenUI
