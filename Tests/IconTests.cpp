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
#include <variant>
#include <vector>

using namespace XenUI;
using namespace XenUI::Testing;

namespace {
    Rect BoundsOf(std::string_view Data) {
        (void)SharedApp();
        return PathBuilder::FromSvgPathData(Data).Build().GetBounds();
    }

    void CheckRect(const Rect& Actual, const Rect& Expected) {
        CHECK(Actual.X == doctest::Approx(Expected.X).epsilon(0.01));
        CHECK(Actual.Y == doctest::Approx(Expected.Y).epsilon(0.01));
        CHECK(Actual.Width == doctest::Approx(Expected.Width).epsilon(0.01));
        CHECK(Actual.Height == doctest::Approx(Expected.Height).epsilon(0.01));
    }

    /// @brief What drawing an icon recorded, one entry per painted shape.
    struct DrawnShape {
        Matrix3x2 Transform;
        bool Filled  = false;
        bool Stroked = false;
        Color Paint;
        StrokeStyle Stroke;
    };

    std::vector<DrawnShape>
    Draw(const SvgIcon& Source, const Rect& Bounds, const Color& Tint = Color::FromHex(0xFFFFFF)) {
        (void)SharedApp();
        DisplayList List;
        {
            Painter Canvas(List);
            Source.Draw(Canvas, Bounds, Tint);
        }
        std::vector<DrawnShape> Shapes;
        for (const DrawCommand& Command : List.GetCommands()) {
            if (const auto* Push = std::get_if<PushTransformCommand>(&Command)) {
                Shapes.push_back({.Transform = Push->Transform});
            } else if (const auto* Fill = std::get_if<FillPathCommand>(&Command)) {
                Shapes.back().Filled = true;
                Shapes.back().Paint  = std::get<Color>(Fill->Paint);
            } else if (const auto* Stroke = std::get_if<StrokePathCommand>(&Command)) {
                Shapes.back().Stroked = true;
                Shapes.back().Paint   = std::get<Color>(Stroke->Paint);
                Shapes.back().Stroke  = Stroke->Stroke;
            }
        }
        return Shapes;
    }

    struct Mounted {
        explicit Mounted(const Node& Root) {
            (void)SharedApp();
            Host.SetViewportSize({300.0f, 200.0f});
            Host.SetRoot(Align {.Position = Alignment::TopLeft(), .Child = Root});
            Host.Update();
            Inspector.Refresh();
        }

        const InspectorNode* WithSemantics(std::string_view Description) const {
            for (const InspectorNode& Entry : Inspector.GetNodes()) {
                if (Entry.Semantics == Description) { return &Entry; }
            }
            return nullptr;
        }

        WidgetHost Host;
        WidgetInspector Inspector {Host};
    };

    constexpr std::string_view kSearchIcon =
      R"(<?xml version="1.0"?>
<!-- A stroked icon in the style of Lucide: the paint is set once on the root. -->
<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none"
     stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
  <circle cx="11" cy="11" r="8"/>
  <path d="m21 21-4.3-4.3"/>
</svg>)";
}  // namespace

TEST_CASE("SVG path data reads absolute, relative and implicit commands") {
    CheckRect(BoundsOf("M2 2h4v4H2z"), {2.0f, 2.0f, 4.0f, 4.0f});
    CheckRect(BoundsOf("m1 1 2 0 0 2"), {1.0f, 1.0f, 2.0f, 2.0f});
    CheckRect(BoundsOf("M0,0L10-5.5.5 3"), {0.0f, -5.5f, 10.0f, 8.5f});
    CheckRect(BoundsOf("M10 10h2v2zm-10 0h1"), {0.0f, 10.0f, 12.0f, 2.0f});
    CheckRect(BoundsOf("M0 0C0 10 10 10 10 0S20-10 20 0"), {0.0f, -7.5f, 20.0f, 15.0f});
}

TEST_CASE("SVG path data reads arcs, including run-together flags and radii that are too small") {
    CheckRect(BoundsOf("M0 0a2 2 0 104 0"), {0.0f, 0.0f, 4.0f, 2.0f});
    CheckRect(BoundsOf("M0 0a2 2 0 1 1 4 0"), {0.0f, -2.0f, 4.0f, 2.0f});
    CheckRect(BoundsOf("M0 0A1 1 0 0 1 4 0"), {0.0f, -2.0f, 4.0f, 2.0f});
    CheckRect(BoundsOf("M0 0A0 3 0 0 1 4 0"), {0.0f, 0.0f, 4.0f, 0.0f});
}

TEST_CASE("Malformed SVG path data is reported") {
    CHECK_THROWS_AS((void)PathBuilder::FromSvgPathData("M0 0 L 5"), Error);
    CHECK_THROWS_AS((void)PathBuilder::FromSvgPathData("10 10"), Error);
    CHECK_THROWS_AS((void)PathBuilder::FromSvgPathData("M0 0X1 2"), Error);
    CHECK_THROWS_AS((void)PathBuilder::FromSvgPathData("M0 0Z 3 4"), Error);
    CHECK_THROWS_AS((void)PathBuilder::FromSvgPathData("M0 0A1 1 0 2 0 3 3"), Error);
    CHECK(PathBuilder::FromSvgPathData("  ").Build().IsEmpty());
}

TEST_CASE("Stroked SVG icons inherit their paint from the root and scale to fit") {
    const SvgIcon Search = SvgIcon::Parse(kSearchIcon);
    CHECK_FALSE(Search.IsEmpty());
    CheckRect(Search.GetViewBox(), {0.0f, 0.0f, 24.0f, 24.0f});

    const Color Tint                     = Color::FromHex(0x4DA3FF);
    const std::vector<DrawnShape> Shapes = Draw(Search, {10.0f, 20.0f, 48.0f, 48.0f}, Tint);
    REQUIRE(Shapes.size() == 2);
    for (const DrawnShape& Shape : Shapes) {
        CHECK_FALSE(Shape.Filled);
        CHECK(Shape.Stroked);
        CHECK(Shape.Paint == Tint);
        CHECK(Shape.Stroke.Width == doctest::Approx(2.0f));
        CHECK(Shape.Stroke.Cap == LineCap::Round);
        CHECK(Shape.Stroke.Join == LineJoin::Round);
        CHECK(Shape.Transform.M11 == doctest::Approx(2.0f));
        CHECK(Shape.Transform.Dx == doctest::Approx(10.0f));
        CHECK(Shape.Transform.Dy == doctest::Approx(20.0f));
    }
}

TEST_CASE("SVG icons apply groups, transforms, opacity and styles, and skip definitions") {
    const SvgIcon Mixed = SvgIcon::Parse(R"svg(<svg viewBox="0 0 24 12">
      <defs><path d="M0 0h24v12H0z"/><linearGradient id="g"><stop offset="0"/></linearGradient></defs>
      <title>Mixed</title>
      <g transform="translate(2 3)" opacity="0.5">
        <path fill-rule="evenodd" d="M0 0h10v10H0z"/>
      </g>
      <rect x="1" y="1" width="4" height="4" rx="1" style="fill: none; stroke: #000; stroke-width: 3"/>
      <line x1="0" y1="0" x2="5" y2="5" stroke="red"/>
      <polygon points="0,0 4,0 4,4" fill="none"/>
    </svg>)svg");

    const std::vector<DrawnShape> Shapes = Draw(Mixed, {0.0f, 0.0f, 24.0f, 24.0f});
    REQUIRE(Shapes.size() == 3);

    CHECK(Shapes[0].Filled);
    CHECK_FALSE(Shapes[0].Stroked);
    CHECK(Shapes[0].Paint.A == doctest::Approx(0.5f));
    CHECK(Shapes[0].Transform.Dx == doctest::Approx(2.0f));
    CHECK(Shapes[0].Transform.Dy == doctest::Approx(3.0f + 6.0f));

    CHECK_FALSE(Shapes[1].Filled);
    CHECK(Shapes[1].Stroked);
    CHECK(Shapes[1].Stroke.Width == doctest::Approx(3.0f));
    CHECK(Shapes[1].Transform.Dy == doctest::Approx(6.0f));

    CHECK_FALSE(Shapes[2].Filled);
    CHECK(Shapes[2].Stroked);
}

TEST_CASE("SVG icons can be made from path data, and documents that aren't SVG are reported") {
    const SvgIcon Play                   = SvgIcon::FromPathData("M8 5v14l11-7z", {24.0f, 24.0f});
    const std::vector<DrawnShape> Shapes = Draw(Play, {0.0f, 0.0f, 12.0f, 12.0f});
    REQUIRE(Shapes.size() == 1);
    CHECK(Shapes[0].Filled);
    CHECK(Shapes[0].Transform.M11 == doctest::Approx(0.5f));

    CHECK(SvgIcon().IsEmpty());
    CHECK_THROWS_AS((void)SvgIcon::Parse("<html><body/></html>"), Error);
    CHECK_THROWS_AS((void)SvgIcon::Parse("just text"), Error);
    CHECK_THROWS_AS((void)SvgIcon::Parse(R"(<svg><path d="M0 0 L"/></svg>)"), Error);
    CHECK_THROWS_AS((void)SvgIcon::FromFile("C:/No/Such/Icon.svg"), Error);
    CHECK_THROWS_AS((void)SvgIcon::FromResource(4242), Error);
}

TEST_CASE("Icons are square, and only labeled icons are described to screen readers") {
    const SvgIcon Play = SvgIcon::FromPathData("M8 5v14l11-7z");

    const Mounted Labeled(Row {.Children = {Icon {.Source = Play, .Size = 20.0f, .Label = "Playing"}}});
    const InspectorNode* Described = Labeled.WithSemantics("Image \"Playing\"");
    REQUIRE(Described != nullptr);
    REQUIRE(Described->Bounds.has_value());
    CHECK(Described->Bounds->Width == doctest::Approx(20.0f));
    CHECK(Described->Bounds->Height == doctest::Approx(20.0f));

    const Mounted Decorative(Row {.Children = {Icon {.Source = Play}}});
    for (const InspectorNode& Entry : Decorative.Inspector.GetNodes()) {
        CHECK(Entry.Semantics.find("Image") == std::string::npos);
    }
}

TEST_CASE("Buttons show an icon beside their label, or alone as a square button") {
    const SvgIcon Play = SvgIcon::FromPathData("M8 5v14l11-7z");

    const Mounted IconOnly(Button {.Icon = Play, .AccessibleLabel = "Play"});
    const InspectorNode* Square = IconOnly.WithSemantics("Button \"Play\"");
    REQUIRE(Square != nullptr);
    REQUIRE(Square->Bounds.has_value());
    CHECK(Square->Bounds->Width == doctest::Approx(Square->Bounds->Height));

    const Mounted Labeled(Row {.Children = {Button {.Label = "Play"}, Button {.Label = "Play", .Icon = Play}}});
    std::vector<float> Widths;
    for (const InspectorNode& Entry : Labeled.Inspector.GetNodes()) {
        if (Entry.Semantics == "Button \"Play\"" && Entry.Bounds) { Widths.push_back(Entry.Bounds->Width); }
    }
    REQUIRE(Widths.size() == 2);
    CHECK(Widths[1] == doctest::Approx(Widths[0] + 16.0f + 6.0f).epsilon(0.02));
}
