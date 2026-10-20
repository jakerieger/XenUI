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

TEST_CASE("Effects run immediately and again when a dependency changes") {
    Signal<int> Count = 1;
    std::vector<int> Seen;
    const Effect Watcher([&] { Seen.push_back(Count()); });

    CHECK(Seen == std::vector<int> {1});
    Count = 2;
    CHECK(Seen == std::vector<int> {1, 2});
    Count = 2;
    CHECK(Seen == std::vector<int> {1, 2});
}

TEST_CASE("Peek and Untracked reads do not subscribe") {
    Signal<int> Count = 1;
    int Runs          = 0;
    const Effect Watcher([&] {
        ++Runs;
        (void)Count.Peek();
        Untracked([&] { (void)Count.Get(); });
    });

    Count = 5;
    CHECK(Runs == 1);
}

TEST_CASE("Batch coalesces multiple writes into one effect run") {
    Signal<int> Left  = 1;
    Signal<int> Right = 2;
    std::vector<int> Sums;
    const Effect Watcher([&] { Sums.push_back(Left() + Right()); });

    Batch([&] {
        Left  = 10;
        Right = 20;
    });
    CHECK(Sums == std::vector<int> {3, 30});
}

TEST_CASE("Computed values are lazy, cached and observable") {
    Signal<int> Base = 2;
    int Evaluations  = 0;
    const Computed<int> Doubled([&] {
        ++Evaluations;
        return Base() * 2;
    });

    CHECK(Evaluations == 0);
    CHECK(Doubled() == 4);
    CHECK(Doubled() == 4);
    CHECK(Evaluations == 1);

    std::vector<int> Seen;
    const Effect Watcher([&] { Seen.push_back(Doubled()); });
    Base = 5;
    CHECK(Seen == std::vector<int> {4, 10});
    CHECK(Evaluations == 2);
}

TEST_CASE("Effects only track what they read on their latest run") {
    Signal<bool> UseLeft      = true;
    Signal<std::string> Left  = std::string("left");
    Signal<std::string> Right = std::string("right");
    std::vector<std::string> Seen;
    const Effect Watcher([&] { Seen.push_back(UseLeft() ? Left() : Right()); });

    Right = std::string("ignored");
    CHECK(Seen.size() == 1);

    UseLeft = false;
    CHECK(Seen.back() == "ignored");

    Left = std::string("now ignored");
    CHECK(Seen.size() == 2);
}

TEST_CASE("Modify notifies after in-place mutation") {
    Signal<std::vector<int>> Items;
    size_t LastSize = 0;
    const Effect Watcher([&] { LastSize = Items().size(); });

    Items.Modify([](std::vector<int>& Values) { Values.push_back(7); });
    CHECK(LastSize == 1);
}

TEST_CASE("Destroying either side unlinks the subscription") {
    Signal<int> Source = 0;
    {
        const Effect Watcher([&] { (void)Source(); });
        Source = 1;
    }
    Source = 2;

    auto Temporary = std::make_unique<Signal<int>>(0);
    const Effect Survivor([&] {
        if (Temporary) { (void)Temporary->Get(); }
    });
    Temporary.reset();
    CHECK(Survivor.GetDependencyCount() == 0);
}
