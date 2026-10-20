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
using namespace XenUI::Testing;

namespace {
    struct Model {
        Signal<int> Presses       = 0;
        Signal<bool> Looping      = false;
        Signal<float> Volume      = 0.5f;
        Signal<std::string> Name  = std::string("Ada");
        Signal<int> Quality       = 0;
        Signal<bool> ShowExtra    = false;
        Signal<int> SelectedTrack = 0;
    };

    struct Panel {
        Model* State = nullptr;

        Node Build(BuildContext& Context) const {
            (void)Context;
            Model* Shared = State;
            NodeList Tracks;
            for (int i = 0; i < 2; ++i) {
                Tracks.push_back(Semantics {
                  .Role     = SemanticsRole::ListItem,
                  .Selected = Shared->SelectedTrack() == i,
                  .OnSelect = [Shared, i] { Shared->SelectedTrack = i; },
                  .Child = Row {.Children = {Text {.Content = i == 0 ? "Kick" : "Snare"}, Text {.Content = "120 BPM"}}},
                });
            }
            NodeList Children {
              Button {.Label = "Play", .OnPressed = [Shared] { Shared->Presses = Shared->Presses.Peek() + 1; }},
              Button {.Label = "Off", .Enabled = false},
              Checkbox {.Value     = Shared->Looping(),
                        .OnChanged = [Shared](bool Value) { Shared->Looping = Value; },
                        .Label     = "Loop"},
              Slider {.Bind = &Shared->Volume, .Label = "Volume"},
              TextField {.Bind = &Shared->Name, .Label = "Name"},
              Select {.Options       = {"Draft", "High"},
                      .SelectedIndex = Shared->Quality(),
                      .OnChanged     = [Shared](int Index) { Shared->Quality = Index; },
                      .Label         = "Quality"},
              Tooltip {.Message = "Record arm", .Child = Button {.Label = "R"}},
              Text {.Content = "Status"},
              ExcludeSemantics {.Child = Text {.Content = "Hidden"}},
              Semantics {.Role  = SemanticsRole::List,
                         .Label = "Tracks",
                         .Child = Column {.Children = std::move(Tracks)}},
            };
            if (Shared->ShowExtra()) { Children.push_back(Button {.Label = "Extra"}); }
            return Column {.Spacing        = 4.0f,
                           .CrossAlignment = CrossAxisAlignment::Start,
                           .Children       = std::move(Children)};
        }
    };

    struct Harness {
        Harness() {
            (void)SharedApp();
            Host.SetViewportSize({400.0f, 600.0f});
            Host.SetRoot(Panel {&State});
            Host.Update();
            Rebuild();
        }

        SemanticsUpdate Rebuild() {
            Host.Update();
            return Tree.Rebuild(Host.GetView(), &Host.GetInput().GetFocusManager());
        }

        [[nodiscard]] const SemanticsNode& Named(std::string_view Label) const {
            const SemanticsNode* Node = Tree.FindByLabel(Label);
            REQUIRE(Node != nullptr);
            return *Node;
        }

        Model State;
        WidgetHost Host;
        SemanticsTree Tree;
    };
}  // namespace

TEST_CASE("Controls describe themselves in the accessibility tree") {
    Harness Test;

    const SemanticsNode& Play = Test.Named("Play");
    CHECK(Play.Properties.Role == SemanticsRole::Button);
    CHECK(Play.Properties.OnInvoke);
    CHECK(Play.Focusable);
    CHECK(Play.Children.empty());
    CHECK(Play.Parent == SemanticsTree::kRootId);

    CHECK_FALSE(Test.Named("Off").Properties.Enabled);

    const SemanticsNode& Loop = Test.Named("Loop");
    CHECK(Loop.Properties.Role == SemanticsRole::CheckBox);
    CHECK(Loop.Properties.Toggled == false);

    const SemanticsNode& Volume = Test.Named("Volume");
    CHECK(Volume.Properties.Role == SemanticsRole::Slider);
    REQUIRE(Volume.Properties.Range.has_value());
    CHECK(Volume.Properties.Range->Value == doctest::Approx(0.5f));

    const SemanticsNode& Name = Test.Named("Name");
    CHECK(Name.Properties.Role == SemanticsRole::Edit);
    CHECK(Name.Properties.Value == "Ada");
    CHECK(Name.Focusable);

    const SemanticsNode& Quality = Test.Named("Quality");
    CHECK(Quality.Properties.Role == SemanticsRole::ComboBox);
    CHECK(Quality.Properties.Value == "Draft");
    CHECK(Quality.Properties.Expanded == false);

    CHECK(Test.Named("Status").Properties.Role == SemanticsRole::Text);
    CHECK(Test.Named("R").Properties.Description == "Record arm");
    CHECK(Test.Tree.FindByLabel("Hidden") == nullptr);

    const SemanticsNode& Tracks = Test.Named("Tracks");
    REQUIRE(Tracks.Children.size() == 2);
    const SemanticsNode& Kick = *Test.Tree.Find(Tracks.Children[0]);
    CHECK(Kick.Properties.Label == "Kick 120 BPM");
    CHECK(Kick.Properties.Selected == true);
    CHECK(Kick.Children.empty());
}

TEST_CASE("Accessibility actions operate the live widgets") {
    Harness Test;

    CHECK(Test.Tree.Invoke(Test.Named("Play").Id));
    CHECK(Test.State.Presses.Peek() == 1);
    CHECK_FALSE(Test.Tree.Invoke(Test.Named("Off").Id));

    CHECK(Test.Tree.Toggle(Test.Named("Loop").Id));
    CHECK(Test.State.Looping.Peek());
    SemanticsUpdate Update = Test.Rebuild();
    CHECK(Test.Named("Loop").Properties.Toggled == true);
    CHECK(std::ranges::any_of(Update.Properties, [](const SemanticsUpdate::PropertyChange& Change) {
        return Change.What == SemanticsUpdate::Change::Toggle;
    }));

    CHECK(Test.Tree.SetRangeValue(Test.Named("Volume").Id, 3.0f));
    CHECK(Test.State.Volume.Peek() == doctest::Approx(1.0f));
    Test.Rebuild();
    CHECK(Test.Named("Volume").Properties.Range->Value == doctest::Approx(1.0f));

    CHECK(Test.Tree.SetValue(Test.Named("Name").Id, "Grace"));
    Test.Rebuild();
    CHECK(Test.State.Name.Peek() == "Grace");
    CHECK(Test.Named("Name").Properties.Value == "Grace");

    const SemanticsNode& Tracks = Test.Named("Tracks");
    CHECK(Test.Tree.Select(Tracks.Children[1]));
    CHECK(Test.State.SelectedTrack.Peek() == 1);
}

TEST_CASE("Combo boxes expand into a menu whose items can be invoked") {
    Harness Test;

    CHECK(Test.Tree.SetExpanded(Test.Named("Quality").Id, true));
    Test.Rebuild();
    CHECK(Test.Named("Quality").Properties.Expanded == true);

    const SemanticsNode& High = Test.Named("High");
    CHECK(High.Properties.Role == SemanticsRole::MenuItem);
    CHECK(Test.Tree.Find(High.Parent)->Properties.Role == SemanticsRole::Menu);
    CHECK(Test.Tree.Invoke(High.Id));
    Test.Rebuild();
    CHECK(Test.State.Quality.Peek() == 1);
    CHECK(Test.Tree.FindByLabel("High") == nullptr);
    CHECK(Test.Named("Quality").Properties.Value == "High");
}

TEST_CASE("The accessibility tree tracks focus, hit testing and structure") {
    Harness Test;
    const uint64_t PlayId = Test.Named("Play").Id;

    CHECK(Test.Tree.GetFocused() == SemanticsTree::kRootId);
    CHECK(Test.Tree.Focus(PlayId));
    SemanticsUpdate Update = Test.Rebuild();
    CHECK(Update.FocusChanged);
    CHECK(Test.Tree.GetFocused() == PlayId);

    Test.Host.GetInput().HandleKey({.Code = KeyCode::Tab});
    Update = Test.Rebuild();
    CHECK(Update.FocusChanged);
    CHECK(Test.Tree.GetFocused() == Test.Named("Loop").Id);

    CHECK(Test.Tree.HitTest(Test.Named("Volume").Bounds.Center()) == Test.Named("Volume").Id);
    CHECK(Test.Tree.HitTest({399.0f, 599.0f}) == SemanticsTree::kRootId);

    Test.State.ShowExtra = true;
    Update               = Test.Rebuild();
    CHECK(std::ranges::find(Update.StructureChanged, SemanticsTree::kRootId) != Update.StructureChanged.end());
    CHECK(Test.Tree.FindByLabel("Extra") != nullptr);
    CHECK(Test.Named("Play").Id == PlayId);
}
