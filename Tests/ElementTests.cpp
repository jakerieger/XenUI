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
#include <vector>

using namespace XenUI;
using namespace XenUI::Testing;

namespace {
    int CounterBuilds = 0;

    struct CounterView {
        const Signal<int>* Count = nullptr;

        Node Build(BuildContext& Context) const {
            (void)Context;
            ++CounterBuilds;
            return Label {"count=" + std::to_string(Count->Get())};
        }
    };

    int LiveStates     = 0;
    int DisposedStates = 0;
    int NextStateId    = 0;

    struct Tracked {
        std::string Name;

        class State : public ComponentState<Tracked> {
        public:
            State() { ++LiveStates; }
            ~State() override { --LiveStates; }

            const int Id = NextStateId++;
            int Updates  = 0;

        protected:
            void Dispose() override { ++DisposedStates; }
            void WidgetUpdated(const Tracked& Previous) override {
                (void)Previous;
                ++Updates;
            }
            Node Build(BuildContext& Context) override {
                (void)Context;
                return Label {GetWidget().Name};
            }
        };
    };

    struct Palette {
        std::string Accent;

        bool operator==(const Palette&) const = default;
    };

    int ThemedBuilds = 0;

    struct ThemedLabel {
        Node Build(BuildContext& Context) const {
            ++ThemedBuilds;
            const Palette* Current = Context.Find<Palette>();
            return Label {Current != nullptr ? Current->Accent : "none"};
        }
    };

    struct Toggle {
        const Signal<bool>* UseBox = nullptr;

        Node Build(BuildContext& Context) const {
            (void)Context;
            if (UseBox->Get()) { return Box {.Name = "box", .Children = {Label {"inner"}}}; }
            return Label {"label"};
        }
    };

    int WrapperBuilds = 0;

    struct Wrapper {
        const Signal<int>* Revision = nullptr;
        Node Child;

        Node Build(BuildContext& Context) const {
            (void)Context;
            ++WrapperBuilds;
            return Frame {.Name = "wrap" + std::to_string(Revision->Get()), .Child = Child};
        }
    };

    std::vector<int> CollectStateIds(const ElementTree& Tree) {
        std::vector<int> Ids;
        Tree.GetRootElement()->VisitChildren([&](Element& Child) {
            if (const auto* Stateful = dynamic_cast<const StatefulElement<Tracked>*>(&Child)) {
                Ids.push_back(Stateful->GetState().Id);
            }
        });
        return Ids;
    }

    Node TrackedList(const std::vector<std::string>& Names, bool UseKeys) {
        NodeList Children;
        for (const std::string& Name : Names) {
            const Node Item = Tracked {Name};
            Children.push_back(UseKeys ? Keyed(Name, Item) : Item);
        }
        return Box {.Name = "list", .Children = std::move(Children)};
    }
}  // namespace

TEST_CASE("Render tree mirrors render widgets") {
    const ElementTree Tree(Box {.Name = "root", .Children = {Label {"a"}, Frame {.Name = "f", .Child = Label {"b"}}}});

    const RenderObject* Root = Tree.GetRootRenderObject();
    REQUIRE(Root != nullptr);
    CHECK(NameOf(Root) == "root");
    CHECK(DescribeChildren(Root) == "a,f");
    CHECK(DescribeChildren(Root->GetChildren()[1]) == "b");
    CHECK(Root->GetChildren()[0]->GetParent() == Root);
}

TEST_CASE("Components rebuild when a signal they read changes, and only then") {
    Signal<int> Count = 0;
    CounterBuilds     = 0;
    ElementTree Tree(Box {.Name = "root", .Children = {CounterView {&Count}, Label {"static"}}});

    CHECK(CounterBuilds == 1);
    CHECK(DescribeChildren(Tree.GetRootRenderObject()) == "count=0,static");

    bool Scheduled = false;
    Tree.SetOnBuildScheduled([&] { Scheduled = true; });
    Count = 3;
    CHECK(Scheduled);
    CHECK(Tree.NeedsBuild());
    CHECK(CounterBuilds == 1);

    Tree.Build();
    CHECK(CounterBuilds == 2);
    CHECK(DescribeChildren(Tree.GetRootRenderObject()) == "count=3,static");

    Tree.Build();
    CHECK(CounterBuilds == 2);
}

TEST_CASE("Unmounting removes signal subscriptions") {
    Signal<int> Count = 0;
    {
        const ElementTree Tree(CounterView {&Count});
        Count = 1;
    }
    CounterBuilds = 0;
    Count         = 2;
    CHECK(CounterBuilds == 0);
}

TEST_CASE("State survives parent rebuilds of the same type and key") {
    LiveStates     = 0;
    DisposedStates = 0;
    ElementTree Tree(TrackedList({"a", "b"}, false));
    const std::vector<int> Initial = CollectStateIds(Tree);
    REQUIRE(Initial.size() == 2);

    Tree.SetRoot(TrackedList({"a2", "b2"}, false));
    CHECK(CollectStateIds(Tree) == Initial);
    CHECK(DescribeChildren(Tree.GetRootRenderObject()) == "a2,b2");
    CHECK(LiveStates == 2);
    CHECK(DisposedStates == 0);
}

TEST_CASE("Keyed children keep their state when reordered") {
    ElementTree Tree(TrackedList({"a", "b", "c", "d"}, true));
    const std::vector<int> Ids = CollectStateIds(Tree);

    Tree.SetRoot(TrackedList({"d", "b", "a", "c"}, true));
    CHECK(CollectStateIds(Tree) == std::vector<int> {Ids[3], Ids[1], Ids[0], Ids[2]});
    CHECK(DescribeChildren(Tree.GetRootRenderObject()) == "d,b,a,c");
}

TEST_CASE("Keyed insertions and removals create and dispose only the affected children") {
    LiveStates     = 0;
    DisposedStates = 0;
    ElementTree Tree(TrackedList({"a", "b", "c"}, true));
    const std::vector<int> Ids = CollectStateIds(Tree);

    Tree.SetRoot(TrackedList({"a", "x", "c"}, true));
    const std::vector<int> After = CollectStateIds(Tree);
    CHECK(After[0] == Ids[0]);
    CHECK(After[2] == Ids[2]);
    CHECK(After[1] != Ids[1]);
    CHECK(DisposedStates == 1);
    CHECK(LiveStates == 3);

    Tree.SetRoot(TrackedList({}, true));
    CHECK(LiveStates == 0);
    CHECK(DescribeChildren(Tree.GetRootRenderObject()).empty());
}

TEST_CASE("Changing a widget's type replaces its element") {
    LiveStates     = 0;
    DisposedStates = 0;
    ElementTree Tree(Box {.Name = "root", .Children = {Tracked {"t"}}});
    CHECK(LiveStates == 1);

    Tree.SetRoot(Box {.Name = "root", .Children = {Label {"plain"}}});
    CHECK(LiveStates == 0);
    CHECK(DisposedStates == 1);
    CHECK(DescribeChildren(Tree.GetRootRenderObject()) == "plain");
}

TEST_CASE("WidgetUpdated receives each new description") {
    ElementTree Tree(Tracked {"one"});
    auto* Stateful = dynamic_cast<StatefulElement<Tracked>*>(Tree.GetRootElement());
    REQUIRE(Stateful != nullptr);

    Tree.SetRoot(Tracked {"two"});
    Tree.SetRoot(Tracked {"three"});
    CHECK(Stateful->GetState().Updates == 2);
    CHECK(NameOf(Tree.GetRootRenderObject()) == "three");
}

TEST_CASE("Provided values reach descendants and rebuild them only when they change") {
    ThemedBuilds = 0;
    ElementTree Tree(Provide<Palette> {.Value = {"blue"}, .Child = Frame {.Name = "f", .Child = ThemedLabel {}}});
    CHECK(DescribeChildren(Tree.GetRootRenderObject()) == "blue");
    CHECK(ThemedBuilds == 1);

    const Node SameChild = Frame {.Name = "f", .Child = ThemedLabel {}};
    Tree.SetRoot(Provide<Palette> {.Value = {"blue"}, .Child = SameChild});
    Tree.Build();
    CHECK(ThemedBuilds == 2);

    Tree.SetRoot(Provide<Palette> {.Value = {"red"}, .Child = SameChild});
    Tree.Build();
    CHECK(DescribeChildren(Tree.GetRootRenderObject()) == "red");

    const ElementTree Orphan(ThemedLabel {});
    CHECK(NameOf(Orphan.GetRootRenderObject()) == "none");
}

TEST_CASE("Replacing a component's render object updates the nearest render ancestor") {
    Signal<bool> UseBox = false;
    ElementTree Tree(Box {.Name = "root", .Children = {Label {"first"}, Toggle {&UseBox}, Label {"last"}}});
    CHECK(DescribeChildren(Tree.GetRootRenderObject()) == "first,label,last");

    UseBox = true;
    Tree.Build();
    const RenderObject* Root = Tree.GetRootRenderObject();
    CHECK(DescribeChildren(Root) == "first,box,last");
    CHECK(DescribeChildren(Root->GetChildren()[1]) == "inner");

    UseBox = false;
    Tree.Build();
    CHECK(DescribeChildren(Tree.GetRootRenderObject()) == "first,label,last");
}

TEST_CASE("Root render object changes are reported") {
    Signal<bool> UseBox = false;
    ElementTree Tree(Toggle {&UseBox});
    const RenderObject* Reported = nullptr;
    int Reports                  = 0;
    Tree.SetOnRootRenderObjectChanged([&](RenderObject* Root) {
        Reported = Root;
        ++Reports;
    });

    UseBox = true;
    Tree.Build();
    CHECK(Reports >= 1);
    CHECK(Reported == Tree.GetRootRenderObject());
    CHECK(NameOf(Reported) == "box");
}

TEST_CASE("A child passed through properties outlives the build that created it") {
    Signal<int> Revision = 0;
    Signal<int> Count    = 7;
    WrapperBuilds        = 0;
    std::unique_ptr<ElementTree> Tree;
    {
        Node Content = CounterView {&Count};
        Tree         = std::make_unique<ElementTree>(Wrapper {&Revision, Content});
    }
    CHECK(NameOf(Tree->GetRootRenderObject()) == "wrap0");

    Revision = 1;
    Tree->Build();
    CHECK(WrapperBuilds == 2);
    CHECK(NameOf(Tree->GetRootRenderObject()) == "wrap1");
    CHECK(DescribeChildren(Tree->GetRootRenderObject()) == "count=7");

    CounterBuilds = 0;
    Revision      = 2;
    Tree->Build();
    CHECK(CounterBuilds == 0);

    Count = 8;
    Tree->Build();
    CHECK(DescribeChildren(Tree->GetRootRenderObject()) == "count=8");
}

TEST_CASE("Dirty parents rebuild before their dirty children") {
    Signal<int> Revision = 0;
    Signal<int> Count    = 0;
    ElementTree Tree(Wrapper {&Revision, CounterView {&Count}});

    CounterBuilds = 0;
    WrapperBuilds = 0;
    Batch([&] {
        Count    = 1;
        Revision = 1;
    });
    Tree.Build();
    CHECK(WrapperBuilds == 1);
    CHECK(CounterBuilds == 1);
    CHECK(DescribeChildren(Tree.GetRootRenderObject()) == "count=1");
}
