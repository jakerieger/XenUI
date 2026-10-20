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

#include <XenUI/XenUI.hpp>

#include <doctest/doctest.h>

#include <string>
#include <vector>

using namespace XenUI;

namespace {
    class FakeHost final : public InputHost {
    public:
        void SetCursor(CursorShape Shape) override { Cursor = Shape; }
        void BeginRelativePointer() override { Relative = true; }
        void EndRelativePointer() override { Relative = false; }

        CursorShape Cursor = CursorShape::Arrow;
        bool Relative      = false;
    };

    struct Harness {
        explicit Harness(const Node& Root) {
            Host.SetViewportSize({400.0f, 300.0f});
            Host.SetRoot(Root);
            Host.GetInput().SetHost(&Platform);
            Host.Update();
        }

        void Down(Vec2 Position, MouseButton Button = MouseButton::Primary, double Time = 0.0) {
            Send(
              {.Phase = PointerPhase::Down, .Position = Position, .Button = Button, .Buttons = Button, .Time = Time});
        }

        void Move(Vec2 Position, MouseButton Held = MouseButton::None) {
            Send({.Phase = PointerPhase::Move, .Position = Position, .Buttons = Held});
        }

        void Up(Vec2 Position, MouseButton Button = MouseButton::Primary, double Time = 0.0) {
            Send({.Phase = PointerPhase::Up, .Position = Position, .Button = Button, .Time = Time});
        }

        void Click(Vec2 Position, double Time = 0.0) {
            Down(Position, MouseButton::Primary, Time);
            Up(Position, MouseButton::Primary, Time);
        }

        bool Press(KeyCode Code, KeyModifiers Modifiers = KeyModifiers::None, double Time = 0.0) {
            const bool Handled = Host.GetInput().HandleKey({.Code = Code, .Modifiers = Modifiers, .Time = Time});
            Host.Update();
            return Handled;
        }

        void Send(const PointerEvent& Event) {
            Host.GetInput().HandlePointer(Event);
            Host.Update();
        }

        WidgetHost Host;
        FakeHost Platform;
    };

    Node Block(float Width, float Height) {
        return Container {.Width = Width, .Height = Height, .Background = Color::FromHex(0x404040)};
    }
}  // namespace

TEST_CASE("Taps reach the topmost detector under the pointer") {
    int Bottom = 0;
    int Top    = 0;
    Harness Test(
      Stack {.Children = {
               GestureDetector {.OnTap = [&](const TapEvent&) { ++Bottom; }, .Child = Block(200, 200)},
               Positioned {.Left  = 50.0f,
                           .Top   = 50.0f,
                           .Child = GestureDetector {.OnTap = [&](const TapEvent&) { ++Top; }, .Child = Block(50, 50)}},
             }});

    Test.Click({60.0f, 60.0f});
    CHECK(Top == 1);
    CHECK(Bottom == 0);

    Test.Click({10.0f, 10.0f});
    CHECK(Bottom == 1);

    Test.Click({300.0f, 250.0f});
    CHECK(Bottom == 1);
}

TEST_CASE("Taps report local positions and survive small movement") {
    Vec2 Seen;
    Harness Test(Align {
      .Position = Alignment::TopLeft(),
      .Child    = Padding {.Insets = {.Left = 30.0f, .Top = 40.0f},
                           .Child  = GestureDetector {.OnTap = [&](const TapEvent& Tap) { Seen = Tap.LocalPosition; },
                                                      .Child = Block(100, 100)}}});
    Test.Down({35.0f, 45.0f});
    Test.Move({37.0f, 46.0f}, MouseButton::Primary);
    Test.Up({37.0f, 46.0f});
    CHECK(Seen == Vec2 {7.0f, 6.0f});
}

TEST_CASE("Moving past the threshold turns a press into a drag for the deepest drag handler") {
    int InnerTaps = 0;
    std::vector<std::string> Log;
    Vec2 Total;
    Harness Test(GestureDetector {
      .OnDragStart  = [&](const DragEvent&) { Log.push_back("start"); },
      .OnDragUpdate = [&](const DragEvent& Drag) { Total = Drag.TotalDelta; },
      .OnDragEnd    = [&](const DragEvent& Drag) { Log.push_back(Drag.Cancelled ? "cancel" : "end"); },
      .Child =
        Center {.Child = GestureDetector {.OnTap = [&](const TapEvent&) { ++InnerTaps; }, .Child = Block(100, 100)}},
    });

    Test.Down({200.0f, 150.0f});
    Test.Move({203.0f, 150.0f}, MouseButton::Primary);
    CHECK(Log.empty());
    Test.Move({220.0f, 160.0f}, MouseButton::Primary);
    Test.Move({230.0f, 170.0f}, MouseButton::Primary);
    Test.Up({230.0f, 170.0f});

    CHECK(Log == std::vector<std::string> {"start", "end"});
    CHECK(Total == Vec2 {30.0f, 20.0f});
    CHECK(InnerTaps == 0);

    Test.Click({200.0f, 150.0f});
    CHECK(InnerTaps == 1);
}

TEST_CASE("Second tap within the interval is a double tap") {
    int Taps    = 0;
    int Doubles = 0;
    Harness Test(GestureDetector {
      .OnTap = [&](const TapEvent&) { ++Taps; },
      .OnDoubleTap =
        [&](const TapEvent& Tap) {
            CHECK(Tap.Count == 2);
            ++Doubles;
        },
      .Child = Block(400, 300),
    });

    Test.Click({10.0f, 10.0f}, 1.0);
    Test.Click({11.0f, 10.0f}, 1.2);
    CHECK(Taps == 2);
    CHECK(Doubles == 1);

    Test.Click({10.0f, 10.0f}, 5.0);
    CHECK(Doubles == 1);
}

TEST_CASE("Secondary taps are separate from primary taps") {
    int Primary   = 0;
    int Secondary = 0;
    Harness Test(GestureDetector {
      .OnTap          = [&](const TapEvent&) { ++Primary; },
      .OnSecondaryTap = [&](const TapEvent&) { ++Secondary; },
      .Child          = Block(400, 300),
    });

    Test.Down({5.0f, 5.0f}, MouseButton::Secondary);
    Test.Up({5.0f, 5.0f}, MouseButton::Secondary);
    CHECK(Secondary == 1);
    CHECK(Primary == 0);
}

TEST_CASE("Relative drags hide the cursor and consume raw motion") {
    float Value = 0.0f;
    Harness Test(GestureDetector {
      .OnDragUpdate = [&](const DragEvent& Drag) { Value -= Drag.Delta.Y; },
      .RelativeDrag = true,
      .Child        = Block(400, 300),
    });

    Test.Down({100.0f, 100.0f});
    Test.Move({100.0f, 90.0f}, MouseButton::Primary);
    CHECK(Test.Platform.Relative);
    CHECK(Test.Platform.Cursor == CursorShape::Hidden);
    CHECK(Value == doctest::Approx(10.0f));

    Test.Move({100.0f, 0.0f}, MouseButton::Primary);
    CHECK(Value == doctest::Approx(10.0f));

    Test.Host.GetInput().HandleRelativeMotion({0.0f, -500.0f}, KeyModifiers::None);
    CHECK(Value == doctest::Approx(510.0f));

    Test.Up({100.0f, 100.0f});
    CHECK_FALSE(Test.Platform.Relative);
    CHECK(Test.Platform.Cursor == CursorShape::Arrow);
}

TEST_CASE("Cancelling ends drags and releases relative mode") {
    bool Cancelled = false;
    Harness Test(GestureDetector {
      .OnDragEnd    = [&](const DragEvent& Drag) { Cancelled = Drag.Cancelled; },
      .RelativeDrag = true,
      .Child        = Block(400, 300),
    });

    Test.Down({100.0f, 100.0f});
    Test.Move({100.0f, 120.0f}, MouseButton::Primary);
    Test.Host.GetInput().CancelAll();
    CHECK(Cancelled);
    CHECK_FALSE(Test.Platform.Relative);
}

TEST_CASE("Hover enter and exit follow the pointer, and the deepest region sets the cursor") {
    std::vector<std::string> Log;
    Harness Test(Row {.Children = {
                        MouseRegion {.OnEnter = [&](const PointerEvent&) { Log.push_back("enter A"); },
                                     .OnExit  = [&](const PointerEvent&) { Log.push_back("exit A"); },
                                     .Cursor  = CursorShape::Hand,
                                     .Child   = Block(100, 100)},
                        MouseRegion {.OnEnter = [&](const PointerEvent&) { Log.push_back("enter B"); },
                                     .OnExit  = [&](const PointerEvent&) { Log.push_back("exit B"); },
                                     .Child   = Block(100, 100)},
                      }});

    Test.Move({50.0f, 150.0f});
    CHECK(Test.Platform.Cursor == CursorShape::Hand);
    Test.Move({150.0f, 150.0f});
    CHECK(Test.Platform.Cursor == CursorShape::Arrow);
    Test.Move({350.0f, 150.0f});
    Test.Send({.Phase = PointerPhase::Leave});

    CHECK(Log == std::vector<std::string> {"enter A", "exit A", "enter B", "exit B"});
}

TEST_CASE("Hover updates when content moves under a still pointer") {
    Signal<float> Offset = 0.0f;
    int Enters           = 0;
    struct Shifted {
        const Signal<float>* Offset = nullptr;
        int* Enters                 = nullptr;
        Node Build(BuildContext& Context) const {
            (void)Context;
            return Align {
              .Position = Alignment::TopLeft(),
              .Child    = Padding {.Insets = {.Left = Offset->Get()},
                                   .Child  = MouseRegion {.OnEnter = [Count = Enters](const PointerEvent&) { ++*Count; },
                                                          .Child   = Block(50, 50)}}};
        }
    };
    Harness Test(Shifted {&Offset, &Enters});

    Test.Move({120.0f, 10.0f});
    CHECK(Enters == 0);
    Offset = 100.0f;
    Test.Host.Update();
    CHECK(Enters == 1);
}

TEST_CASE("Wheel events bubble to the nearest listener that handles them") {
    float Scrolled = 0.0f;
    int Inner      = 0;
    Harness Test(Listener {
      .OnWheel =
        [&](const PointerEvent& Event) {
            Scrolled += Event.WheelDelta.Y;
            return true;
        },
      .Child = Center {.Child = Listener {.OnDown = [&](const PointerEvent&) { ++Inner; }, .Child = Block(100, 100)}},
    });

    Test.Send({.Phase = PointerPhase::Wheel, .Position = {200.0f, 150.0f}, .WheelDelta = {0.0f, -2.0f}});
    CHECK(Scrolled == doctest::Approx(-2.0f));
}

TEST_CASE("Tapping a focusable widget focuses it and keys reach it, then bubble") {
    FocusNode First;
    FocusNode Second;
    std::vector<std::string> Log;
    Harness Test(Focus {
      .OnKey =
        [&](const KeyEvent& Event) {
            Log.push_back("outer " + std::to_string(static_cast<int>(Event.Code)));
            return true;
        },
      .Child = Row {.Children =
                      {
                        Focus {.Target = &First,
                               .OnKey =
                                 [&](const KeyEvent& Event) {
                                     if (Event.Code != KeyCode::A) { return false; }
                                     Log.push_back("first A");
                                     return true;
                                 },
                               .Child = Block(100, 100)},
                        Focus {.Target = &Second, .Child = Block(100, 100)},
                      }},
    });

    CHECK_FALSE(First.HasFocus());
    Test.Click({50.0f, 150.0f});
    CHECK(First.HasFocus());

    Test.Press(KeyCode::A);
    Test.Press(KeyCode::B);
    CHECK(Log == std::vector<std::string> {"first A", "outer 66"});

    Test.Click({150.0f, 150.0f});
    CHECK(Second.HasFocus());
    CHECK_FALSE(First.HasFocus());
}

TEST_CASE("Tab traversal cycles through focusable widgets in paint order") {
    FocusNode A;
    FocusNode B;
    FocusNode C;
    Harness Test(Column {.Children = {
                           Focus {.Target = &A, .AutoFocus = true, .Child = Block(10, 10)},
                           Focus {.Target = &B, .Child = Block(10, 10)},
                           Focus {.Target = &C, .CanRequestFocus = false, .Child = Block(10, 10)},
                         }});

    CHECK(A.HasFocus());
    CHECK(Test.Press(KeyCode::Tab));
    CHECK(B.HasFocus());
    Test.Press(KeyCode::Tab);
    CHECK(A.HasFocus());
    Test.Press(KeyCode::Tab, KeyModifiers::Shift);
    CHECK(B.HasFocus());
}

TEST_CASE("Focus state is a signal components can rebuild from") {
    FocusNode Tracked;
    struct Indicator {
        FocusNode* Target = nullptr;
        Node Build(BuildContext& Context) const {
            (void)Context;
            return Focus {.Target = Target, .Child = Block(Target->HasFocus() ? 80.0f : 40.0f, 40.0f)};
        }
    };
    Harness Test(Align {.Position = Alignment::TopLeft(), .Child = Indicator {&Tracked}});

    RenderBox& Root = *Test.Host.GetRootBox();
    CHECK(Root.GetChild(0).GetSize().Width == doctest::Approx(40.0f));
    Tracked.RequestFocus();
    Test.Host.Update();
    CHECK(Root.GetChild(0).GetSize().Width == doctest::Approx(80.0f));
}

TEST_CASE("Shortcuts run for single combinations and chords") {
    int Saves  = 0;
    int Chords = 0;
    Harness Test(Shortcuts {
      .Bindings =
        {
          {{KeyModifiers::Control + KeyCode::S}, [&] { ++Saves; }},
          {{KeyModifiers::Control + KeyCode::K, KeyModifiers::Control + KeyCode::C}, [&] { ++Chords; }},
        },
      .Child = Focus {.AutoFocus = true, .Child = Block(10, 10)},
    });

    CHECK(Test.Press(KeyCode::S, KeyModifiers::Control));
    CHECK(Saves == 1);

    CHECK(Test.Press(KeyCode::K, KeyModifiers::Control, 1.0));
    CHECK(Chords == 0);
    CHECK(Test.Press(KeyCode::C, KeyModifiers::Control, 1.2));
    CHECK(Chords == 1);

    Test.Press(KeyCode::K, KeyModifiers::Control, 2.0);
    CHECK_FALSE(Test.Press(KeyCode::C, KeyModifiers::Control, 5.0));
    CHECK(Chords == 1);

    CHECK_FALSE(Test.Press(KeyCode::S));
    CHECK((KeyModifiers::Control | KeyModifiers::Shift + KeyCode::S).Modifiers ==
          (KeyModifiers::Control | KeyModifiers::Shift));
}

TEST_CASE("Destroyed targets are skipped safely mid-gesture") {
    Signal<bool> Show = true;
    int Taps          = 0;
    struct Toggle {
        const Signal<bool>* Show = nullptr;
        int* Taps                = nullptr;
        Node Build(BuildContext& Context) const {
            (void)Context;
            if (!Show->Get()) { return Block(400, 300); }
            return GestureDetector {.OnTap = [Count = Taps](const TapEvent&) { ++*Count; }, .Child = Block(400, 300)};
        }
    };
    Harness Test(Toggle {&Show, &Taps});

    Test.Down({10.0f, 10.0f});
    Show = false;
    Test.Host.Update();
    Test.Up({10.0f, 10.0f});
    CHECK(Taps == 0);
}

TEST_CASE("Custom title bars report caption, buttons and client areas to the window") {
    WindowChrome Chrome;
    WidgetHost Host;
    Host.SetViewportSize({400.0f, 300.0f});
    Host.SetWindowChrome(&Chrome);
    Host.SetRoot(Column {
      .CrossAlignment = CrossAxisAlignment::Stretch,
      .Children =
        {
          WindowDragArea {.Child = Row {.Children =
                                          {
                                            SizedBox {.Width = 60.0f, .Height = 32.0f},
                                            GestureDetector {.OnTap    = [](const TapEvent&) {},
                                                             .Behavior = HitTestBehavior::Opaque,
                                                             .Child    = SizedBox {.Width = 50.0f, .Height = 32.0f}},
                                            Spacer {},
                                            WindowControls {},
                                          }}},
          Expanded {.Child = Listener {.Behavior = HitTestBehavior::Opaque, .Child = SizedBox {}}},
        },
    });
    Host.Update();

    const InputDispatcher& Input = Host.GetInput();
    CHECK(Input.HitTestWindowRegion({30.0f, 16.0f}) == WindowRegion::Caption);
    CHECK(Input.HitTestWindowRegion({200.0f, 16.0f}) == WindowRegion::Caption);
    CHECK(Input.HitTestWindowRegion({80.0f, 16.0f}) == WindowRegion::Client);
    CHECK(Input.HitTestWindowRegion({400.0f - 46.0f * 2.5f, 16.0f}) == WindowRegion::Minimize);
    CHECK(Input.HitTestWindowRegion({400.0f - 46.0f * 1.5f, 16.0f}) == WindowRegion::Maximize);
    CHECK(Input.HitTestWindowRegion({399.0f, 16.0f}) == WindowRegion::Close);
    CHECK(Input.HitTestWindowRegion({200.0f, 150.0f}) == WindowRegion::Client);
}

TEST_CASE("Window controls render nothing without a custom title bar") {
    WidgetHost Host;
    Host.SetViewportSize({400.0f, 100.0f});
    Host.SetRoot(Align {.Position = Alignment::TopRight(), .Child = WindowControls {}});
    Host.Update();
    CHECK(Host.GetInput().HitTestWindowRegion({399.0f, 10.0f}) == WindowRegion::Client);
}
