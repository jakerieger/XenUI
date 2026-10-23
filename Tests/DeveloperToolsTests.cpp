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

using namespace XenUI;
using namespace XenUI::Testing;

namespace {
    struct Counted {
        Signal<int>* Value = nullptr;

        Node Build(BuildContext& Context) const {
            (void)Context;
            return Column {.Children = {Text {.Content = std::to_string(Value->Get())}, Button {.Label = "Go"}}};
        }
    };

    bool ShowsTextStartingWith(const RenderObject& Root, const std::string& Prefix) {
        const std::function<bool(const RenderObject&)> Search = [&](const RenderObject& Node) {
            if (const auto* Paragraph = dynamic_cast<const RenderParagraph*>(&Node);
                Paragraph != nullptr && Paragraph->GetText().starts_with(Prefix)) {
                return true;
            }
            for (const RenderObject* Child : Node.GetChildren()) {
                if (Search(*Child)) { return true; }
            }
            return false;
        };
        return Search(Root);
    }
}  // namespace

TEST_CASE("Frame statistics keep the most recent frames in order") {
    FrameStatistics Stats;
    for (int i = 0; i < 300; ++i) {
        Stats.Record({.Time = static_cast<double>(i) * 0.1, .BuildMs = static_cast<float>(i)});
    }
    CHECK(Stats.GetTotalFrames() == 300);
    const std::vector<FrameSample> Last = Stats.GetRecent(3);
    REQUIRE(Last.size() == 3);
    CHECK(Last[0].BuildMs == doctest::Approx(297.0f));
    CHECK(Last[2].BuildMs == doctest::Approx(299.0f));
    CHECK(Stats.GetRecent().size() == FrameStatistics::kCapacity);
    CHECK(Stats.CountFramesSince(28.95) == 10);
}

TEST_CASE("Widget hosts time each update and count the work it did") {
    (void)SharedApp();
    Signal<int> Value = 1;
    WidgetHost Host;
    Host.SetViewportSize({300.0f, 200.0f});
    Host.SetRoot(Counted {&Value});
    Host.Update();

    const UpdateTimings First = Host.GetLastUpdate();
    CHECK(First.Rebuilt == 0);
    CHECK(First.LaidOut > 3);
    CHECK(First.Repainted >= 1);
    CHECK(Host.GetFrameStatistics().GetTotalFrames() == 1);

    Value = 2;
    Host.Update();
    const UpdateTimings Second = Host.GetLastUpdate();
    CHECK(Second.Rebuilt >= 1);
    CHECK(Second.LaidOut >= 1);
    CHECK(Second.LaidOut < First.LaidOut);
    CHECK(Second.Repainted >= 1);
    CHECK(Host.GetFrameStatistics().GetTotalFrames() == 2);

    Host.Update();
    const UpdateTimings Idle = Host.GetLastUpdate();
    CHECK(Idle.Rebuilt == 0);
    CHECK(Idle.LaidOut == 0);
    CHECK(Idle.Repainted == 0);
}

TEST_CASE("The widget inspector snapshots the element tree and finds elements under a point") {
    (void)SharedApp();
    Signal<int> Value = 7;
    WidgetHost Host;
    Host.SetViewportSize({300.0f, 200.0f});
    Host.SetRoot(Align {.Position = Alignment::TopLeft(), .Child = Counted {&Value}});
    Host.Update();

    CHECK(WidgetInspector::CleanTypeName("struct XenUI::Provide<struct XenUI::Theme>") == "Provide<Theme>");

    WidgetInspector Inspector(Host);
    Inspector.Refresh();
    const auto& Nodes = Inspector.GetNodes();
    REQUIRE_FALSE(Nodes.empty());
    CHECK(Nodes[0].Name == "Provide<DeveloperHandle>");
    CHECK(Nodes[0].Id == "0");

    const auto Named = [&Nodes](const std::string& Name) -> const InspectorNode* {
        for (const InspectorNode& Entry : Nodes) {
            if (Entry.Name == Name) { return &Entry; }
        }
        return nullptr;
    };
    const InspectorNode* Counter =
      Named("`anonymous namespace'::Counted") != nullptr ? Named("`anonymous namespace'::Counted") : Named("Counted");
    REQUIRE(Counter != nullptr);
    CHECK(Counter->Kind == "Component");
    const InspectorNode* Go = Named("Button");
    REQUIRE(Go != nullptr);
    REQUIRE(Go->Bounds.has_value());

    const InspectorNode* Paragraph = nullptr;
    for (const InspectorNode& Entry : Nodes) {
        if (Entry.RenderName == "RenderParagraph" && Entry.Semantics == "Text \"7\"") { Paragraph = &Entry; }
    }
    REQUIRE(Paragraph != nullptr);
    CHECK(Paragraph->Kind == "Render");
    REQUIRE(Paragraph->Constraints.has_value());
    CHECK(Paragraph->Parent >= 0);
    CHECK(Inspector.Find(Paragraph->Id) == Paragraph);

    const std::string Hit = Inspector.HitTest(Go->Bounds->Center());
    REQUIRE_FALSE(Hit.empty());
    CHECK(Hit.starts_with(Go->Id + "/"));

    const uint64_t Before = Inspector.GetRevision();
    Inspector.Refresh();
    CHECK(Inspector.GetRevision() == Before + 1);
    CHECK(Inspector.Find(Go->Id) != nullptr);
}

TEST_CASE("Developer tools toggle the frame stats overlay with Shift+F12") {
    (void)SharedApp();
    WidgetHost Host;
    Host.SetViewportSize({600.0f, 400.0f});
    Host.SetRoot(DeveloperTools {.Child = Center {.Child = Text {.Content = "App"}}});
    Host.Update();
    CHECK_FALSE(ShowsTextStartingWith(Host.GetView(), "bars:"));

    const auto Settle = [&Host] {
        TimerQueue::Get().RunDue();
        Host.Update();
        TimerQueue::Get().RunDue();
        Host.Update();
    };

    CHECK(Host.GetInput().HandleKey({.Code = KeyCode::F12, .Modifiers = KeyModifiers::Shift}));
    Settle();
    CHECK(ShowsTextStartingWith(Host.GetView(), "bars:"));

    CHECK(Host.GetInput().HandleKey({.Code = KeyCode::F12, .Modifiers = KeyModifiers::Shift}));
    Settle();
    CHECK_FALSE(ShowsTextStartingWith(Host.GetView(), "bars:"));

    CHECK_FALSE(Host.GetInput().HandleKey({.Code = KeyCode::F11}));
}
