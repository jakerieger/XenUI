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

#include <vector>

using namespace XenUI;
using namespace XenUI::Testing;

TEST_CASE("Nodes are reference counted and released back to the arena") {
    const size_t Baseline = WidgetArena::GetStats().LiveAllocations;
    {
        Node First = Label {"hello"};
        CHECK(WidgetArena::GetStats().LiveAllocations == Baseline + 1);

        const Node Copy = First;
        CHECK(Copy.IsSameWidget(First));
        CHECK(WidgetArena::GetStats().LiveAllocations == Baseline + 1);

        First = Node {};
        CHECK(Copy.As<Label>().Text == "hello");
    }
    CHECK(WidgetArena::GetStats().LiveAllocations == Baseline);
}

TEST_CASE("Nested descriptions keep their children alive") {
    const size_t Baseline = WidgetArena::GetStats().LiveAllocations;
    Node Tree;
    {
        Node Child = Label {"child"};
        Tree       = Box {.Name = "parent", .Children = {Child, Label {"other"}}};
    }
    CHECK(WidgetArena::GetStats().LiveAllocations == Baseline + 3);
    CHECK(Tree.As<Box>().Children[0].As<Label>().Text == "child");

    Tree = Node {};
    CHECK(WidgetArena::GetStats().LiveAllocations == Baseline);
}

TEST_CASE("Arena blocks are recycled once every allocation in them dies") {
    const WidgetArenaStats Before = WidgetArena::GetStats();
    {
        std::vector<Node> Many;
        for (int i = 0; i < 5000; ++i) {
            Many.push_back(Label {"item " + std::to_string(i)});
        }
        CHECK(WidgetArena::GetStats().ActiveBlocks > Before.ActiveBlocks);
    }
    const WidgetArenaStats After = WidgetArena::GetStats();
    CHECK(After.LiveAllocations == Before.LiveAllocations);
    CHECK(After.ActiveBlocks <= Before.ActiveBlocks + 1);
}

TEST_CASE("Node type identity and keys") {
    const Node A = Label {"a"};
    const Node B = Label {"b"};
    const Node C = Box {"c"};

    CHECK(A.IsSameType(B));
    CHECK_FALSE(A.IsSameType(C));
    CHECK(A.Is<Label>());
    CHECK_FALSE(A.Is<Box>());

    const Node KeyedA = Keyed("first", A);
    CHECK(KeyedA.GetKey() == Key("first"));
    CHECK_FALSE(KeyedA.IsSameWidget(A));
    CHECK(Key(42) == Key(42));
    CHECK_FALSE(Key(42) == Key("42"));
    CHECK_FALSE(Key().IsValid());
}
