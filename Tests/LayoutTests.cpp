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

#include <string>

using namespace XenUI;

namespace {
    struct Hosted {
        explicit Hosted(const Node& Root, Size Viewport = {400.0f, 300.0f}) {
            Host.SetViewportSize(Viewport);
            Host.SetRoot(Root);
            Host.Update();
        }

        [[nodiscard]] RenderBox& Root() const { return *Host.GetRootBox(); }

        WidgetHost Host;
    };

    RenderBox& Descend(RenderBox& From, std::initializer_list<size_t> Path) {
        RenderBox* Current = &From;
        for (const size_t Index : Path) {
            Current = &Current->GetChild(Index);
        }
        return *Current;
    }

    int PaintsA = 0;
    int PaintsB = 0;
}  // namespace

TEST_CASE("The root widget fills the viewport") {
    const Hosted View(SizedBox {});
    CHECK(View.Root().GetSize() == Size {400.0f, 300.0f});
}

TEST_CASE("Padding insets its child") {
    const Hosted View(Align {
      .Position = Alignment::TopLeft(),
      .Child = Padding {.Insets = {10.0f, 20.0f, 30.0f, 40.0f}, .Child = SizedBox {.Width = 50.0f, .Height = 60.0f}}});
    RenderBox& Padded = Descend(View.Root(), {0});
    CHECK(Padded.GetSize() == Size {90.0f, 120.0f});
    CHECK(Padded.GetChild(0).GetOffset() == Vec2 {10.0f, 20.0f});
}

TEST_CASE("Align centers a smaller child") {
    const Hosted View(Center {.Child = SizedBox {.Width = 100.0f, .Height = 50.0f}});
    RenderBox& Child = Descend(View.Root(), {0});
    CHECK(Child.GetSize() == Size {100.0f, 50.0f});
    CHECK(Child.GetOffset() == Vec2 {150.0f, 125.0f});
}

TEST_CASE("Align with size factors wraps its child instead of filling the space") {
    const Hosted View(Align {
      .Position = Alignment::TopLeft(),
      .Child =
        Align {.Child = SizedBox {.Width = 100.0f, .Height = 50.0f}, .WidthFactor = 1.0f, .HeightFactor = 2.0f}});
    RenderBox& Wrapper = Descend(View.Root(), {0});
    CHECK(Wrapper.GetSize() == Size {100.0f, 100.0f});
    CHECK(Wrapper.GetChild(0).GetOffset() == Vec2 {0.0f, 25.0f});
}

TEST_CASE("Expanded children share the free space by flex") {
    const Hosted View(Row {
      .Spacing = 10.0f,
      .Children =
        {
          SizedBox {.Width = 50.0f, .Height = 20.0f},
          Expanded {.Flex = 1, .Child = SizedBox {}},
          Expanded {.Flex = 2, .Child = SizedBox {}},
        },
    });
    RenderBox& Line = View.Root();
    CHECK(Line.GetSize() == Size {400.0f, 300.0f});
    CHECK(Line.GetChild(1).GetSize().Width == doctest::Approx(110.0f));
    CHECK(Line.GetChild(2).GetSize().Width == doctest::Approx(220.0f));
    CHECK(Line.GetChild(1).GetOffset().X == doctest::Approx(60.0f));
    CHECK(Line.GetChild(2).GetOffset().X == doctest::Approx(180.0f));
}

TEST_CASE("Main and cross axis alignment position children") {
    const Hosted View(Row {
      .MainAlignment  = MainAxisAlignment::SpaceBetween,
      .CrossAlignment = CrossAxisAlignment::End,
      .Children       = {SizedBox {.Width = 40.0f, .Height = 10.0f}, SizedBox {.Width = 60.0f, .Height = 30.0f}},
    });
    RenderBox& Line = View.Root();
    CHECK(Line.GetChild(0).GetOffset() == Vec2 {0.0f, 290.0f});
    CHECK(Line.GetChild(1).GetOffset() == Vec2 {340.0f, 270.0f});
}

TEST_CASE("Stretch fills the cross axis and Min main size shrink-wraps") {
    const Hosted View(Align {.Child = Column {
                               .CrossAlignment = CrossAxisAlignment::Stretch,
                               .MainSize       = MainAxisSize::Min,
                               .Children       = {SizedBox {.Height = 25.0f}, SizedBox {.Height = 35.0f}},
                             }});
    RenderBox& Stacked = Descend(View.Root(), {0});
    CHECK(Stacked.GetSize() == Size {400.0f, 60.0f});
    CHECK(Stacked.GetChild(1).GetSize() == Size {400.0f, 35.0f});
    CHECK(Stacked.GetOffset().Y == doctest::Approx(120.0f));
}

TEST_CASE("Stack aligns unpositioned children and places positioned ones by edge") {
    const Hosted View(Stack {
      .DefaultAlignment = Alignment::Center(),
      .Fit              = StackFit::Expand,
      .Children =
        {
          SizedBox {},
          Positioned {.Right = 10.0f, .Bottom = 20.0f, .Child = SizedBox {.Width = 30.0f, .Height = 40.0f}},
          Positioned {.Left = 5.0f, .Top = 5.0f, .Right = 5.0f, .Height = 12.0f, .Child = SizedBox {}},
        },
    });
    RenderBox& Layered = View.Root();
    CHECK(Layered.GetChild(1).GetOffset() == Vec2 {360.0f, 240.0f});
    CHECK(Layered.GetChild(2).GetSize() == Size {390.0f, 12.0f});
    CHECK(Layered.GetChild(2).GetOffset() == Vec2 {5.0f, 5.0f});
}

TEST_CASE("Container composes margin, size, decoration and padding") {
    const Hosted View(Align {.Position = Alignment::TopLeft(),
                             .Child    = Container {
                                  .Width      = 100.0f,
                                  .Height     = 80.0f,
                                  .Margin     = EdgeInsets::All(8.0f),
                                  .Padding    = EdgeInsets::All(4.0f),
                                  .Background = Color::FromHex(0x202020),
                                  .Child      = SizedBox {},
                             }});
    RenderBox& Outer = Descend(View.Root(), {0});
    CHECK(Outer.GetSize() == Size {116.0f, 96.0f});
    RenderBox& Sized = Descend(Outer, {0});
    CHECK(Sized.GetOffset() == Vec2 {8.0f, 8.0f});
    CHECK(Sized.GetSize() == Size {100.0f, 80.0f});
}

TEST_CASE("Changing a signal re-lays out only what depends on it") {
    struct Bar {
        const Signal<float>* Width = nullptr;
        Node Build(BuildContext& Context) const {
            (void)Context;
            return SizedBox {.Width = Width->Get(), .Height = 10.0f};
        }
    };

    Signal<float> Width = 50.0f;
    bool Requested      = false;
    Hosted View(Align {.Position = Alignment::TopLeft(),
                       .Child    = Row {.MainSize = MainAxisSize::Min,
                                        .Children = {Bar {&Width}, SizedBox {.Width = 20.0f, .Height = 10.0f}}}});
    View.Host.SetOnUpdateRequested([&] { Requested = true; });

    RenderBox& Line = Descend(View.Root(), {0});
    CHECK(Line.GetSize().Width == doctest::Approx(70.0f));

    Width = 80.0f;
    CHECK(Requested);
    CHECK(View.Host.NeedsUpdate());
    View.Host.Update();
    CHECK_FALSE(View.Host.NeedsUpdate());
    CHECK(Line.GetSize().Width == doctest::Approx(100.0f));
    CHECK(Line.GetChild(1).GetOffset().X == doctest::Approx(80.0f));
}

TEST_CASE("Viewport changes re-lay out the tree") {
    Hosted View(Row {.Children = {Expanded {.Child = SizedBox {}}}});
    View.Host.SetViewportSize({200.0f, 100.0f});
    View.Host.Update();
    CHECK(View.Root().GetChild(0).GetSize() == Size {200.0f, 0.0f});
}

TEST_CASE("Repaint boundaries record into their own layers and repaint independently") {
    Signal<int> TickA = 0;
    Signal<int> TickB = 0;
    PaintsA           = 0;
    PaintsB           = 0;

    const auto PaintA = [&TickA](Painter& Canvas, const Size& Extent) {
        ++PaintsA;
        Canvas.FillRect({0.0f, 0.0f, Extent.Width, Extent.Height * static_cast<float>(TickA() % 2 + 1) * 0.5f},
                        Color::FromHex(0xFF0000));
    };
    const auto PaintB = [&TickB](Painter& Canvas, const Size& Extent) {
        ++PaintsB;
        (void)TickB();
        Canvas.FillRect({0.0f, 0.0f, Extent.Width, Extent.Height}, Color::FromHex(0x00FF00));
    };

    Hosted View(Row {.Children = {
                       Expanded {.Child = RepaintBoundary {.Child = CustomPaint {.OnPaint = PaintA}}},
                       Expanded {.Child = RepaintBoundary {.Child = CustomPaint {.OnPaint = PaintB}}},
                     }});
    CHECK(PaintsA == 1);
    CHECK(PaintsB == 1);

    const std::shared_ptr<Layer>& Root = View.Host.GetRootLayer();
    REQUIRE(Root->GetChildren().size() == 2);
    CHECK(Root->GetChildren()[1]->GetOffset() == Vec2 {200.0f, 0.0f});

    TickA = 1;
    View.Host.Update();
    CHECK(PaintsA == 2);
    CHECK(PaintsB == 1);

    TickB = 1;
    View.Host.Update();
    CHECK(PaintsA == 2);
    CHECK(PaintsB == 2);
}

TEST_CASE("Content after a repaint boundary keeps its paint order and offset") {
    const auto Fill = [](Painter& Canvas, const Size& Extent) {
        Canvas.FillRect({0.0f, 0.0f, Extent.Width, Extent.Height}, Color::FromHex(0x808080));
    };
    Hosted View(
      Column {.Children = {
                SizedBox {.Height = 50.0f, .Child = CustomPaint {.OnPaint = Fill}},
                SizedBox {.Height = 50.0f, .Child = RepaintBoundary {.Child = CustomPaint {.OnPaint = Fill}}},
                Padding {.Insets = {.Left = 7.0f},
                         .Child  = SizedBox {.Width = 10.0f, .Height = 50.0f, .Child = CustomPaint {.OnPaint = Fill}}},
              }});

    const auto& Layers = View.Host.GetRootLayer()->GetChildren();
    REQUIRE(Layers.size() == 3);
    CHECK(Layers[1]->GetOffset() == Vec2 {0.0f, 50.0f});
    const Rect After = Layers[2]->GetContent().GetBounds();
    CHECK(After.X == doctest::Approx((400.0f - 17.0f) * 0.5f + 7.0f - 1.0f));
    CHECK(After.Y == doctest::Approx(100.0f - 1.0f));
}

TEST_CASE("Text measures, wraps and inherits its style") {
    (void)Testing::SharedApp();

    Hosted Wide(Align {.Position = Alignment::TopLeft(), .Child = Text {.Content = "Hello"}});
    const Size Measured = Descend(Wide.Root(), {0}).GetSize();
    CHECK(Measured.Width > 10.0f);
    CHECK(Measured.Height > 5.0f);

    Hosted Narrow(Align {.Position = Alignment::TopLeft(),
                         .Child    = SizedBox {.Width = 60.0f, .Child = Text {.Content = "several words that wrap"}}},
                  {400.0f, 300.0f});
    const auto& Paragraph = static_cast<RenderParagraph&>(Descend(Narrow.Root(), {0, 0}));
    CHECK(Paragraph.GetTextLayout().GetLineCount() > 1);
    CHECK(Paragraph.GetSize().Width == doctest::Approx(60.0f));

    Hosted Styled(
      Provide<TextStyle> {.Value = {.Font = {.Size = 30.0f}},
                          .Child = Align {.Position = Alignment::TopLeft(), .Child = Text {.Content = "Hello"}}});
    CHECK(Descend(Styled.Root(), {0}).GetSize().Height > Measured.Height * 1.5f);
}
