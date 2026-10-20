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

#include "TestWidgets.hpp"

#include <doctest/doctest.h>

#include <Windows.h>

#include <optional>
#include <string>

using namespace XenUI;
using namespace XenUI::Testing;

namespace {
    struct FieldHarness {
        explicit FieldHarness(const Node& Root) {
            (void)SharedApp();
            Host.SetViewportSize({400.0f, 60.0f});
            Host.SetRoot(Align {.Position = Alignment::TopLeft(), .Child = SizedBox {.Width = 300.0f, .Child = Root}});
            Host.Update();
        }

        void Type(const std::string& Text) {
            Host.GetInput().HandleText({Text});
            Host.Update();
        }

        bool Press(KeyCode Code, KeyModifiers Modifiers = KeyModifiers::None) {
            const bool Handled = Host.GetInput().HandleKey({.Code = Code, .Modifiers = Modifiers});
            Host.GetInput().HandleKey({.Code = Code, .Modifiers = Modifiers, .Pressed = false});
            Host.Update();
            return Handled;
        }

        void Click(Vec2 Position, double Time) {
            Host.GetInput().HandlePointer({.Phase    = PointerPhase::Down,
                                           .Position = Position,
                                           .Button   = MouseButton::Primary,
                                           .Buttons  = MouseButton::Primary,
                                           .Time     = Time});
            Host.GetInput().HandlePointer(
              {.Phase = PointerPhase::Up, .Position = Position, .Button = MouseButton::Primary, .Time = Time});
            Host.Update();
        }

        WidgetHost Host;
    };
}  // namespace

TEST_CASE("Text editor inserts, deletes and moves by characters and words") {
    TextEditor Editor;
    Editor.Insert("hello world");
    CHECK(Editor.GetText() == "hello world");
    CHECK(Editor.GetSelection().Caret == 11);

    Editor.Move(TextUnit::Word, false, false);
    CHECK(Editor.GetSelection().Caret == 6);
    Editor.Move(TextUnit::Word, false, true);
    CHECK(Editor.GetSelection() == TextSelection {6, 0});
    CHECK(Editor.GetSelectedText() == "hello ");

    Editor.Insert("goodbye ");
    CHECK(Editor.GetText() == "goodbye world");
    Editor.Move(TextUnit::Line, true, false);
    CHECK(Editor.Delete(TextUnit::Word, false));
    CHECK(Editor.GetText() == "goodbye ");
    CHECK(Editor.Delete(TextUnit::Character, false));
    CHECK(Editor.GetText() == "goodbye");

    Editor.Move(TextUnit::Line, false, false);
    Editor.Move(TextUnit::Word, true, false);
    CHECK(Editor.GetSelection().Caret == 7);
}

TEST_CASE("Text editor steps over multi-byte characters and respects caret stops") {
    TextEditor Editor("aé😀b");
    CHECK(Editor.GetText().size() == 1 + 2 + 4 + 1);
    Editor.Move(TextUnit::Character, false, false);
    CHECK(Editor.GetSelection().Caret == 7);
    Editor.Move(TextUnit::Character, false, false);
    CHECK(Editor.GetSelection().Caret == 3);
    CHECK(Editor.Delete(TextUnit::Character, false));
    CHECK(Editor.GetText() == "a😀b");

    TextEditor Combined("e\xCC\x81x");
    Combined.SetCaretStops({0, 3, 4});
    Combined.Move(TextUnit::Line, false, false);
    Combined.Move(TextUnit::Character, true, false);
    CHECK(Combined.GetSelection().Caret == 3);
}

TEST_CASE("Text editor groups typing into undo steps and limits length") {
    TextEditor Editor;
    for (const char* Letter : {"a", "b", "c", " ", "d", "e"}) {
        Editor.Insert(Letter);
    }
    CHECK(Editor.GetText() == "abc de");
    CHECK(Editor.Undo());
    CHECK(Editor.GetText() == "abc");
    CHECK(Editor.Undo());
    CHECK(Editor.GetText().empty());
    CHECK(Editor.Redo());
    CHECK(Editor.GetText() == "abc");

    Editor.SetMaxLength(5);
    Editor.Insert("line\nbreak");
    CHECK(Editor.GetText() == "abcli");

    Editor.SetMaxLength(0);
    Editor.SelectAll();
    Editor.Insert("one\r\ntwo");
    CHECK(Editor.GetText() == "one two");

    Editor.SelectWord(5);
    CHECK(Editor.GetSelectedText() == "two");
}

TEST_CASE("Text layouts hit test and report caret positions in UTF-8 offsets") {
    (void)SharedApp();
    const TextLayout Layout("añb", Font {.Size = 14.0f});
    const Rect First  = Layout.GetCaretBounds(0);
    const Rect Middle = Layout.GetCaretBounds(3);
    const Rect End    = Layout.GetCaretBounds(4);
    CHECK(First.X == doctest::Approx(0.0f));
    CHECK(Middle.X > First.X);
    CHECK(End.X > Middle.X);
    CHECK(Layout.HitTest({End.X + 50.0f, 5.0f}) == 4);
    CHECK(Layout.HitTest({-10.0f, 5.0f}) == 0);
    CHECK(Layout.GetCaretStops() == std::vector<size_t> {0, 1, 3, 4});
    CHECK(Layout.GetRangeBounds(1, 3).size() == 1);
}

TEST_CASE("Text fields edit through keys and text, bind to signals and consume printable keys") {
    Signal<std::string> Value = std::string("hi");
    std::string Submitted;
    int Changes = 0;
    FieldHarness Test(TextField {
      .Bind      = &Value,
      .OnChanged = [&Changes](const std::string&) { ++Changes; },
      .OnSubmit  = [&Submitted](const std::string& Text) { Submitted = Text; },
      .AutoFocus = true,
    });

    Test.Type(" there");
    CHECK(Value.Peek() == "hi there");
    CHECK(Changes == 1);

    CHECK(Test.Press(KeyCode::Space));
    CHECK(Test.Press(KeyCode::Backspace, KeyModifiers::Control));
    CHECK(Value.Peek() == "hi ");
    CHECK(Test.Press(KeyCode::A, KeyModifiers::Control));
    Test.Type("new");
    CHECK(Value.Peek() == "new");
    CHECK(Test.Press(KeyCode::Z, KeyModifiers::Control));
    CHECK(Value.Peek() == "hi ");

    CHECK(Test.Press(KeyCode::Enter));
    CHECK(Submitted == "hi ");
    CHECK_FALSE(Test.Press(KeyCode::Escape));
    CHECK_FALSE(Test.Press(KeyCode::S, KeyModifiers::Control));

    Value = std::string("replaced");
    Test.Host.Update();
    Test.Type("!");
    CHECK(Value.Peek() == "replaced!");
}

TEST_CASE("Text fields place the caret where clicked and select words on double-click") {
    TextEditingController Controller("alpha beta");
    FieldHarness Test(TextField {.Controller = &Controller});

    Test.Click({290.0f, 13.0f}, 1.0);
    CHECK(Controller.PeekText() == "alpha beta");
    CHECK(Controller.GetEditor().GetSelection().Caret == 10);

    Test.Click({12.0f, 13.0f}, 3.0);
    CHECK(Controller.GetEditor().GetSelection().Caret <= 1);
    Test.Click({12.0f, 13.0f}, 3.1);
    CHECK(Controller.GetEditor().GetSelectedText() == "alpha");
}

TEST_CASE("Text field compositions display without changing the text until committed") {
    TextEditingController Controller("ab");
    FieldHarness Test(TextField {.Controller = &Controller, .AutoFocus = true});

    TextInputClient* Client = Test.Host.GetInput().GetFocusManager().GetTextInput();
    REQUIRE(Client == &Controller);
    Client->UpdateComposition("にほ", 6);
    Test.Host.Update();
    CHECK(Controller.PeekText() == "ab");
    CHECK(Controller.IsComposing());
    CHECK(Test.Press(KeyCode::Left));
    CHECK(Controller.GetEditor().GetSelection().Caret == 2);

    Client->UpdateComposition({}, 0);
    Test.Type("日本");
    CHECK(Controller.PeekText() == "ab日本");
    CHECK(Client->GetCaretBounds().Height > 0.0f);
}

TEST_CASE("Clipboard round-trips text and custom data") {
    const bool SafeToReplace = CountClipboardFormats() == 0 || Clipboard::HasText();
    if (!SafeToReplace) {
        MESSAGE("Skipped: the clipboard holds non-text data that this test would replace");
        return;
    }
    const std::optional<std::string> Saved = Clipboard::GetText();

    CHECK(Clipboard::SetText("first line\nsecond ✓"));
    CHECK(Clipboard::GetText() == "first line\nsecond ✓");

    const std::vector<uint8_t> Payload {1, 2, 3, 0, 255};
    CHECK(Clipboard::SetData("XenUI.Test", Payload, "fallback"));
    CHECK(Clipboard::HasData("XenUI.Test"));
    CHECK(Clipboard::GetData("XenUI.Test") == Payload);
    CHECK(Clipboard::GetText() == "fallback");

    if (Saved) {
        Clipboard::SetText(*Saved);
    } else if (OpenClipboard(nullptr) != FALSE) {
        EmptyClipboard();
        CloseClipboard();
    }
}

TEST_CASE("Preempting shortcuts beat focused controls but not typing") {
    int Plays                 = 0;
    int Presses               = 0;
    Signal<std::string> Typed = std::string();
    FocusNode ButtonFocus;
    WidgetHost Host;
    (void)SharedApp();
    Host.SetViewportSize({400.0f, 200.0f});
    Host.SetRoot(Shortcuts {
      .Bindings = {{.Keys = {KeyCombination {KeyCode::Space}}, .Action = [&Plays] { ++Plays; }, .Preempts = true}},
      .Child    = Column {.Children =
                            {
                           Focus {.Target = &ButtonFocus,
                                     .OnKey =
                                    [&Presses](const KeyEvent& Event) {
                                        if (Event.Pressed && Event.Code == KeyCode::Space) {
                                            ++Presses;
                                            return true;
                                        }
                                        return false;
                                    },
                                     .Child = SizedBox {.Width = 50.0f, .Height = 20.0f}},
                           SizedBox {.Width = 300.0f, .Child = TextField {.Bind = &Typed, .AutoFocus = true}},
                         }},
    });
    Host.Update();

    CHECK(Host.GetInput().HandleKey({.Code = KeyCode::Space}));
    Host.GetInput().HandleText({" "});
    Host.Update();
    CHECK(Typed.Peek() == " ");
    CHECK(Plays == 0);

    ButtonFocus.RequestFocus();
    Host.Update();
    CHECK(Host.GetInput().HandleKey({.Code = KeyCode::Space}));
    CHECK(Plays == 1);
    CHECK(Presses == 0);
}
