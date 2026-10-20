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

#include <XenUI/Graphics/LiveRecording.hpp>
#include <XenUI/XenUI.hpp>

#include <doctest/doctest.h>

#include <atomic>
#include <thread>
#include <vector>

using namespace XenUI;

TEST_CASE("SpscQueue preserves order and reports full and empty") {
    SpscQueue<int, 8> Queue;
    CHECK_FALSE(Queue.Pop().has_value());

    for (int i = 0; i < 7; ++i) {
        CHECK(Queue.Push(i));
    }
    CHECK_FALSE(Queue.Push(99));
    CHECK(Queue.GetSize() == 7);

    for (int i = 0; i < 7; ++i) {
        CHECK(Queue.Pop() == i);
    }
    CHECK_FALSE(Queue.Pop().has_value());

    const int Values[] = {1, 2, 3};
    CHECK(Queue.PushRange(Values) == 3);
    int Received[8] {};
    CHECK(Queue.PopRange(Received) == 3);
    CHECK(Received[2] == 3);
}

TEST_CASE("SpscQueue transfers a long stream between threads without loss") {
    constexpr int kCount = 1'000'000;
    SpscQueue<int, 1024> Queue;

    std::thread Producer([&] {
        for (int i = 0; i < kCount;) {
            if (Queue.Push(i)) { ++i; }
        }
    });

    int Expected = 0;
    bool InOrder = true;
    while (Expected < kCount) {
        if (const std::optional<int> Value = Queue.Pop()) {
            InOrder = InOrder && *Value == Expected;
            ++Expected;
        }
    }
    Producer.join();
    CHECK(InOrder);
    CHECK(Expected == kCount);
}

TEST_CASE("TripleBuffer hands over the latest value and never tears") {
    struct Snapshot {
        int A = 0;
        int B = 0;
    };
    TripleBuffer<Snapshot> Buffer;
    CHECK_FALSE(Buffer.Acquire());

    Buffer.Publish({1, 1});
    Buffer.Publish({2, 2});
    CHECK(Buffer.Acquire());
    CHECK(Buffer.Read().A == 2);
    CHECK_FALSE(Buffer.Acquire());

    std::atomic<bool> Done = false;
    std::thread Producer([&] {
        for (int i = 3; i < 200'000; ++i) {
            Snapshot& Slot = Buffer.GetWriteSlot();
            Slot.A         = i;
            Slot.B         = i;
            Buffer.Publish();
        }
        Done = true;
    });

    int Last     = 2;
    bool Ordered = true;
    while (!Done) {
        if (Buffer.Acquire()) {
            const Snapshot& Value = Buffer.Read();
            Ordered               = Ordered && Value.A == Value.B && Value.A >= Last;
            Last                  = Value.A;
        }
    }
    Producer.join();
    Buffer.Acquire();
    CHECK(Ordered);
    CHECK(Buffer.Read().A == 199'999);
}

TEST_CASE("LiveCanvas batches primitives and splits batches at textures") {
    LiveRecording Recording;
    LiveCanvas Canvas(Recording, {200.0f, 100.0f});
    LiveTexture Texture(4, 4);

    std::vector<Vec2> Points;
    for (int i = 0; i < 100; ++i) {
        Points.push_back({static_cast<float>(i), static_cast<float>(i % 7)});
    }
    Canvas.DrawPolyline(Points, 1.5f, Color::FromHex(0xFFFFFF));
    Canvas.FillRect({0.0f, 0.0f, 10.0f, 10.0f}, Color::FromHex(0xFF0000));
    Canvas.DrawTexture(Texture, {0.0f, 0.0f, 50.0f, 50.0f});
    const float Tops[] = {10.0f, 20.0f, 100.0f};
    Canvas.FillColumns(0.0f, 1.0f, 100.0f, Tops, Color::FromHex(0x00FF00));

    CHECK(Canvas.GetPrimitiveCount() == 99 + 1 + 2);
    REQUIRE(Recording.Batches.size() == 3);
    CHECK(Recording.Batches[0].InstanceCount == 100);
    CHECK(Recording.Batches[1].IsTexture);
    CHECK(Recording.Batches[2].FirstInstance == 100);
    CHECK(Recording.Batches[2].InstanceCount == 2);

    Canvas.FillRect({0.0f, 0.0f, 0.0f, 10.0f}, Color::FromHex(0xFF0000));
    Canvas.DrawLine({0.0f, 0.0f}, {1.0f, 1.0f}, 1.0f, Color::FromHex(0xFF0000, 0.0f));
    CHECK(Canvas.GetPrimitiveCount() == 102);
}

TEST_CASE("LiveTexture columns write top to bottom") {
    LiveTexture Texture(3, 2);
    const uint32_t Column[] = {0xFF0000FFu, 0xFF00FF00u};
    Texture.WriteColumn(1, Column);
    CHECK(Texture.GetData().Pixels[1] == 0xFF0000FFu);
    CHECK(Texture.GetData().Pixels[4] == 0xFF00FF00u);
    CHECK_THROWS_AS(Texture.WriteColumn(3, Column), Error);
}
