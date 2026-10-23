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

#include <format>
#include <set>
#include <algorithm>
#include <string>
#include <vector>

using namespace XenUI;
using namespace XenUI::Testing;

namespace {
    struct Harness {
        explicit Harness(const Node& Root, Size Viewport = {400.0f, 300.0f}) {
            (void)SharedApp();
            Host.SetViewportSize(Viewport);
            Host.SetRoot(Root);
            Update();
        }

        SemanticsUpdate Update() {
            Host.Update();
            TimerQueue::Get().RunDue();
            Host.Update();
            return Tree.Rebuild(Host.GetView(), &Host.GetInput().GetFocusManager());
        }

        void Press(KeyCode Code, KeyModifiers Modifiers = KeyModifiers::None) {
            Host.GetInput().HandleKey({.Code = Code, .Modifiers = Modifiers});
            Update();
        }

        void Type(const std::string& Text) {
            Host.GetInput().HandleText({Text});
            Update();
        }

        [[nodiscard]] const SemanticsNode& Named(std::string_view Label) const {
            const SemanticsNode* Node = Tree.FindByLabel(Label);
            REQUIRE(Node != nullptr);
            return *Node;
        }

        WidgetHost Host;
        SemanticsTree Tree;
    };

    RenderBox& FirstChildOfRoot(const Harness& Test) {
        return *Test.Host.GetRootBox();
    }

    struct RadioModel {
        Signal<int> Choice = 0;

        struct View {
            RadioModel* Model = nullptr;

            Node Build(BuildContext& Context) const {
                (void)Context;
                RadioModel* Shared = Model;
                return Align {
                  .Position = Alignment::TopLeft(),
                  .Child =
                    RadioGroup {
                      .Options       = {"Low", "Medium", "High"},
                      .SelectedIndex = Shared->Choice(),
                      .OnChanged     = [Shared](int Index) { Shared->Choice = Index; },
                      .Label         = "Quality",
                    },
                };
            }
        };
    };

    struct TabModel {
        Signal<int> Selected = 0;

        struct View {
            TabModel* Model = nullptr;

            Node Build(BuildContext& Context) const {
                (void)Context;
                return TabView {
                  .Tabs =
                    {
                      Tab {.Label = "General", .Content = Text {.Content = "General settings"}},
                      Tab {.Label = "Audio", .Content = Text {.Content = "Audio settings"}},
                      Tab {.Label = "MIDI", .Content = Text {.Content = "MIDI settings"}},
                    },
                  .Selected = &Model->Selected,
                };
            }
        };
    };
}  // namespace

TEST_CASE("Wrap starts a new run when the next child does not fit") {
    NodeList Chips;
    for (int i = 0; i < 5; ++i) {
        Chips.push_back(SizedBox {.Width = 60.0f, .Height = i == 1 ? 30.0f : 20.0f});
    }
    const Harness Test(
      Align {.Position = Alignment::TopLeft(),
             .Child =
               SizedBox {.Width = 200.0f, .Child = Wrap {.Spacing = 10.0f, .RunSpacing = 5.0f, .Children = Chips}}},
      {400.0f, 300.0f});

    RenderBox& Flow = FirstChildOfRoot(Test).GetChild(0).GetChild(0);
    REQUIRE(Flow.GetChildCount() == 5);
    CHECK(Flow.GetChild(2).GetOffset() == Vec2 {140.0f, 0.0f});
    CHECK(Flow.GetChild(3).GetOffset() == Vec2 {0.0f, 35.0f});
    CHECK(Flow.GetChild(4).GetOffset() == Vec2 {70.0f, 35.0f});
    CHECK(Flow.GetSize() == Size {200.0f, 55.0f});
}

TEST_CASE("Grid fits as many columns as the width allows and sizes rows to their tallest child") {
    NodeList Cells;
    for (int i = 0; i < 6; ++i) {
        Cells.push_back(SizedBox {.Height = i == 5 ? 50.0f : 30.0f});
    }
    const Harness Test(
      Align {
        .Position = Alignment::TopLeft(),
        .Child =
          SizedBox {.Width = 400.0f,
                    .Child = Grid {.MinColumnWidth = 90.0f, .Spacing = 10.0f, .RunSpacing = 4.0f, .Children = Cells}}},
      {500.0f, 300.0f});

    auto& Layout = dynamic_cast<RenderGrid&>(FirstChildOfRoot(Test).GetChild(0).GetChild(0));
    CHECK(Layout.GetColumnCount() == 4);
    CHECK(Layout.GetChild(0).GetSize().Width == doctest::Approx(92.5f));
    CHECK(Layout.GetChild(1).GetOffset().X == doctest::Approx(102.5f));
    CHECK(Layout.GetChild(4).GetOffset() == Vec2 {0.0f, 34.0f});
    CHECK(Layout.GetSize().Height == doctest::Approx(84.0f));

    const Harness Fixed(
      Align {.Position = Alignment::TopLeft(),
             .Child = SizedBox {.Width = 300.0f, .Child = Grid {.Columns = 3, .RowHeight = 40.0f, .Children = Cells}}},
      {500.0f, 300.0f});
    auto& FixedLayout = dynamic_cast<RenderGrid&>(FirstChildOfRoot(Fixed).GetChild(0).GetChild(0));
    CHECK(FixedLayout.GetColumnCount() == 3);
    CHECK(FixedLayout.GetChild(3).GetOffset() == Vec2 {0.0f, 40.0f});
    CHECK(FixedLayout.GetChild(5).GetSize() == Size {100.0f, 40.0f});
}

TEST_CASE("Radio groups select one option by click, accessibility or arrow keys") {
    RadioModel Model;
    Harness Test(RadioModel::View {&Model});

    const SemanticsNode& Medium = Test.Named("Medium");
    CHECK(Medium.Properties.Role == SemanticsRole::RadioButton);
    CHECK(Medium.Properties.Selected == false);
    CHECK(Test.Named("Low").Properties.Selected == true);
    CHECK(Test.Tree.Find(Medium.Parent)->Properties.Label == "Quality");

    CHECK(Test.Tree.Select(Medium.Id));
    Test.Update();
    CHECK(Model.Choice.Peek() == 1);
    CHECK(Test.Named("Medium").Properties.Selected == true);

    CHECK(Test.Tree.Focus(Test.Named("Medium").Id));
    Test.Update();
    Test.Press(KeyCode::Down);
    CHECK(Model.Choice.Peek() == 2);
    CHECK(Test.Tree.GetFocused() == Test.Named("High").Id);
    Test.Press(KeyCode::Down);
    CHECK(Model.Choice.Peek() == 0);
}

TEST_CASE("Progress bars report their range, or none while indeterminate") {
    Harness Determinate(Column {.Children = {ProgressBar {.Value = 30.0f, .Max = 120.0f, .Label = "Export"}}});
    const SemanticsNode& Export = Determinate.Named("Export");
    CHECK(Export.Properties.Role == SemanticsRole::ProgressBar);
    REQUIRE(Export.Properties.Range.has_value());
    CHECK(Export.Properties.Range->Value == doctest::Approx(30.0f));
    CHECK(Export.Properties.Range->Max == doctest::Approx(120.0f));
    CHECK(Export.Properties.ReadOnly);

    AnimationTicker::SetAnimationsEnabled(false);
    Harness Waiting(Column {.Children = {ProgressBar {.Label = "Scanning"}}});
    CHECK_FALSE(Waiting.Named("Scanning").Properties.Range.has_value());
    AnimationTicker::SetAnimationsEnabled(std::nullopt);
}

TEST_CASE("Tab views show the selected tab and switch by selection, arrows and Ctrl+Tab") {
    TabModel Model;
    Harness Test(TabModel::View {&Model});

    CHECK(Test.Named("General").Properties.Role == SemanticsRole::TabItem);
    CHECK(Test.Tree.Find(Test.Named("General").Parent)->Properties.Role == SemanticsRole::TabList);
    CHECK(Test.Tree.FindByLabel("General settings") != nullptr);
    CHECK(Test.Tree.FindByLabel("Audio settings") == nullptr);

    CHECK(Test.Tree.Select(Test.Named("Audio").Id));
    Test.Update();
    CHECK(Model.Selected.Peek() == 1);
    CHECK(Test.Tree.FindByLabel("Audio settings") != nullptr);
    CHECK(Test.Tree.FindByLabel("General settings") == nullptr);

    CHECK(Test.Tree.Focus(Test.Named("Audio").Id));
    Test.Update();
    Test.Press(KeyCode::Right);
    CHECK(Model.Selected.Peek() == 2);
    CHECK(Test.Tree.GetFocused() == Test.Named("MIDI").Id);

    Test.Press(KeyCode::Tab, KeyModifiers::Control);
    CHECK(Model.Selected.Peek() == 0);
    Test.Press(KeyCode::Tab, KeyModifiers::Control | KeyModifiers::Shift);
    CHECK(Model.Selected.Peek() == 2);
}

TEST_CASE("Number fields format, parse, clamp and step their value") {
    Signal<float> Gain = 5.0f;
    Harness Test(Align {.Position = Alignment::TopLeft(),
                        .Child    = NumberField {.Bind     = &Gain,
                                                 .Min      = -60.0f,
                                                 .Max      = 12.0f,
                                                 .Step     = 0.5f,
                                                 .Decimals = 1,
                                                 .Suffix   = " dB",
                                                 .Label    = "Gain"}});

    const SemanticsNode& Field = Test.Named("Gain");
    CHECK(Field.Properties.Role == SemanticsRole::Spinner);
    CHECK(Field.Properties.Value == "5.0 dB");
    REQUIRE(Field.Properties.Range.has_value());
    CHECK(Field.Properties.Range->Max == doctest::Approx(12.0f));

    CHECK(Test.Tree.Focus(Field.Id));
    Test.Update();
    Test.Type("-3.25 dB");
    Test.Press(KeyCode::Enter);
    CHECK(Gain.Peek() == doctest::Approx(-3.25f));
    CHECK(Test.Named("Gain").Properties.Value == "-3.2 dB");

    Test.Press(KeyCode::Up);
    CHECK(Gain.Peek() == doctest::Approx(-2.75f));
    Test.Press(KeyCode::Up, KeyModifiers::Shift);
    CHECK(Gain.Peek() == doctest::Approx(2.25f));

    Test.Press(KeyCode::A, KeyModifiers::Control);
    Test.Type("99");
    Test.Press(KeyCode::Enter);
    CHECK(Gain.Peek() == doctest::Approx(12.0f));

    Test.Press(KeyCode::A, KeyModifiers::Control);
    Test.Type("loud");
    Test.Press(KeyCode::Enter);
    CHECK(Gain.Peek() == doctest::Approx(12.0f));
    CHECK(Test.Named("Gain").Properties.Value == "12.0 dB");

    Gain = -20.0f;
    Test.Update();
    CHECK(Test.Named("Gain").Properties.Value == "-20.0 dB");

    CHECK(Test.Tree.SetRangeValue(Test.Named("Gain").Id, -100.0f));
    Test.Update();
    CHECK(Gain.Peek() == doctest::Approx(-60.0f));
}

namespace {
    struct DialogHost {
        Signal<int>* Result = nullptr;

        class State : public ComponentState<DialogHost> {
        protected:
            Node Build(BuildContext& Context) override {
                _Overlay = OverlayController::Of(Context);
                _Style   = PopupStyle::Capture(Context);
                return Align {.Position = Alignment::TopLeft(),
                              .Child    = Button {.Label = "Remove clip", .OnPressed = [this] { Open(); }}};
            }

        private:
            void Open() {
                Signal<int>* Outcome = GetWidget().Result;
                _Dialog              = ShowDialog(
                  *_Overlay,
                  _Style,
                  Dialog {
                    .Title   = "Delete clip?",
                    .Content = Text {.Content = "This cannot be undone."},
                    .Actions =
                      {
                        DialogAction {.Label = "Cancel", .OnPressed = [this, Outcome] { Close(Outcome, 0); }},
                        DialogAction {.Label     = "Delete",
                                      .OnPressed = [this, Outcome] { Close(Outcome, 1); },
                                      .Variant   = ButtonVariant::Primary},
                      },
                    .OnDismiss = [this, Outcome] { Close(Outcome, -1); },
                  });
            }

            void Close(Signal<int>* Outcome, int Value) {
                *Outcome = Value;
                _Dialog.Reset();
            }

            OverlayController* _Overlay = nullptr;
            PopupStyle _Style;
            OverlayEntry _Dialog;
        };
    };

    struct MenuHost {
        std::vector<std::string>* Log = nullptr;

        Node Build(BuildContext& Context) const {
            (void)Context;
            std::vector<std::string>* Record = Log;
            return Align {
              .Position = Alignment::TopLeft(),
              .Child =
                MenuBar {
                  .Menus =
                    {
                      Menu {.Label = "File",
                            .Items = {MenuItem {.Label = "Open", .OnSelect = [Record] { Record->push_back("open"); }}}},
                      Menu {.Label = "Edit",
                            .Items = {MenuItem {.Label = "Undo", .OnSelect = [Record] { Record->push_back("undo"); }}}},
                    }},
            };
        }
    };

    std::vector<TreeItem> LibraryChildren(const std::string& Parent) {
        if (Parent.empty()) {
            return {{"drums", "Drums", true}, {"synths", "Synths", true}, {"readme", "Readme", false}};
        }
        if (Parent == "drums") { return {{"kick", "Kick", false}, {"snare", "Snare", false}}; }
        if (Parent == "synths") { return {{"pads", "Pads", true}}; }
        if (Parent == "pads") { return {{"warm", "Warm pad", false}}; }
        return {};
    }
}  // namespace

TEST_CASE("Dialogs trap focus, run their default action on Enter and dismiss on Escape") {
    Signal<int> Result = 99;
    Harness Test(DialogHost {&Result});

    CHECK(Test.Tree.Invoke(Test.Named("Remove clip").Id));
    Test.Update();
    const SemanticsNode& Panel = Test.Named("Delete clip?");
    CHECK(Panel.Properties.Role == SemanticsRole::Dialog);
    CHECK(Test.Tree.GetFocused() == Panel.Id);

    const uint64_t Opener = Test.Named("Remove clip").Id;
    for (int Step = 0; Step < 6; ++Step) {
        Test.Press(KeyCode::Tab);
        CHECK(Test.Tree.GetFocused() != Opener);
    }

    CHECK(Test.Tree.Focus(Test.Named("Delete clip?").Id));
    Test.Update();
    Test.Press(KeyCode::Enter);
    CHECK(Result.Peek() == 1);
    CHECK(Test.Tree.FindByLabel("Delete clip?") == nullptr);

    CHECK(Test.Tree.Invoke(Test.Named("Remove clip").Id));
    Test.Update();
    Test.Press(KeyCode::Escape);
    CHECK(Result.Peek() == -1);
    CHECK(Test.Tree.FindByLabel("Delete clip?") == nullptr);
}

TEST_CASE("Menu bars open menus, switch between them and run items") {
    std::vector<std::string> Log;
    Harness Test(MenuHost {&Log});

    const SemanticsNode& File = Test.Named("File");
    CHECK(Test.Tree.Find(File.Parent)->Properties.Role == SemanticsRole::MenuBar);
    CHECK(File.Properties.Expanded == false);

    CHECK(Test.Tree.Invoke(File.Id));
    Test.Update();
    CHECK(Test.Named("File").Properties.Expanded == true);
    REQUIRE(Test.Tree.FindByLabel("Open") != nullptr);

    Test.Press(KeyCode::Right);
    CHECK(Test.Named("Edit").Properties.Expanded == true);
    CHECK(Test.Named("File").Properties.Expanded == false);
    CHECK(Test.Tree.FindByLabel("Open") == nullptr);

    CHECK(Test.Tree.Invoke(Test.Named("Undo").Id));
    Test.Update();
    CHECK(Log == std::vector<std::string> {"undo"});
    CHECK(Test.Named("Edit").Properties.Expanded == false);
    CHECK(Test.Tree.FindByLabel("Undo") == nullptr);

    CHECK(Test.Tree.SetExpanded(Test.Named("File").Id, true));
    Test.Update();
    Test.Press(KeyCode::Escape);
    CHECK(Test.Named("File").Properties.Expanded == false);
}

TEST_CASE("Tree views expand, navigate with the keyboard and activate items") {
    std::vector<std::string> Activated;
    Signal<std::string> Selected;
    Harness Test(
      SizedBox {.Height = 240.0f,
                .Child  = TreeView {.ChildrenOf  = LibraryChildren,
                                    .Selected    = &Selected,
                                    .OnActivated = [&Activated](const std::string& Id) { Activated.push_back(Id); },
                                    .Label       = "Library"}});

    CHECK(Test.Named("Library").Properties.Role == SemanticsRole::Tree);
    CHECK(Test.Named("Drums").Properties.Role == SemanticsRole::TreeItem);
    CHECK(Test.Named("Drums").Properties.Expanded == false);
    CHECK_FALSE(Test.Named("Readme").Properties.Expanded.has_value());
    CHECK(Test.Tree.FindByLabel("Kick") == nullptr);

    CHECK(Test.Tree.Focus(Test.Named("Library").Id));
    Test.Update();
    Test.Press(KeyCode::Down);
    CHECK(Selected.Peek() == "drums");
    Test.Press(KeyCode::Right);
    CHECK(Test.Tree.FindByLabel("Kick") != nullptr);
    Test.Press(KeyCode::Right);
    CHECK(Selected.Peek() == "kick");
    Test.Press(KeyCode::Down);
    CHECK(Selected.Peek() == "snare");
    Test.Press(KeyCode::Left);
    CHECK(Selected.Peek() == "drums");
    Test.Press(KeyCode::Left);
    CHECK(Test.Tree.FindByLabel("Kick") == nullptr);
    Test.Press(KeyCode::End);
    CHECK(Selected.Peek() == "readme");
    Test.Press(KeyCode::Enter);
    CHECK(Activated == std::vector<std::string> {"readme"});

    CHECK(Test.Tree.SetExpanded(Test.Named("Synths").Id, true));
    Test.Update();
    CHECK(Test.Tree.SetExpanded(Test.Named("Pads").Id, true));
    Test.Update();
    CHECK(Test.Tree.FindByLabel("Warm pad") != nullptr);
    CHECK(Test.Tree.Select(Test.Named("Warm pad").Id));
    Test.Update();
    CHECK(Selected.Peek() == "warm");
    CHECK(Test.Named("Warm pad").Properties.Selected == true);
}

namespace {
    struct MenuFocusHost {
        Signal<bool>* ShowButton = nullptr;

        Node Build(BuildContext& Context) const {
            (void)Context;
            NodeList Children {MenuBar {.Menus = {Menu {.Label = "File", .Items = {MenuItem {.Label = "Open"}}}}}};
            if (ShowButton->Get()) { Children.push_back(Button {.Label = "Play"}); }
            return Align {.Position = Alignment::TopLeft(),
                          .Child =
                            Column {.CrossAlignment = CrossAxisAlignment::Start, .Children = std::move(Children)}};
        }
    };
}  // namespace

TEST_CASE("Clicking a menu bar item does not take focus, so focus never falls back to it") {
    Signal<bool> ShowButton = true;
    Harness Test(MenuFocusHost {&ShowButton});

    const uint64_t Play = Test.Named("Play").Id;
    CHECK(Test.Tree.Focus(Play));
    Test.Update();

    const Vec2 FileCenter = Test.Named("File").Bounds.Center();
    Test.Host.GetInput().HandlePointer({.Phase = PointerPhase::Move, .Position = FileCenter});
    Test.Host.GetInput().HandlePointer({.Phase    = PointerPhase::Down,
                                        .Position = FileCenter,
                                        .Button   = MouseButton::Primary,
                                        .Buttons  = MouseButton::Primary});
    Test.Host.GetInput().HandlePointer(
      {.Phase = PointerPhase::Up, .Position = FileCenter, .Button = MouseButton::Primary});
    Test.Update();
    CHECK(Test.Named("File").Properties.Expanded == true);

    Test.Press(KeyCode::Escape);
    CHECK(Test.Named("File").Properties.Expanded == false);
    CHECK(Test.Tree.GetFocused() == Play);

    ShowButton = false;
    Test.Update();
    CHECK(Test.Tree.GetFocused() != Test.Named("File").Id);
}

TEST_CASE("Text areas insert line breaks and move the caret by visual lines") {
    TextEditingController Notes;
    std::string Submitted;
    Harness Test(Align {
      .Position = Alignment::TopLeft(),
      .Child    = SizedBox {.Width = 220.0f,
                            .Child = TextArea {.Controller = &Notes,
                                               .OnSubmit   = [&Submitted](const std::string& Text) { Submitted = Text; },
                                               .Lines      = 4,
                                               .Label      = "Notes"}}});

    CHECK(Test.Named("Notes").Properties.Role == SemanticsRole::Edit);
    CHECK(Test.Tree.Focus(Test.Named("Notes").Id));
    Test.Update();
    Test.Type("first");
    Test.Press(KeyCode::Enter);
    Test.Type("second");
    CHECK(Notes.PeekText() == "first\nsecond");

    Test.Press(KeyCode::Up);
    Test.Type("X");
    CHECK(Notes.PeekText() == "firstX\nsecond");
    Test.Press(KeyCode::Home);
    Test.Type("<");
    CHECK(Notes.PeekText() == "<firstX\nsecond");
    Test.Press(KeyCode::Down);
    Test.Press(KeyCode::End);
    Test.Type("!");
    CHECK(Notes.PeekText() == "<firstX\nsecond!");
    Test.Press(KeyCode::Home, KeyModifiers::Control);
    CHECK(Notes.GetEditor().GetSelection().Caret == 0);

    Test.Press(KeyCode::Enter, KeyModifiers::Control);
    CHECK(Submitted == "<firstX\nsecond!");

    Notes.SetText("one two three four five six seven eight nine ten eleven twelve thirteen fourteen");
    Test.Update();
    Test.Press(KeyCode::End, KeyModifiers::Control);
    Test.Press(KeyCode::Home);
    const size_t LineStart = Notes.GetEditor().GetSelection().Caret;
    CHECK(LineStart > 0);
    CHECK(LineStart < Notes.PeekText().size());
    Test.Press(KeyCode::Up);
    CHECK(Notes.GetEditor().GetSelection().Caret < LineStart);
}

TEST_CASE("Tables describe their columns and rows, sort on request and navigate by keyboard") {
    std::vector<TableSort> Requests;
    int Activated        = -1;
    Signal<int> Selected = -1;
    Harness Test(SizedBox {.Height = 300.0f,
                           .Child  = Table {
                             .Columns  = {TableColumn {.Title = "Name", .Width = 160.0f},
                                          TableColumn {.Title = "Size", .Alignment = TextAlignment::Trailing}},
                             .RowCount = 1000,
                             .CellText =
                               [](int Row, int Column) {
                                   return Column == 0 ? std::format("Sample {}", Row) : std::format("{} KB", Row * 3);
                               },
                             .Selected    = &Selected,
                             .OnActivated = [&Activated](int Row) { Activated = Row; },
                             .OnSort      = [&Requests](const TableSort& Sort) { Requests.push_back(Sort); },
                             .Label       = "Samples",
                           }});

    CHECK(Test.Named("Samples").Properties.Role == SemanticsRole::Table);
    CHECK(Test.Named("Name").Properties.Role == SemanticsRole::ColumnHeader);
    const SemanticsNode& First = Test.Named("Sample 0 0 KB");
    CHECK(First.Properties.Role == SemanticsRole::TableRow);

    CHECK(Test.Tree.Invoke(Test.Named("Size").Id));
    Test.Update();
    CHECK(Test.Tree.Invoke(Test.Named("Size").Id));
    Test.Update();
    REQUIRE(Requests.size() == 2);
    CHECK(Requests[0] == TableSort {1, true});
    CHECK(Requests[1] == TableSort {1, false});

    CHECK(Test.Tree.Focus(Test.Named("Samples").Id));
    Test.Update();
    Test.Press(KeyCode::Down);
    CHECK(Selected.Peek() == 0);
    Test.Press(KeyCode::End);
    CHECK(Selected.Peek() == 999);
    Test.Update();
    CHECK(Test.Tree.FindByLabel("Sample 999 2997 KB") != nullptr);
    Test.Press(KeyCode::Enter);
    CHECK(Activated == 999);

    CHECK(Test.Tree.Select(Test.Named("Sample 998 2994 KB").Id));
    Test.Update();
    CHECK(Selected.Peek() == 998);
}

TEST_CASE("Tree views scroll to a selection made from outside, even inside newly expanded items") {
    Signal<std::string> Selected;
    Signal<std::set<std::string>> Expanded;
    ScrollController Scroll;
    const auto Children = [](const std::string& Parent) {
        std::vector<TreeItem> Items;
        const int Depth = Parent.empty() ? 0 : static_cast<int>(std::count(Parent.begin(), Parent.end(), '/')) + 1;
        if (Depth >= 4) { return Items; }
        for (int i = 0; i < 6; ++i) {
            const std::string Id = Parent.empty() ? std::to_string(i) : Parent + "/" + std::to_string(i);
            Items.push_back({Id, "Item " + Id, Depth < 3});
        }
        return Items;
    };
    Harness Test(SizedBox {.Height = 120.0f,
                           .Child  = TreeView {.ChildrenOf = Children,
                                               .Selected   = &Selected,
                                               .Expanded   = &Expanded,
                                               .Controller = &Scroll,
                                               .Label      = "Deep"}});
    CHECK(Scroll.PeekOffset() == doctest::Approx(0.0f));

    Expanded = std::set<std::string> {"5", "5/5", "5/5/5"};
    Selected = std::string("5/5/5/5");
    Test.Update();
    Test.Update();
    CHECK(Scroll.PeekOffset() > 100.0f);
    CHECK(Test.Tree.FindByLabel("Item 5/5/5/5") != nullptr);
}
