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

#include <optional>
#include <string>
#include <vector>

using namespace XenUI;
using namespace XenUI::Testing;

namespace {
    class DragHost final : public InputHost {
    public:
        void SetCursor(CursorShape Shape) override { Cursor = Shape; }
        void BeginRelativePointer() override {}
        void EndRelativePointer() override {}

        std::optional<DropEffect> PerformSystemDrag(const DragData& Data, DropEffect Allowed) override {
            ++SystemDrags;
            SystemText    = Data.Text.value_or("");
            SystemAllowed = Allowed;
            return SystemResult;
        }

        CursorShape Cursor = CursorShape::Arrow;
        int SystemDrags    = 0;
        std::string SystemText;
        DropEffect SystemAllowed = DropEffect::None;
        DropEffect SystemResult  = DropEffect::Copy;
    };

    struct Record {
        int Overs  = 0;
        int Leaves = 0;
        int Drops  = 0;
        std::optional<int> Dropped;
        std::vector<std::filesystem::path> DroppedFiles;
        Vec2 DropPosition;
        std::optional<DropEffect> Ended;
    };

    struct DragHarness {
        explicit DragHarness(const Node& Root) {
            (void)SharedApp();
            Host.SetViewportSize({400.0f, 300.0f});
            Host.SetRoot(Root);
            Host.GetInput().SetHost(&Platform);
            Host.Update();
        }

        void Send(const PointerEvent& Event) {
            Host.GetInput().HandlePointer(Event);
            Host.Update();
        }

        void Down(Vec2 Position) {
            Send({.Phase    = PointerPhase::Down,
                  .Position = Position,
                  .Button   = MouseButton::Primary,
                  .Buttons  = MouseButton::Primary});
        }
        void Move(Vec2 Position) {
            Send({.Phase = PointerPhase::Move, .Position = Position, .Buttons = MouseButton::Primary});
        }
        void Up(Vec2 Position) {
            Send({.Phase = PointerPhase::Up, .Position = Position, .Button = MouseButton::Primary});
        }

        void Drag(Vec2 From, Vec2 To) {
            Down(From);
            Move({From.X + 10.0f, From.Y});
            Move(To);
        }

        WidgetHost Host;
        DragHost Platform;
    };

    Node Scene(Record& Log, Node Source, bool AcceptInts = true) {
        return Stack {
          .Fit = StackFit::Expand,
          .Children =
            {
              Positioned {.Left = 0.0f, .Top = 0.0f, .Width = 100.0f, .Height = 100.0f, .Child = std::move(Source)},
              Positioned {
                .Left   = 200.0f,
                .Top    = 0.0f,
                .Width  = 100.0f,
                .Height = 100.0f,
                .Child  = DropTarget {
                   .OnOver =
                    [&Log, AcceptInts](const DropEvent& Event) {
                        ++Log.Overs;
                        const bool Wanted =
                          (AcceptInts && Event.Data->Get<int>() != nullptr) || !Event.Data->Files.empty();
                        return Wanted ? DropEffect::Copy : DropEffect::None;
                    },
                   .OnLeave = [&Log] { ++Log.Leaves; },
                   .OnDrop =
                    [&Log](const DropEvent& Event) {
                        ++Log.Drops;
                        if (const int* Value = Event.Data->Get<int>()) { Log.Dropped = *Value; }
                        Log.DroppedFiles = Event.Data->Files;
                        Log.DropPosition = Event.LocalPosition;
                    },
                   .Behavior = HitTestBehavior::Opaque,
                   .Child    = SizedBox {},
                },
              },
            },
        };
    }

    Node IntSource(Record& Log, std::optional<std::string> Text = std::nullopt) {
        return DragSource {
          .Data      = [Text]() -> std::optional<DragData> { return DragData {.Value = 42, .Text = Text}; },
          .OnDragEnd = [&Log](DropEffect Result) { Log.Ended = Result; },
          .Behavior  = HitTestBehavior::Opaque,
          .Child     = SizedBox {},
        };
    }
}  // namespace

TEST_CASE("Data drags reach drop targets and report the result to the source") {
    Record Log;
    DragHarness Test(Scene(Log, IntSource(Log)));

    Test.Drag({50.0f, 50.0f}, {250.0f, 40.0f});
    CHECK(Test.Host.GetInput().IsDataDragActive());
    CHECK(Log.Overs >= 1);
    CHECK(Test.Platform.Cursor == CursorShape::Arrow);

    Test.Up({250.0f, 40.0f});
    CHECK_FALSE(Test.Host.GetInput().IsDataDragActive());
    CHECK(Log.Drops == 1);
    CHECK(Log.Dropped == 42);
    CHECK(Log.DropPosition == Vec2 {50.0f, 40.0f});
    CHECK(Log.Ended == DropEffect::Copy);
    CHECK(Log.Leaves == 1);
}

TEST_CASE("Targets that refuse data show a not-allowed cursor and receive nothing") {
    Record Log;
    DragHarness Test(Scene(Log, IntSource(Log), false));

    Test.Drag({50.0f, 50.0f}, {250.0f, 40.0f});
    CHECK(Test.Platform.Cursor == CursorShape::NotAllowed);
    Test.Up({250.0f, 40.0f});
    CHECK(Log.Drops == 0);
    CHECK(Log.Ended == DropEffect::None);
}

TEST_CASE("Escape cancels a data drag") {
    Record Log;
    DragHarness Test(Scene(Log, IntSource(Log)));

    Test.Drag({50.0f, 50.0f}, {250.0f, 40.0f});
    CHECK(Test.Host.GetInput().HandleKey({.Code = KeyCode::Escape}));
    CHECK(Log.Ended == DropEffect::None);
    CHECK(Log.Leaves == 1);
    Test.Up({250.0f, 40.0f});
    CHECK(Log.Drops == 0);
}

TEST_CASE("The deepest drag handler wins between detectors and drag sources") {
    Record Log;
    int DetectorDrags = 0;
    DragHarness Test(
      Scene(Log,
            DragSource {
              .Data      = []() -> std::optional<DragData> { return DragData {.Value = 7}; },
              .OnDragEnd = [&Log](DropEffect Result) { Log.Ended = Result; },
              .Behavior  = HitTestBehavior::Opaque,
              .Child     = GestureDetector {.OnDragStart = [&DetectorDrags](const DragEvent&) { ++DetectorDrags; },
                                            .Behavior    = HitTestBehavior::Opaque,
                                            .Child       = SizedBox {}},
            }));
    Test.Drag({50.0f, 50.0f}, {250.0f, 40.0f});
    Test.Up({250.0f, 40.0f});
    CHECK(DetectorDrags == 1);
    CHECK(Log.Drops == 0);
    CHECK_FALSE(Log.Ended.has_value());

    Record Inner;
    DragHarness Nested(Scene(Inner,
                             GestureDetector {.OnDragStart = [&DetectorDrags](const DragEvent&) { ++DetectorDrags; },
                                              .Behavior    = HitTestBehavior::Opaque,
                                              .Child       = IntSource(Inner)}));
    Nested.Drag({50.0f, 50.0f}, {250.0f, 40.0f});
    Nested.Up({250.0f, 40.0f});
    CHECK(DetectorDrags == 1);
    CHECK(Inner.Dropped == 42);
}

TEST_CASE("Drags from other applications are routed to drop targets") {
    Record Log;
    DragHarness Test(Scene(Log, IntSource(Log)));
    InputDispatcher& Input = Test.Host.GetInput();
    const DragData Files {.Files = {"C:/Samples/Kick.wav"}, .External = true};

    CHECK(Input.DragOverExternal({20.0f, 20.0f}, Files, KeyModifiers::None, DropEffect::Copy | DropEffect::Move) ==
          DropEffect::None);
    CHECK(Input.DragOverExternal({220.0f, 20.0f}, Files, KeyModifiers::None, DropEffect::Copy | DropEffect::Move) ==
          DropEffect::Copy);
    Input.DragLeaveExternal();
    CHECK(Log.Leaves == 1);

    CHECK(Input.DropExternal({230.0f, 30.0f}, Files, KeyModifiers::None, DropEffect::Copy) == DropEffect::Copy);
    REQUIRE(Log.DroppedFiles.size() == 1);
    CHECK(Log.DroppedFiles.front() == std::filesystem::path("C:/Samples/Kick.wav"));
    CHECK(Log.DropPosition == Vec2 {30.0f, 30.0f});
}

TEST_CASE("Dragging exportable data out of the window continues as a system drag") {
    Record Log;
    DragHarness Test(Scene(Log, IntSource(Log, "forty-two")));

    Test.Drag({50.0f, 50.0f}, {450.0f, 50.0f});
    CHECK(Test.Platform.SystemDrags == 1);
    CHECK(Test.Platform.SystemText == "forty-two");
    CHECK(Test.Platform.SystemAllowed == DropEffect::Copy);
    CHECK(Log.Ended == DropEffect::Copy);
    CHECK_FALSE(Test.Host.GetInput().IsDataDragActive());

    Record Local;
    DragHarness InWindow(Scene(Local, IntSource(Local)));
    InWindow.Drag({50.0f, 50.0f}, {450.0f, 50.0f});
    CHECK(InWindow.Platform.SystemDrags == 0);
    CHECK(InWindow.Host.GetInput().IsDataDragActive());
    InWindow.Up({450.0f, 50.0f});
    CHECK(Local.Ended == DropEffect::None);
}
