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

#include <functional>
#include <string>
#include <vector>

using namespace XenUI;
using namespace XenUI::Testing;

namespace {
    class FakeHost final : public InputHost {
    public:
        void SetCursor(CursorShape Shape) override { Cursor = Shape; }
        void BeginRelativePointer() override { Relative = true; }
        void EndRelativePointer() override { Relative = false; }

        CursorShape Cursor = CursorShape::Arrow;
        bool Relative      = false;
    };

    struct FakeClock {
        FakeClock() {
            TimerQueue::Get().SetTimeSource([this] { return Now; });
        }
        ~FakeClock() { TimerQueue::Get().SetTimeSource({}); }

        void Advance(double Seconds) {
            Now += Seconds;
            TimerQueue::Get().RunDue();
        }

        double Now = 1000.0;
    };

    struct Harness {
        explicit Harness(const Node& Root, Size Viewport = {400.0f, 300.0f}) {
            (void)SharedApp();
            Host.SetViewportSize(Viewport);
            Host.SetRoot(Root);
            Host.GetInput().SetHost(&Platform);
            Host.Update();
        }

        void Send(const PointerEvent& Event) {
            Host.GetInput().HandlePointer(Event);
            Host.Update();
        }

        void Click(Vec2 Position, MouseButton Button = MouseButton::Primary, double Time = 0.0) {
            Send({.Phase = PointerPhase::Move, .Position = Position});
            Send(
              {.Phase = PointerPhase::Down, .Position = Position, .Button = Button, .Buttons = Button, .Time = Time});
            Send({.Phase = PointerPhase::Up, .Position = Position, .Button = Button, .Time = Time});
        }

        void Drag(Vec2 From, Vec2 To) {
            Send({.Phase    = PointerPhase::Down,
                  .Position = From,
                  .Button   = MouseButton::Primary,
                  .Buttons  = MouseButton::Primary});
            Send({.Phase = PointerPhase::Move, .Position = To, .Buttons = MouseButton::Primary});
            Send({.Phase = PointerPhase::Up, .Position = To, .Button = MouseButton::Primary});
        }

        void Wheel(Vec2 Position, float Notches) {
            Send({.Phase = PointerPhase::Wheel, .Position = Position, .WheelDelta = {0.0f, Notches}});
        }

        bool Press(KeyCode Code, KeyModifiers Modifiers = KeyModifiers::None) {
            const bool Handled = Host.GetInput().HandleKey({.Code = Code, .Modifiers = Modifiers});
            Host.Update();
            return Handled;
        }

        [[nodiscard]] bool ShowsText(const std::string& Wanted) const {
            const std::function<bool(const RenderObject&)> Search = [&](const RenderObject& Node) {
                if (const auto* Paragraph = dynamic_cast<const RenderParagraph*>(&Node);
                    Paragraph != nullptr && Paragraph->GetText() == Wanted) {
                    return true;
                }
                for (const RenderObject* Child : Node.GetChildren()) {
                    if (Search(*Child)) { return true; }
                }
                return false;
            };
            return Search(*Host.GetRootBox()->GetParent());
        }

        WidgetHost Host;
        FakeHost Platform;
    };

    Node Block(float Width, float Height, uint32_t Tint = 0x404040) {
        return Container {.Width = Width, .Height = Height, .Background = Color::FromHex(Tint)};
    }

    int ContentPaints = 0;
    int LiveRows      = 0;
    int BuiltRows     = 0;

    struct CountedRow {
        int Index = 0;

        class State : public ComponentState<CountedRow> {
        public:
            State() { ++LiveRows; }
            ~State() override { --LiveRows; }

        protected:
            Node Build(BuildContext& Context) override {
                (void)Context;
                ++BuiltRows;
                return Block(100, 20);
            }
        };
    };
}  // namespace

TEST_CASE("Timers fire once or repeatedly on the queue's clock, and cancel with their handle") {
    FakeClock Clock;
    int Once    = 0;
    int Repeats = 0;

    Timer Single = Timer::After(1.0, [&] { ++Once; });
    {
        Timer Repeating = Timer::Every(0.5, [&] { ++Repeats; });
        Clock.Advance(0.5);
        Clock.Advance(0.5);
        CHECK(Repeats == 2);
        CHECK(Once == 1);
        CHECK_FALSE(Single.IsActive());
    }
    Clock.Advance(5.0);
    CHECK(Repeats == 2);

    Timer Cancelled = Timer::After(1.0, [&] { ++Once; });
    Cancelled.Cancel();
    Clock.Advance(2.0);
    CHECK(Once == 1);
}

TEST_CASE("ScrollView scrolls with the wheel, clamps, and moves content without repainting it") {
    ScrollController Controller;
    int Tapped    = -1;
    ContentPaints = 0;

    NodeList Rows;
    for (int i = 0; i < 20; ++i) {
        Rows.push_back(
          GestureDetector {.OnTap = [&Tapped, i](const TapEvent&) { Tapped = i; }, .Child = Block(400, 50)});
    }
    Rows.push_back(CustomPaint {.OnPaint = [](Painter&, const Size&) { ++ContentPaints; }, .Height = 1.0f});

    Harness Test(ScrollView {.Controller = &Controller, .Child = Column {.Children = Rows}});
    CHECK(Controller.GetMaxOffset() == doctest::Approx(701.0f));
    const int PaintsBefore = ContentPaints;

    Test.Wheel({100.0f, 100.0f}, -2.0f);
    CHECK(Controller.PeekOffset() == doctest::Approx(96.0f));
    CHECK(ContentPaints == PaintsBefore);

    Test.Wheel({100.0f, 100.0f}, -100.0f);
    CHECK(Controller.PeekOffset() == doctest::Approx(701.0f));
    Test.Wheel({100.0f, 100.0f}, 100.0f);
    CHECK(Controller.PeekOffset() == doctest::Approx(0.0f));

    Controller.SetOffset(300.0f);
    Test.Host.Update();
    Test.Click({100.0f, 225.0f});
    CHECK(Tapped == 10);
}

TEST_CASE("Dragging the scrollbar thumb scrolls proportionally") {
    ScrollController Controller;
    NodeList Rows;
    for (int i = 0; i < 10; ++i) {
        Rows.push_back(Block(400, 60));
    }
    Harness Test(ScrollView {.Controller = &Controller, .Child = Column {.Children = Rows}});

    const float TrackX = 400.0f - RenderScrollable::kScrollbarWidth * 0.5f - 2.0f;
    Test.Drag({TrackX, 20.0f}, {TrackX, 20.0f + 100.0f});
    CHECK(Controller.PeekOffset() > 100.0f);
    CHECK(Controller.PeekOffset() < Controller.GetMaxOffset());
}

TEST_CASE("ListView builds only the items near the viewport") {
    ScrollController Controller;
    LiveRows  = 0;
    BuiltRows = 0;
    Harness Test(ListView {
      .ItemCount   = 10000,
      .ItemExtent  = 20.0f,
      .ItemBuilder = [](int Index) { return Node(CountedRow {Index}); },
      .Controller  = &Controller,
    });

    CHECK(LiveRows <= 15 + RenderVirtualList::kOverscan * 2);
    CHECK(LiveRows >= 15);
    CHECK(Controller.GetMaxOffset() == doctest::Approx(10000.0f * 20.0f - 300.0f));

    Controller.SetOffset(5000.0f * 20.0f);
    Test.Host.Update();
    CHECK(LiveRows <= 15 + RenderVirtualList::kOverscan * 2 + 1);
    const auto& List = static_cast<const RenderVirtualList&>(*Test.Host.GetRootBox());
    CHECK(List.GetFirstBuiltIndex() == 5000 - RenderVirtualList::kOverscan);

    const int BuiltBefore = BuiltRows;
    Controller.ScrollBy(20.0f);
    Test.Host.Update();
    CHECK(BuiltRows - BuiltBefore <= 2);
}

TEST_CASE("Context menus open at the pointer, run the chosen item and close") {
    std::vector<std::string> Log;
    Harness Test(ContextMenu {
      .Items =
        {
          {.Label = "Rename", .OnSelect = [&] { Log.push_back("rename"); }},
          MenuItem::Divider(),
          {.Label = "Delete", .OnSelect = [&] { Log.push_back("delete"); }},
        },
      .Child = GestureDetector {.OnTap = [&](const TapEvent&) { Log.push_back("content"); }, .Child = Block(400, 300)},
    });

    Test.Click({20.0f, 20.0f}, MouseButton::Secondary);
    CHECK(Test.ShowsText("Rename"));

    Test.Click({60.0f, 26.0f});
    CHECK(Log == std::vector<std::string> {"rename"});
    CHECK_FALSE(Test.ShowsText("Rename"));

    Test.Click({20.0f, 20.0f}, MouseButton::Secondary);
    Test.Click({350.0f, 250.0f});
    CHECK_FALSE(Test.ShowsText("Rename"));
    CHECK(Log == std::vector<std::string> {"rename"});

    Test.Click({20.0f, 20.0f}, MouseButton::Secondary);
    CHECK(Test.Press(KeyCode::Down));
    CHECK(Test.Press(KeyCode::Down));
    CHECK(Test.Press(KeyCode::Enter));
    CHECK(Log == std::vector<std::string> {"rename", "delete"});
}

TEST_CASE("Escape closes a menu and returns focus to what had it") {
    FocusNode Owner;
    Harness Test(Focus {
      .Target    = &Owner,
      .AutoFocus = true,
      .Child     = ContextMenu {.Items = {{.Label = "Only"}}, .Child = Block(400, 300)},
    });
    CHECK(Owner.HasFocus());

    Test.Click({20.0f, 20.0f}, MouseButton::Secondary);
    CHECK_FALSE(Owner.HasFocus());
    CHECK(Test.Press(KeyCode::Escape));
    CHECK_FALSE(Test.ShowsText("Only"));
    CHECK(Owner.HasFocus());
}

TEST_CASE("Tooltips appear after the pointer rests and hide when it leaves") {
    FakeClock Clock;
    Harness Test(
      Align {.Position = Alignment::TopLeft(), .Child = Tooltip {.Message = "Hint", .Child = Block(100, 40)}});

    Test.Send({.Phase = PointerPhase::Move, .Position = {50.0f, 20.0f}});
    CHECK_FALSE(Test.ShowsText("Hint"));
    Clock.Advance(1.0);
    Test.Host.Update();
    CHECK(Test.ShowsText("Hint"));

    Test.Send({.Phase = PointerPhase::Move, .Position = {300.0f, 250.0f}});
    CHECK_FALSE(Test.ShowsText("Hint"));
}

TEST_CASE("Buttons activate on click and on Enter while focused") {
    int Presses = 0;
    Harness Test(Center {.Child = Button {.Label = "Go", .OnPressed = [&] { ++Presses; }, .Width = 100.0f}});

    Test.Click({200.0f, 150.0f});
    CHECK(Presses == 1);
    CHECK(Test.Press(KeyCode::Enter));
    CHECK(Presses == 2);

    Test.Click({10.0f, 10.0f});
    CHECK(Presses == 2);
}

TEST_CASE("Disabled buttons ignore input") {
    int Presses = 0;
    Harness Test(Center {.Child = Button {.Label = "No", .OnPressed = [&] { ++Presses; }, .Enabled = false}});
    Test.Click({200.0f, 150.0f});
    CHECK(Presses == 0);
}

TEST_CASE("Checkboxes and switches toggle through their callbacks") {
    Signal<bool> Checked = false;
    struct Bound {
        Signal<bool>* Value = nullptr;
        Node Build(BuildContext& Context) const {
            (void)Context;
            return Align {.Position = Alignment::TopLeft(),
                          .Child    = Column {.CrossAlignment = CrossAxisAlignment::Start,
                                              .Children       = {
                                             Checkbox {.Value     = Value->Get(),
                                                                .OnChanged = [Target = Value](bool Next) { *Target = Next; },
                                                                .Label     = "Check"},
                                             Switch {.Value     = Value->Get(),
                                                              .OnChanged = [Target = Value](bool Next) { *Target = Next; }},
                                           }}};
        }
    };
    Harness Test(Bound {&Checked});

    Test.Click({8.0f, 8.0f});
    CHECK(Checked.Peek());
    Test.Click({8.0f, 8.0f});
    CHECK_FALSE(Checked.Peek());
    Test.Click({10.0f, 24.0f});
    CHECK(Checked.Peek());
}

TEST_CASE("Sliders map pointer position to value and nudge with the wheel") {
    Signal<float> Level = 0.0f;
    Harness Test(
      Align {.Position = Alignment::TopLeft(), .Child = SizedBox {.Width = 214.0f, .Child = Slider {.Bind = &Level}}});

    Test.Click({7.0f + 200.0f * 0.75f, 10.0f});
    CHECK(Level.Peek() == doctest::Approx(0.75f).epsilon(0.01));

    Test.Drag({7.0f + 200.0f * 0.75f, 10.0f}, {7.0f + 200.0f * 0.25f, 10.0f});
    CHECK(Level.Peek() == doctest::Approx(0.25f).epsilon(0.01));

    Test.Wheel({100.0f, 10.0f}, 1.0f);
    CHECK(Level.Peek() == doctest::Approx(0.27f).epsilon(0.01));
}

TEST_CASE("Knobs use relative drags, double-click to reset and clamp to their range") {
    Signal<float> Pan = 0.0f;
    Harness Test(Align {.Position = Alignment::TopLeft(),
                        .Child    = Knob {.Min = -1.0f, .Max = 1.0f, .Bind = &Pan, .Diameter = 40.0f}});

    Test.Send({.Phase    = PointerPhase::Down,
               .Position = {20.0f, 20.0f},
               .Button   = MouseButton::Primary,
               .Buttons  = MouseButton::Primary});
    Test.Send({.Phase = PointerPhase::Move, .Position = {20.0f, 10.0f}, .Buttons = MouseButton::Primary});
    CHECK(Test.Platform.Relative);
    Test.Host.GetInput().HandleRelativeMotion({0.0f, -40.0f}, KeyModifiers::None);
    CHECK(Pan.Peek() == doctest::Approx(0.5f));
    Test.Host.GetInput().HandleRelativeMotion({0.0f, -1000.0f}, KeyModifiers::None);
    CHECK(Pan.Peek() == doctest::Approx(1.0f));
    Test.Send({.Phase = PointerPhase::Up, .Position = {20.0f, 20.0f}, .Button = MouseButton::Primary});
    CHECK_FALSE(Test.Platform.Relative);

    Test.Click({20.0f, 20.0f}, MouseButton::Primary, 10.0);
    Test.Click({20.0f, 20.0f}, MouseButton::Primary, 10.2);
    CHECK(Pan.Peek() == doctest::Approx(0.0f));
}

TEST_CASE("Select shows its options and reports the choice") {
    int Chosen = 0;
    Harness Test(Align {
      .Position = Alignment::TopLeft(),
      .Child = Select {.Options = {"44.1 kHz", "48 kHz", "96 kHz"}, .OnChanged = [&](int Index) { Chosen = Index; }}});

    Test.Click({50.0f, 13.0f});
    CHECK(Test.ShowsText("96 kHz"));
    Test.Click({50.0f, 26.0f + 4.0f + 24.0f * 2.0f + 12.0f});
    CHECK(Chosen == 2);
    CHECK_FALSE(Test.ShowsText("96 kHz"));
}

TEST_CASE("SplitView divides space by ratio and its divider drags") {
    Signal<float> Ratio = 0.5f;
    Harness Test(SplitView {.Ratio     = &Ratio,
                            .MinFirst  = 50.0f,
                            .MinSecond = 50.0f,
                            .First     = Block(10, 10),
                            .Second    = Block(10, 10)});

    RenderBox& Split      = *Test.Host.GetRootBox();
    const float Available = 400.0f - RenderSplit::kDividerThickness;
    CHECK(Split.GetChild(0).GetSize().Width == doctest::Approx(Available * 0.5f));

    const float DividerX = Available * 0.5f + RenderSplit::kDividerThickness * 0.5f;
    Test.Send({.Phase = PointerPhase::Move, .Position = {DividerX, 100.0f}});
    CHECK(Test.Platform.Cursor == CursorShape::ResizeHorizontal);
    Test.Drag({DividerX, 100.0f}, {DividerX + 60.0f, 100.0f});
    CHECK(Ratio.Peek() == doctest::Approx((Available * 0.5f + 60.0f) / Available));
    CHECK(Split.GetChild(0).GetSize().Width == doctest::Approx(Available * 0.5f + 60.0f));

    Ratio = 0.0f;
    Test.Host.Update();
    CHECK(Split.GetChild(0).GetSize().Width == doctest::Approx(50.0f));
}

TEST_CASE("Popup placement flips to stay inside its bounds") {
    const Rect Screen {0.0f, 0.0f, 1000.0f, 800.0f};

    CHECK(ComputePopupPosition({100.0f, 100.0f, 80.0f, 20.0f}, {200.0f, 150.0f}, Screen, PopupPlacement::Below) ==
          Vec2 {100.0f, 120.0f});
    CHECK(ComputePopupPosition({100.0f, 750.0f, 80.0f, 20.0f}, {200.0f, 150.0f}, Screen, PopupPlacement::Below) ==
          Vec2 {100.0f, 600.0f});
    CHECK(ComputePopupPosition({900.0f, 100.0f, 80.0f, 20.0f}, {200.0f, 150.0f}, Screen, PopupPlacement::Below) ==
          Vec2 {800.0f, 120.0f});
    CHECK(ComputePopupPosition({900.0f, 100.0f, 50.0f, 20.0f}, {200.0f, 150.0f}, Screen, PopupPlacement::Right) ==
          Vec2 {700.0f, 100.0f});
    CHECK(ComputePopupPosition({0.0f, 0.0f, 10.0f, 10.0f}, {2000.0f, 150.0f}, Screen, PopupPlacement::Below) ==
          Vec2 {0.0f, 10.0f});

    const Rect SecondMonitor {1920.0f, -200.0f, 1280.0f, 1024.0f};
    CHECK(
      ComputePopupPosition({3100.0f, 700.0f, 40.0f, 20.0f}, {200.0f, 150.0f}, SecondMonitor, PopupPlacement::Below) ==
      Vec2 {3000.0f, 550.0f});
}

TEST_CASE("Menus inherit the theme and text style of the widget that opened them") {
    Theme Custom    = Theme::Dark();
    Custom.Accent   = Color::FromHex(0xFF00FF);
    Custom.BodyFont = {.Family = "Consolas", .Size = 19.0f};
    Harness Test(Provide<Theme> {.Value = Custom,
                                 .Child = ContextMenu {.Items = {{.Label = "Styled"}}, .Child = Block(400, 300)}});

    Test.Click({20.0f, 20.0f}, MouseButton::Secondary);
    const std::function<const RenderParagraph*(const RenderObject&)> Find =
      [&](const RenderObject& Node) -> const RenderParagraph* {
        if (const auto* Paragraph = dynamic_cast<const RenderParagraph*>(&Node);
            Paragraph != nullptr && Paragraph->GetText() == "Styled") {
            return Paragraph;
        }
        for (const RenderObject* Child : Node.GetChildren()) {
            if (const RenderParagraph* Found = Find(*Child)) { return Found; }
        }
        return nullptr;
    };
    const RenderParagraph* Label = Find(*Test.Host.GetRootBox()->GetParent());
    REQUIRE(Label != nullptr);
    CHECK(Label->GetTextLayout().GetMeasuredSize().Height > 20.0f);
}

TEST_CASE("Nested scroll views split wheel axes and size to their content across") {
    ScrollController Vertical;
    ScrollController Horizontal;
    NodeList Lanes;
    for (int i = 0; i < 10; ++i) {
        Lanes.push_back(Block(2000, 60));
    }

    Harness Test(ScrollView {
      .Controller = &Vertical,
      .Child =
        Row {
          .CrossAlignment = CrossAxisAlignment::Start,
          .Children =
            {
              Block(100, 600),
              Expanded {.Child = ScrollView {.Controller = &Horizontal,
                                             .Direction  = Axis::Horizontal,
                                             .Child      = Column {.Children = Lanes}}},
            },
        },
    });

    CHECK(Vertical.GetMaxOffset() == doctest::Approx(300.0f));
    CHECK(Horizontal.GetMaxOffset() == doctest::Approx(1700.0f));

    Test.Wheel({200.0f, 100.0f}, -1.0f);
    CHECK(Vertical.PeekOffset() == doctest::Approx(RenderScrollable::kWheelStep));
    CHECK(Horizontal.PeekOffset() == doctest::Approx(0.0f));

    Test.Send({.Phase      = PointerPhase::Wheel,
               .Position   = {200.0f, 100.0f},
               .WheelDelta = {0.0f, -1.0f},
               .Modifiers  = KeyModifiers::Shift});
    CHECK(Horizontal.PeekOffset() == doctest::Approx(RenderScrollable::kWheelStep));
    CHECK(Vertical.PeekOffset() == doctest::Approx(RenderScrollable::kWheelStep));

    Horizontal.RequestOffset(5000.0f);
    Test.Host.Update();
    CHECK(Horizontal.PeekOffset() == doctest::Approx(1700.0f));
}

TEST_CASE("Scroll views without a scrollbar pass presses on its track through to the content") {
    int Taps = 0;
    Harness Test(
      ScrollView {
        .Direction     = Axis::Horizontal,
        .ShowScrollbar = false,
        .Child         = GestureDetector {.OnTap    = [&Taps](const TapEvent&) { ++Taps; },
                                          .Behavior = HitTestBehavior::Opaque,
                                          .Child    = SizedBox {.Width = 2000.0f, .Height = 40.0f}},
      },
      {400.0f, 40.0f});

    Test.Click({100.0f, 36.0f});
    CHECK(Taps == 1);
}
