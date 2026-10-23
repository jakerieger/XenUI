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

#include "XenUI/Widgets/TextField.hpp"
#include "XenUI/Platform/Clipboard.hpp"
#include "XenUI/Widgets/Basic.hpp"
#include "XenUI/Widgets/Input.hpp"
#include "XenUI/Widgets/Theme.hpp"

#include <algorithm>
#include <cmath>

namespace XenUI {
    namespace {
        constexpr uint16_t kProcessKey    = 0xE5;
        constexpr float kMultilinePadding = 6.0f;

        float LineHeight(const XenUI::Font& TextFont) {
            return std::ceil(TextLayout("Ag", TextFont).GetMeasuredSize().Height);
        }
    }  // namespace

    Node TextArea::Build(BuildContext& Context) const {
        (void)Context;
        return TextField {
          .Controller     = Controller,
          .Bind           = Bind,
          .InitialText    = InitialText,
          .OnChanged      = OnChanged,
          .OnSubmit       = OnSubmit,
          .OnFocusChanged = OnFocusChanged,
          .Placeholder    = Placeholder,
          .TextFont       = TextFont,
          .FocusTarget    = FocusTarget,
          .AutoFocus      = AutoFocus,
          .Enabled        = Enabled,
          .MaxLength      = MaxLength,
          .Width          = Width,
          .Label          = Label,
          .Lines          = std::max(Lines, 2),
        };
    }

    TextEditingController& TextField::State::GetController() {
        TextEditingController* External = GetWidget().Controller;
        return External != nullptr ? *External : _InternalController;
    }

    FocusNode& TextField::State::GetFocus() {
        FocusNode* External = GetWidget().FocusTarget;
        return External != nullptr ? *External : _InternalFocus;
    }

    void TextField::State::Init() {
        const TextField& Props = GetWidget();
        GetController().GetEditor().SetSingleLine(Props.Lines <= 1);
        if (Props.Bind != nullptr) {
            GetController().SetText(Props.Bind->Peek());
        } else if (Props.Controller == nullptr && !Props.InitialText.empty()) {
            _InternalController.SetText(Props.InitialText);
        }
        Attach();
    }

    void TextField::State::WidgetUpdated(const TextField& Previous) {
        if (Previous.Controller != GetWidget().Controller) {
            TextEditingController& Old = Previous.Controller != nullptr ? *Previous.Controller : _InternalController;
            Old.SetOnTextChanged({});
        }
        Attach();
    }

    void TextField::State::Dispose() {
        _Blink.Cancel();
        _FocusNotice.Cancel();
        GetController().SetOnTextChanged({});
    }

    void TextField::State::Attach() {
        TextEditingController& Editing = GetController();
        Editing.GetEditor().SetSingleLine(GetWidget().Lines <= 1);
        Editing.GetEditor().SetMaxLength(GetWidget().MaxLength);
        Editing.SetOnTextChanged([this] { TextChanged(); });
    }

    void TextField::State::TextChanged() {
        RestartBlink();
        if (_Syncing) { return; }
        const TextField& Props  = GetWidget();
        const std::string& Text = GetController().PeekText();
        if (Props.Bind != nullptr && Props.Bind->Peek() != Text) { Props.Bind->Set(Text); }
        if (Props.OnChanged) { Props.OnChanged(Text); }
    }

    void TextField::State::RestartBlink() {
        _CaretOn = true;
        if (!_WasFocused) {
            _Blink.Cancel();
            return;
        }
        _Blink = Timer::Every(kBlinkInterval, [this] { _CaretOn = !_CaretOn.Peek(); });
    }

    void TextField::State::FocusChanged(bool Focused) {
        _WasFocused                    = Focused;
        TextEditingController& Editing = GetController();
        if (Focused && GetWidget().SelectAllOnFocus) {
            Editing.Edit([](TextEditor& Editor) {
                Editor.SelectAll();
                return false;
            });
        }
        if (!Focused) {
            Editing.UpdateComposition({}, 0);
            Editing.GetEditor().BreakUndoGroup();
        }
        RestartBlink();
        if (GetWidget().OnFocusChanged) {
            _FocusNotice = Timer::After(0.0, [this, Focused] {
                if (GetWidget().OnFocusChanged) { GetWidget().OnFocusChanged(Focused); }
            });
        }
    }

    void TextField::State::Copy() {
        const std::string Selected = GetController().GetEditor().GetSelectedText();
        if (!Selected.empty()) { Clipboard::SetText(Selected); }
    }

    void TextField::State::Paste() {
        const std::optional<std::string> Pasted = Clipboard::GetText();
        if (!Pasted) { return; }
        GetController().Edit([&Pasted](TextEditor& Editor) { return Editor.Insert(*Pasted); });
    }

    bool TextField::State::HandleKey(const KeyEvent& Event) {
        const TextField& Props = GetWidget();
        if (Props.OnKey && Props.OnKey(Event)) { return true; }

        TextEditingController& Editing = GetController();
        const bool Control             = HasFlags(Event.Modifiers, KeyModifiers::Control);
        const bool Shift               = HasFlags(Event.Modifiers, KeyModifiers::Shift);
        const bool Alt                 = HasFlags(Event.Modifiers, KeyModifiers::Alt);
        if (Editing.IsComposing() || static_cast<uint16_t>(Event.Code) == kProcessKey) { return true; }
        if (!Event.Pressed) { return !Control && !Alt; }

        const auto Run = [&Editing, this](const std::function<bool(TextEditor&)>& Operation) {
            Editing.Edit(Operation);
            RestartBlink();
            return true;
        };
        const TextUnit Step  = Control ? TextUnit::Word : TextUnit::Character;
        const bool Multiline = Props.Lines > 1;

        if (Multiline) {
            switch (Event.Code) {
                case KeyCode::Up:
                case KeyCode::Down:
                    Editing.MoveVertical(Event.Code == KeyCode::Down ? 1 : -1, Shift);
                    RestartBlink();
                    return true;
                case KeyCode::PageUp:
                case KeyCode::PageDown: {
                    const int Page = std::max(1, Editing.GetVisibleLineCount() - 1);
                    Editing.MoveVertical(Event.Code == KeyCode::PageDown ? Page : -Page, Shift);
                    RestartBlink();
                    return true;
                }
                case KeyCode::Home:
                case KeyCode::End:
                    if (Control || !Editing.MoveToLineEdge(Event.Code == KeyCode::End, Shift)) {
                        Editing.Edit([&](TextEditor& Editor) {
                            Editor.Move(TextUnit::Line, Event.Code == KeyCode::End, Shift);
                            return false;
                        });
                    }
                    RestartBlink();
                    return true;
                case KeyCode::Enter:
                    if (Control) {
                        if (!Props.OnSubmit) { return false; }
                        Editing.GetEditor().BreakUndoGroup();
                        Props.OnSubmit(Editing.PeekText());
                        return true;
                    }
                    return Run([](TextEditor& Editor) { return Editor.Insert("\n"); });
                default:
                    break;
            }
        }

        switch (Event.Code) {
            case KeyCode::Left:
            case KeyCode::Right:
                return Run([&](TextEditor& Editor) {
                    Editor.Move(Step, Event.Code == KeyCode::Right, Shift);
                    return false;
                });
            case KeyCode::Home:
            case KeyCode::End:
                return Run([&](TextEditor& Editor) {
                    Editor.Move(TextUnit::Line, Event.Code == KeyCode::End, Shift);
                    return false;
                });
            case KeyCode::Backspace:
                return Run([&](TextEditor& Editor) { return Editor.Delete(Step, false); });
            case KeyCode::Delete:
                if (Shift && !Control) {
                    Copy();
                    return Run([](TextEditor& Editor) { return Editor.Delete(TextUnit::Character, true); });
                }
                return Run([&](TextEditor& Editor) { return Editor.Delete(Step, true); });
            case KeyCode::Insert:
                if (Control && !Shift) {
                    Copy();
                    return true;
                }
                if (Shift && !Control) {
                    Paste();
                    return true;
                }
                return false;
            case KeyCode::Enter:
                if (!Props.OnSubmit) { return false; }
                Editing.GetEditor().BreakUndoGroup();
                Props.OnSubmit(Editing.PeekText());
                return true;
            case KeyCode::Escape:
            case KeyCode::Tab:
            case KeyCode::Up:
            case KeyCode::Down:
            case KeyCode::PageUp:
            case KeyCode::PageDown:
                return false;
            default:
                break;
        }

        if (Control && !Alt) {
            switch (Event.Code) {
                case KeyCode::A:
                    return Run([](TextEditor& Editor) {
                        Editor.SelectAll();
                        return false;
                    });
                case KeyCode::C:
                    Copy();
                    return true;
                case KeyCode::X:
                    Copy();
                    return Run([](TextEditor& Editor) {
                        return Editor.GetSelection().IsCollapsed() ? false : Editor.Delete(TextUnit::Character, true);
                    });
                case KeyCode::V:
                    Paste();
                    return true;
                case KeyCode::Z:
                    return Run([Shift](TextEditor& Editor) { return Shift ? Editor.Redo() : Editor.Undo(); });
                case KeyCode::Y:
                    return Run([](TextEditor& Editor) { return Editor.Redo(); });
                default:
                    return false;
            }
        }
        if (Event.Code >= KeyCode::F1 && Event.Code <= KeyCode::F24) { return false; }
        return !Control && !Alt;
    }

    bool TextField::State::HandleText(const TextEvent& Event) {
        GetController().Edit([&Event](TextEditor& Editor) { return Editor.Insert(Event.Text); });
        RestartBlink();
        return true;
    }

    Node TextField::State::Build(BuildContext& Context) {
        const Theme& Style             = Theme::Of(Context);
        const TextField& Props         = GetWidget();
        TextEditingController& Editing = GetController();
        FocusNode& Keyboard            = GetFocus();

        if (Props.Bind != nullptr) {
            const std::string& Bound = Props.Bind->Get();
            if (Bound != Editing.PeekText() && !Editing.IsComposing()) {
                _Syncing = true;
                Editing.SetText(Bound);
                _Syncing = false;
            }
        }
        const bool Focused = Keyboard.HasFocus();
        if (Focused != _WasFocused) { FocusChanged(Focused); }
        const XenUI::Font Font = Props.TextFont.value_or(Style.BodyFont);
        const bool Multiline   = Props.Lines > 1;

        Node Editable = RawEditableText {
          .Controller = &Editing,
          .Style =
            {
              .TextFont         = Font,
              .Foreground       = Style.Text,
              .PlaceholderColor = Style.TextMuted,
              .SelectionColor   = Style.Accent.WithAlpha(0.35f),
              .CaretColor       = Style.Text,
              .Placeholder      = Props.Placeholder,
            },
          .Focused      = Focused,
          .Enabled      = Props.Enabled,
          .CaretVisible = &_CaretOn,
          .Label        = Props.Label,
          .Multiline    = Props.Lines > 1,
        };
        NodeList Children;
        if (!Props.Leading.IsEmpty()) { Children.push_back(Props.Leading); }
        Children.push_back(Expanded {.Child = Editable});

        return Focus {
          .Target          = &Keyboard,
          .AutoFocus       = Props.AutoFocus,
          .CanRequestFocus = Props.Enabled,
          .OnKey           = [this](const KeyEvent& Event) { return HandleKey(Event); },
          .OnText          = [this](const TextEvent& Event) { return HandleText(Event); },
          .TextInput       = &Editing,
          .Behavior        = HitTestBehavior::Opaque,
          .Child =
            Container {
              .Width        = Props.Width,
              .Height       = Multiline ? kMultilinePadding * 2.0f + LineHeight(Font) * static_cast<float>(Props.Lines)
                                        : Style.ControlHeight,
              .Padding      = EdgeInsets::Symmetric(8.0f, Multiline ? kMultilinePadding : 0.0f),
              .Background   = Style.Track,
              .CornerRadius = Style.CornerRadius,
              .BorderColor  = Focused && Props.Enabled ? Style.Accent : Style.Border,
              .Child = Row {.Spacing        = 6.0f,
                            .CrossAlignment = Multiline ? CrossAxisAlignment::Stretch : CrossAxisAlignment::Center,
                            .Children       = std::move(Children)},
            },
        };
    }
}  // namespace XenUI
