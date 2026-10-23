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

#include "XenUI/Widgets/Controls.hpp"
#include "XenUI/Graphics/Path.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace XenUI {
    namespace {
        constexpr float kKnobSweep        = std::numbers::pi_v<float> * 0.75f;
        constexpr float kKnobDragRange    = 200.0f;
        constexpr float kFineFactor       = 0.1f;
        constexpr float kKeyboardFraction = 0.05f;
        constexpr float kMenuRowHeight    = 24.0f;
        constexpr double kHoverDuration   = 0.12;
        constexpr double kPressDuration   = 0.05;
        constexpr double kToggleDuration  = 0.16;

        bool IsActivationKey(const KeyEvent& Event) {
            return Event.Pressed && !Event.Repeat && (Event.Code == KeyCode::Enter || Event.Code == KeyCode::Space) &&
                   Event.Modifiers == KeyModifiers::None;
        }

        Vec2 PointOnCircle(const Vec2& Center, float Radius, float Angle) {
            return {Center.X + std::sin(Angle) * Radius, Center.Y - std::cos(Angle) * Radius};
        }

        Path BuildArc(const Vec2& Center, float Radius, float From, float To) {
            const float Start = std::min(From, To);
            const float End   = std::max(From, To);
            return PathBuilder()
              .MoveTo(PointOnCircle(Center, Radius, Start))
              .ArcTo(PointOnCircle(Center, Radius, End),
                     {Radius, Radius},
                     0.0f,
                     ArcSweep::Clockwise,
                     End - Start > std::numbers::pi_v<float> ? ArcSize::Large : ArcSize::Small)
              .Build();
        }

        Node ToggleRow(const Theme& Style,
                       const Node& Indicator,
                       const std::string& Label,
                       bool Enabled,
                       SemanticsRole Role,
                       bool Value,
                       std::function<void()> Toggle) {
            NodeList Children {Indicator};
            if (!Label.empty()) {
                Children.push_back(Text {.Content  = Label,
                                         .Font     = Style.BodyFont,
                                         .Color    = Enabled ? Style.Text : Style.TextDisabled,
                                         .Wrapping = TextWrapping::NoWrap});
            }
            const Node Content = Row {.Spacing = 8.0f, .MainSize = MainAxisSize::Min, .Children = std::move(Children)};
            if (!Enabled) { return Semantics {.Role = Role, .Toggled = Value, .Enabled = false, .Child = Content}; }

            return Semantics {.Role     = Role,
                              .Toggled  = Value,
                              .OnToggle = Toggle,
                              .Child    = Focus {
                                .OnKey =
                                  [Toggle](const KeyEvent& Event) {
                                      if (!IsActivationKey(Event)) { return false; }
                                      Toggle();
                                      return true;
                                  },
                                .Child =
                                  MouseRegion {
                                    .Cursor = CursorShape::Hand,
                                    .Child =
                                      GestureDetector {
                                        .OnTap    = [Toggle](const TapEvent&) { Toggle(); },
                                        .Behavior = HitTestBehavior::Opaque,
                                        .Child    = Content,
                                      },
                                  },
                              }};
        }
    }  // namespace

    PopupStyle PopupStyle::Capture(BuildContext& Context) {
        const TextStyle* Inherited = Context.Find<TextStyle>();
        return {Theme::Of(Context), Inherited != nullptr ? *Inherited : TextStyle {}};
    }

    Node PopupStyle::Wrap(const Node& Content) const {
        return Provide<Theme> {.Value = Palette, .Child = Provide<TextStyle> {.Value = Typography, .Child = Content}};
    }

    Node Button::State::Build(BuildContext& Context) {
        const Theme& Style  = Theme::Of(Context);
        const Button& Props = GetWidget();
        const bool Hovered  = _Hovered();
        const bool Pressed  = _Pressed();
        const bool Focused  = _Focus.HasFocus();

        Color Fill      = Style.SurfaceRaised;
        Color TextColor = Style.Text;
        std::optional<Color> Border;
        switch (Props.Variant) {
            case ButtonVariant::Primary:
                Fill      = Pressed ? Style.Accent.Lerp(Style.Pressed, 0.3f)
                                    : (Hovered ? Style.Accent.Lerp(Style.Text, 0.15f) : Style.Accent);
                TextColor = Style.AccentText;
                break;
            case ButtonVariant::Secondary:
                Fill   = Pressed ? Style.Pressed : (Hovered ? Style.Hover : Style.SurfaceRaised);
                Border = Style.Border;
                break;
            case ButtonVariant::Ghost:
                Fill = Pressed ? Style.Pressed : (Hovered ? Style.Hover : Style.Hover.WithAlpha(0.0f));
                break;
        }
        if (!Props.Enabled) {
            Fill      = Style.Surface;
            TextColor = Style.TextDisabled;
        }
        if (Focused && Props.Enabled) { Border = Style.Accent; }

        if (_Shown) {
            _Fill.AnimateTo(Fill, Pressed ? kPressDuration : kHoverDuration);
        } else {
            _Fill.Set(Fill);
            _Shown = true;
        }

        const Node Face = Container {
          .Width        = Props.Width,
          .Height       = Style.ControlHeight,
          .Padding      = EdgeInsets::Symmetric(12.0f, 0.0f),
          .Background   = _Fill.Get(),
          .CornerRadius = Style.CornerRadius,
          .BorderColor  = Border,
          .Child =
            Align {
              .Child       = Text {.Content  = Props.Label,
                                   .Font     = Style.BodyFont,
                                   .Color    = TextColor,
                                   .Wrapping = TextWrapping::NoWrap},
              .WidthFactor = Props.Width ? std::nullopt : std::optional<float>(1.0f),
            },
        };
        if (!Props.Enabled) {
            return Semantics {.Role    = SemanticsRole::Button,
                              .Label   = Props.AccessibleLabel.empty() ? Props.Label : Props.AccessibleLabel,
                              .Enabled = false,
                              .Child   = Face};
        }

        return Semantics {
          .Role     = SemanticsRole::Button,
          .Label    = Props.AccessibleLabel.empty() ? Props.Label : Props.AccessibleLabel,
          .OnInvoke = [this] { Activate(); },
          .Child    = Focus {
            .Target = &_Focus,
            .OnKey =
              [this](const KeyEvent& Event) {
                  if (!IsActivationKey(Event)) { return false; }
                  Activate();
                  return true;
              },
            .Child = MouseRegion {
              .OnEnter = [this](const PointerEvent&) { _Hovered = true; },
              .OnExit =
                [this](const PointerEvent&) {
                    _Hovered = false;
                    _Pressed = false;
                },
              .Cursor = CursorShape::Hand,
              .Child =
                Listener {
                  .OnDown =
                    [this](const PointerEvent& Event) {
                        if (Event.Button == MouseButton::Primary) { _Pressed = true; }
                    },
                  .OnUp     = [this](const PointerEvent&) { _Pressed = false; },
                  .OnCancel = [this](const PointerEvent&) { _Pressed = false; },
                  .Child    = GestureDetector {.OnTap = [this](const TapEvent&) { Activate(); }, .Child = Face},
                },
            },
          }};
    }

    void Button::State::Activate() const {
        const std::function<void()> Action = GetWidget().OnPressed;
        if (Action) { Action(); }
    }

    Node Checkbox::State::Build(BuildContext& Context) {
        const Theme& Style         = Theme::Of(Context);
        const Checkbox& Props      = GetWidget();
        const bool Checked         = Props.Value;
        const std::string& Caption = Props.Label;
        const bool Active          = Props.Enabled;
        const auto& Changed        = Props.OnChanged;
        if (_Shown) {
            _Checked.AnimateTo(Checked ? 1.0f : 0.0f, kToggleDuration);
        } else {
            _Checked.Set(Checked ? 1.0f : 0.0f);
            _Shown = true;
        }
        const float Amount = _Checked.Get();
        const Node Box     = Container {
          .Width        = 16.0f,
          .Height       = 16.0f,
          .Background   = Interpolate(Style.Track, Style.Accent, Amount),
          .CornerRadius = 3.0f,
          .BorderColor  = Interpolate(Style.Border, Style.Accent, Amount),
          .Child = Amount > 0.01f
                     ? Node(CustomPaint {.OnPaint =
                                           [Mark = Style.AccentText.WithAlpha(
                                              Style.AccentText.A * Amount)](Painter& Canvas, const Size& Extent) {
                                               const Path Tick =
                                                 PathBuilder()
                                                   .MoveTo({Extent.Width * 0.22f, Extent.Height * 0.52f})
                                                   .LineTo({Extent.Width * 0.42f, Extent.Height * 0.72f})
                                                   .LineTo({Extent.Width * 0.78f, Extent.Height * 0.30f})
                                                   .Build();
                                               Canvas.StrokePath(
                                                 Tick,
                                                 Mark,
                                                 {.Width = 2.0f, .Cap = LineCap::Round, .Join = LineJoin::Round});
                                           }})
                     : Node {},
        };
        return ToggleRow(Style, Box, Caption, Active, SemanticsRole::CheckBox, Checked, [Checked, Changed] {
            if (Changed) { Changed(!Checked); }
        });
    }

    Node Switch::State::Build(BuildContext& Context) {
        const Theme& Style         = Theme::Of(Context);
        const Switch& Props        = GetWidget();
        const bool On              = Props.Value;
        const std::string& Caption = Props.Label;
        const bool Active          = Props.Enabled;
        const auto& Changed        = Props.OnChanged;
        if (_Shown) {
            _Position.AnimateTo(On ? 1.0f : 0.0f, kToggleDuration, Easing::EaseOutBack);
        } else {
            _Position.Set(On ? 1.0f : 0.0f);
            _Shown = true;
        }
        const float Position = _Position.Get();
        const Node Track     = CustomPaint {
          .OnPaint =
            [Position, Style](Painter& Canvas, const Size& Extent) {
                const float Radius = Extent.Height * 0.5f;
                const float Blend  = std::clamp(Position, 0.0f, 1.0f);
                Canvas.FillRoundedRect({0.0f, 0.0f, Extent.Width, Extent.Height},
                                       Radius,
                                       Interpolate(Style.Track, Style.Accent, Blend));
                Canvas.StrokeRoundedRect({0.5f, 0.5f, Extent.Width - 1.0f, Extent.Height - 1.0f},
                                         Radius,
                                         Interpolate(Style.Border, Style.Accent, Blend));
                const float ThumbX = Interpolate(Radius, Extent.Width - Radius, Position);
                Canvas.FillCircle({ThumbX, Radius},
                                  Radius - 3.0f,
                                  Interpolate(Style.TextMuted, Style.AccentText, Blend));
            },
          .Width  = 32.0f,
          .Height = 18.0f,
        };
        return ToggleRow(Style, Track, Caption, Active, SemanticsRole::Switch, On, [On, Changed] {
            if (Changed) { Changed(!On); }
        });
    }

    Node Slider::Build(BuildContext& Context) const {
        const Theme& Style = Theme::Of(Context);
        const BoundValue Source {Value, Bind, OnChanged};

        const Node Raw = RawSlider {{
          .Source     = Source,
          .Min        = Min,
          .Max        = Max,
          .Direction  = Direction,
          .Enabled    = Enabled,
          .TrackColor = Style.Track,
          .FillColor  = Style.Accent,
          .ThumbColor = Enabled ? Style.Text : Style.TextDisabled,
          .Label      = Label,
        }};
        if (!Enabled) { return Raw; }

        return Focus {
          .OnKey =
            [Source, Low = Min, High = Max, Direction = Direction](const KeyEvent& Event) {
                if (!Event.Pressed) { return false; }
                const bool Increase =
                  Direction == Axis::Horizontal ? Event.Code == KeyCode::Right : Event.Code == KeyCode::Up;
                const bool Decrease =
                  Direction == Axis::Horizontal ? Event.Code == KeyCode::Left : Event.Code == KeyCode::Down;
                if (!Increase && !Decrease) { return false; }

                const float Step = (High - Low) * kKeyboardFraction *
                                   (HasFlags(Event.Modifiers, KeyModifiers::Shift) ? kFineFactor : 1.0f);
                Source.Commit(
                  std::clamp(Source.Peek() + (Increase ? Step : -Step), std::min(Low, High), std::max(Low, High)));
                return true;
            },
          .Child = Raw,
        };
    }

    BoundValue Knob::State::GetSource() const {
        const Knob& Props = GetWidget();
        return {Props.Value, Props.Bind, Props.OnChanged};
    }

    Node Knob::State::Build(BuildContext& Context) {
        const Theme& Style      = Theme::Of(Context);
        const Knob& Props       = GetWidget();
        const BoundValue Source = GetSource();
        const float Low         = std::min(Props.Min, Props.Max);
        const float High        = std::max(Props.Min, Props.Max);
        const float Range       = High - Low;

        const auto Clamp = [Low, High](float Candidate) { return std::clamp(Candidate, Low, High); };

        return Semantics {
          .Role       = SemanticsRole::Slider,
          .Label      = Props.Label,
          .Range      = SemanticsRange {Source.Peek(), Low, High, Range * kKeyboardFraction},
          .ReadRange  = [Source] { return Source.Peek(); },
          .OnSetRange = [Source, Clamp](float Value) { Source.Commit(Clamp(Value)); },
          .Child =
            Focus {
              .OnKey =
                [Source, Clamp, Range](const KeyEvent& Event) {
                    if (!Event.Pressed) { return false; }
                    const bool Increase = Event.Code == KeyCode::Up || Event.Code == KeyCode::Right;
                    const bool Decrease = Event.Code == KeyCode::Down || Event.Code == KeyCode::Left;
                    if (!Increase && !Decrease) { return false; }
                    const float Step =
                      Range * kKeyboardFraction * (HasFlags(Event.Modifiers, KeyModifiers::Shift) ? kFineFactor : 1.0f);
                    Source.Commit(Clamp(Source.Peek() + (Increase ? Step : -Step)));
                    return true;
                },
              .Child = GestureDetector {
                .OnDoubleTap = [Source, Default = Props.Default](const TapEvent&) { Source.Commit(Default); },
                .OnDragStart = [this, Source](const DragEvent&) { _DragValue = Source.Peek(); },
                .OnDragUpdate =
                  [this, Source, Clamp, Range](const DragEvent& Drag) {
                      const float Sensitivity = HasFlags(Drag.Modifiers, KeyModifiers::Shift) ? kFineFactor : 1.0f;
                      _DragValue              = Clamp(_DragValue - Drag.Delta.Y / kKnobDragRange * Range * Sensitivity);
                      Source.Commit(_DragValue);
                  },
                .RelativeDrag = true,
                .Child =
                  Listener {
                    .OnWheel =
                      [Source, Clamp, Range](const PointerEvent& Event) {
                          const float Sensitivity = HasFlags(Event.Modifiers, KeyModifiers::Shift) ? kFineFactor : 1.0f;
                          Source.Commit(Clamp(Source.Peek() + Event.WheelDelta.Y * Range * 0.02f * Sensitivity));
                          return true;
                      },
                    .Child =
                      MouseRegion {
                        .Cursor = CursorShape::ResizeVertical,
                        .Child =
                          CustomPaint {
                            .OnPaint =
                              [Source, Low, Range, Bipolar = Props.Bipolar, Style](Painter& Canvas,
                                                                                   const Size& Extent) {
                                  const float Fraction = Range > 0.0f ? (Source.Get() - Low) / Range : 0.0f;
                                  const Vec2 Center {Extent.Width * 0.5f, Extent.Height * 0.5f};
                                  const float Radius = std::min(Extent.Width, Extent.Height) * 0.5f - 3.0f;
                                  const float Angle  = -kKnobSweep + Fraction * kKnobSweep * 2.0f;
                                  const float Origin = Bipolar ? 0.0f : -kKnobSweep;

                                  const StrokeStyle Ring {.Width = 3.0f, .Cap = LineCap::Round};
                                  Canvas.FillCircle(Center, Radius - 3.0f, Style.SurfaceRaised);
                                  Canvas.StrokePath(BuildArc(Center, Radius, -kKnobSweep, kKnobSweep),
                                                    Style.Border,
                                                    Ring);
                                  if (std::abs(Angle - Origin) > 0.01f) {
                                      Canvas.StrokePath(BuildArc(Center, Radius, Origin, Angle), Style.Accent, Ring);
                                  }
                                  Canvas.DrawLine(PointOnCircle(Center, Radius * 0.25f, Angle),
                                                  PointOnCircle(Center, Radius - 5.0f, Angle),
                                                  Style.Text,
                                                  {.Width = 2.0f, .Cap = LineCap::Round});
                              },
                            .Width  = Props.Diameter,
                            .Height = Props.Diameter,
                          },
                      },
                  },
              }}};
    }

    Node Tooltip::State::Build(BuildContext& Context) {
        _Overlay = OverlayController::Of(Context);
        _Style   = PopupStyle::Capture(Context);

        return SemanticsHint {
          .Hint  = GetWidget().Message,
          .Child = MouseRegion {
            .OnEnter = [this](const PointerEvent&) { _Timer = Timer::After(GetWidget().Delay, [this] { Show(); }); },
            .OnExit  = [this](const PointerEvent&) { Hide(); },
            .Child   = Listener {.OnDown = [this](const PointerEvent&) { Hide(); }, .Child = GetWidget().Child},
          }};
    }

    void Tooltip::State::Show() {
        if (_Overlay == nullptr || GetWidget().Message.empty()) { return; }

        Rect Anchor = GetGlobalBounds(GetElement());
        Anchor      = {Anchor.X, Anchor.Y, Anchor.Width, Anchor.Height + 4.0f};
        _Entry      = _Overlay->ShowPopup({
          .Anchor = Anchor,
          .Kind   = PopupKind::Tooltip,
          .Builder =
            [Message = GetWidget().Message, Style = _Style] {
                const Theme& Palette = Style.Palette;
                return Style.Wrap(Container {
                  .Padding      = EdgeInsets::Symmetric(8.0f, 4.0f),
                  .Background   = Palette.SurfaceRaised,
                  .CornerRadius = Palette.CornerRadius,
                  .BorderColor  = Palette.Border,
                  .Child        = Text {.Content  = Message,
                                        .Font     = Palette.SmallFont,
                                        .Color    = Palette.Text,
                                        .Wrapping = TextWrapping::NoWrap},
                });
            },
          .Background = _Style.Palette.SurfaceRaised,
        });
    }

    void Tooltip::State::Hide() {
        _Timer.Cancel();
        _Entry.Reset();
    }

    Node MenuPanel::State::Build(BuildContext& Context) {
        const Theme& Style     = Theme::Of(Context);
        const MenuPanel& Props = GetWidget();
        const int Highlighted  = _Highlighted();

        NodeList Rows;
        for (size_t i = 0; i < Props.Items.size(); ++i) {
            const MenuItem& Item = Props.Items[i];
            const int Index      = static_cast<int>(i);
            if (Item.Separator) {
                Rows.push_back(
                  Container {.Height = 1.0f, .Margin = EdgeInsets::Symmetric(4.0f, 4.0f), .Background = Style.Border});
                continue;
            }

            const bool Active     = Item.Enabled && Index == Highlighted;
            const Color LabelTint = !Item.Enabled ? Style.TextDisabled : (Active ? Style.AccentText : Style.Text);
            const Node Content    = Container {
              .Height       = kMenuRowHeight,
              .Padding      = EdgeInsets::Symmetric(8.0f, 0.0f),
              .Background   = Active ? Style.Accent : Style.Accent.WithAlpha(0.0f),
              .CornerRadius = 3.0f,
              .Child =
                Row {
                  .Spacing = 8.0f,
                  .Children =
                    {
                      SizedBox {.Width = 12.0f,
                                .Child = Item.Checked
                                           ? Node(Text {.Content = "✓", .Font = Style.SmallFont, .Color = LabelTint})
                                           : Node {}},
                      Expanded {.Child = Text {.Content  = Item.Label,
                                               .Font     = Style.BodyFont,
                                               .Color    = LabelTint,
                                               .Wrapping = TextWrapping::NoWrap}},
                      Text {.Content = Item.ShortcutText,
                            .Font    = Style.SmallFont,
                            .Color   = Active ? Style.AccentText : Style.TextMuted},
                    },
                },
            };

            const std::optional<bool> Checked = Item.Checked ? std::optional<bool>(true) : std::nullopt;
            if (!Item.Enabled) {
                Rows.push_back(Semantics {.Role        = SemanticsRole::MenuItem,
                                          .Label       = Item.Label,
                                          .Description = Item.ShortcutText,
                                          .Toggled     = Checked,
                                          .Enabled     = false,
                                          .Child       = Content});
                continue;
            }
            Rows.push_back(Semantics {
              .Role        = SemanticsRole::MenuItem,
              .Label       = Item.Label,
              .Description = Item.ShortcutText,
              .Toggled     = Checked,
              .Selected    = Index == Highlighted,
              .OnInvoke    = [this, Index] { Select(Index); },
              .Child =
                MouseRegion {
                  .OnEnter = [this, Index](const PointerEvent&) { _Highlighted = Index; },
                  .Child =
                    GestureDetector {.OnTap = [this, Index](const TapEvent&) { Select(Index); }, .Child = Content},
                },
            });
        }

        return Semantics {.Role  = SemanticsRole::Menu,
                          .Child = Focus {
                            .Target    = &_Focus,
                            .AutoFocus = true,
                            .OnKey =
                              [this](const KeyEvent& Event) {
                                  if (!Event.Pressed) { return false; }
                                  switch (Event.Code) {
                                      case KeyCode::Down:
                                          Move(1);
                                          return true;
                                      case KeyCode::Up:
                                          Move(-1);
                                          return true;
                                      case KeyCode::Enter:
                                      case KeyCode::Space:
                                          Select(_Highlighted.Peek());
                                          return true;
                                      case KeyCode::Escape:
                                          if (const std::function<void()> Dismiss = GetWidget().OnDismiss) {
                                              Dismiss();
                                          }
                                          return true;
                                      case KeyCode::Left:
                                      case KeyCode::Right:
                                          if (const std::function<void(int)> Sideways = GetWidget().OnSideways) {
                                              Sideways(Event.Code == KeyCode::Left ? -1 : 1);
                                              return true;
                                          }
                                          return false;
                                      default:
                                          return false;
                                  }
                              },
                            .Behavior = HitTestBehavior::Opaque,
                            .Child =
                              Container {
                                .Width        = Props.Width,
                                .Padding      = EdgeInsets::All(4.0f),
                                .Background   = Style.SurfaceRaised,
                                .CornerRadius = 6.0f,
                                .BorderColor  = Style.Border,
                                .Child        = Column {.CrossAlignment = CrossAxisAlignment::Stretch,
                                                        .MainSize       = MainAxisSize::Min,
                                                        .Children       = std::move(Rows)},
                              },
                          }};
    }

    void MenuPanel::State::Move(int Direction) {
        const std::vector<MenuItem>& Entries = GetWidget().Items;
        const int Count                      = static_cast<int>(Entries.size());
        int Candidate                        = _Highlighted.Peek();
        for (int Attempt = 0; Attempt < Count; ++Attempt) {
            Candidate = Candidate < 0 ? (Direction > 0 ? 0 : Count - 1) : (Candidate + Direction + Count) % Count;
            const MenuItem& Item = Entries[static_cast<size_t>(Candidate)];
            if (!Item.Separator && Item.Enabled) {
                _Highlighted = Candidate;
                return;
            }
        }
    }

    void MenuPanel::State::Select(int Index) const {
        const std::vector<MenuItem>& Entries = GetWidget().Items;
        if (Index < 0 || Index >= static_cast<int>(Entries.size())) { return; }

        const MenuItem& Item = Entries[static_cast<size_t>(Index)];
        if (Item.Separator || !Item.Enabled) { return; }

        const std::function<void()> Action  = Item.OnSelect;
        const std::function<void()> Dismiss = GetWidget().OnDismiss;
        if (Dismiss) { Dismiss(); }
        if (Action) { Action(); }
    }

    OverlayEntry ShowMenu(OverlayController& Overlay,
                          const Rect& Anchor,
                          PopupPlacement Placement,
                          std::vector<MenuItem> Items,
                          std::function<void()> OnClosed,
                          const PopupStyle& Style,
                          float Width,
                          std::function<void(int)> OnSideways) {
        return Overlay.ShowPopup({
          .Anchor    = Anchor,
          .Placement = Placement,
          .Kind      = PopupKind::Menu,
          .Builder =
            [Items = std::move(Items), OnClosed, Width, Style, OnSideways] {
                return Style.Wrap(
                  MenuPanel {.Items = Items, .OnDismiss = OnClosed, .Width = Width, .OnSideways = OnSideways});
            },
          .OnDismissed = OnClosed,
          .Background  = Style.Palette.SurfaceRaised,
        });
    }

    Node ContextMenu::State::Build(BuildContext& Context) {
        _Overlay = OverlayController::Of(Context);
        _Style   = PopupStyle::Capture(Context);
        return GestureDetector {
          .OnSecondaryTap = [this](const TapEvent& Tap) { Open(Tap.LocalPosition); },
          .Child          = GetWidget().Child,
        };
    }

    void ContextMenu::State::Open(const Vec2& LocalPosition) {
        if (_Overlay == nullptr) { return; }
        const Vec2 Origin = GetGlobalBounds(GetElement()).Origin() + LocalPosition;
        _Menu             = ShowMenu(
          *_Overlay,
          {Origin.X, Origin.Y, 0.0f, 0.0f},
          PopupPlacement::Below,
          GetWidget().Items,
          [this] { _Menu.Reset(); },
          _Style);
    }

    Node Select::State::Build(BuildContext& Context) {
        _Overlay            = OverlayController::Of(Context);
        _Style              = PopupStyle::Capture(Context);
        const Theme& Style  = Theme::Of(Context);
        const Select& Props = GetWidget();
        const bool Hovered  = _Hovered();
        const std::string Current =
          Props.SelectedIndex >= 0 && Props.SelectedIndex < static_cast<int>(Props.Options.size())
            ? Props.Options[static_cast<size_t>(Props.SelectedIndex)]
            : std::string();

        const bool Focused = _Focus.HasFocus();
        return Semantics {.Role     = SemanticsRole::ComboBox,
                          .Label    = Props.Label,
                          .Value    = Current,
                          .Expanded = _Menu.IsShown(),
                          .ReadOnly = true,
                          .OnExpand =
                            [this](bool Expand) {
                                if (Expand) {
                                    Open();
                                } else {
                                    Close();
                                }
                            },
                          .Child =
                            Focus {.Target = &_Focus,
                                   .OnKey =
                                     [this](const KeyEvent& Event) {
                                         if (!Event.Pressed || Event.Repeat) { return false; }
                                         const bool Opens = Event.Code == KeyCode::Enter ||
                                                            Event.Code == KeyCode::Space || Event.Code == KeyCode::Down;
                                         if (!Opens || _Menu.IsShown()) { return false; }
                                         Open();
                                         return true;
                                     },
                                   .Child =
                                     MouseRegion {
                                       .OnEnter = [this](const PointerEvent&) { _Hovered = true; },
                                       .OnExit  = [this](const PointerEvent&) { _Hovered = false; },
                                       .Cursor  = CursorShape::Hand,
                                       .Child =
                                         GestureDetector {
                                           .OnTap = [this](const TapEvent&) { Open(); },
                                           .Child =
                                             Container {
                                               .Width        = Props.Width,
                                               .Height       = Style.ControlHeight,
                                               .Padding      = EdgeInsets::Symmetric(10.0f, 0.0f),
                                               .Background   = Hovered ? Style.Hover : Style.SurfaceRaised,
                                               .CornerRadius = Style.CornerRadius,
                                               .BorderColor  = _Menu.IsShown() || Focused ? Style.Accent : Style.Border,
                                               .Child =
                                                 Row {
                                                   .Spacing = 6.0f,
                                                   .Children =
                                                     {
                                                       Expanded {.Child = Text {.Content  = Current,
                                                                                .Font     = Style.BodyFont,
                                                                                .Wrapping = TextWrapping::NoWrap,
                                                                                .Trimming = TextTrimming::Ellipsis}},
                                                       ExcludeSemantics {.Child = Text {.Content = "▾",
                                                                                        .Font    = Style.SmallFont,
                                                                                        .Color   = Style.TextMuted}},
                                                     },
                                                 },
                                             },
                                         },
                                     }}};
    }

    void Select::State::Close() {
        _Menu.Reset();
        MarkNeedsBuild();
    }

    void Select::State::Open() {
        if (_Overlay == nullptr || _Menu.IsShown()) { return; }

        const Select& Props = GetWidget();
        std::vector<MenuItem> Items;
        for (size_t i = 0; i < Props.Options.size(); ++i) {
            Items.push_back({
              .Label = Props.Options[i],
              .OnSelect =
                [Changed = Props.OnChanged, Index = static_cast<int>(i)] {
                    if (Changed) { Changed(Index); }
                },
              .Checked = static_cast<int>(i) == Props.SelectedIndex,
            });
        }

        const Rect Anchor = GetGlobalBounds(GetElement());
        _Menu             = ShowMenu(
          *_Overlay,
          Anchor,
          PopupPlacement::Below,
          std::move(Items),
          [this] {
              _Menu.Reset();
              MarkNeedsBuild();
          },
          _Style,
          std::max(Anchor.Width, 120.0f));
        MarkNeedsBuild();
    }

    void SplitView::State::Init() {
        _Ratio = GetWidget().InitialRatio;
    }

    Node SplitView::State::Build(BuildContext& Context) {
        const Theme& Style     = Theme::Of(Context);
        const SplitView& Props = GetWidget();
        return RawSplit {
          .Configuration =
            {
              .Ratio        = Props.Ratio != nullptr ? Props.Ratio : &_Ratio,
              .Direction    = Props.Direction,
              .MinFirst     = Props.MinFirst,
              .MinSecond    = Props.MinSecond,
              .DividerColor = Style.Border,
              .ActiveColor  = Style.Accent,
            },
          .Children = {Props.First, Props.Second},
        };
    }
}  // namespace XenUI
