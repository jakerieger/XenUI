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

#include <Windows.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>

using namespace XenUI;
using namespace XenUI::Testing;

namespace {
    /// @brief Handles every queued message, as the run loop would, including the quit message that closing the
    /// last window posts.
    void PumpMessages() {
        MSG Message {};
        while (PeekMessageW(&Message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&Message);
            DispatchMessageW(&Message);
        }
    }

    /// @brief Writes a 16x16 single-color .ico file.
    std::filesystem::path WriteIconFile() {
        constexpr int kSize = 16;
        std::vector<uint8_t> Bytes;
        const auto Put16 = [&Bytes](uint16_t Value) {
            Bytes.push_back(static_cast<uint8_t>(Value));
            Bytes.push_back(static_cast<uint8_t>(Value >> 8));
        };
        const auto Put32 = [&Put16](uint32_t Value) {
            Put16(static_cast<uint16_t>(Value));
            Put16(static_cast<uint16_t>(Value >> 16));
        };
        const uint32_t PixelBytes = kSize * kSize * 4;
        const uint32_t MaskBytes  = kSize * 4;
        const uint32_t ImageBytes = 40 + PixelBytes + MaskBytes;

        Put16(0);
        Put16(1);
        Put16(1);
        Bytes.push_back(kSize);
        Bytes.push_back(kSize);
        Bytes.push_back(0);
        Bytes.push_back(0);
        Put16(1);
        Put16(32);
        Put32(ImageBytes);
        Put32(6 + 16);

        Put32(40);
        Put32(kSize);
        Put32(kSize * 2);
        Put16(1);
        Put16(32);
        for (int i = 0; i < 6; ++i) {
            Put32(0);
        }
        for (uint32_t i = 0; i < kSize * kSize; ++i) {
            Put32(0xFF4DA3FF);
        }
        Bytes.insert(Bytes.end(), MaskBytes, 0);

        const std::filesystem::path Path = std::filesystem::temp_directory_path() / "XenUITestIcon.ico";
        std::ofstream(Path, std::ios::binary).write(reinterpret_cast<const char*>(Bytes.data()), Bytes.size());
        return Path;
    }
}  // namespace

TEST_CASE("Work posted from other threads runs on the UI thread in order") {
    (void)SharedApp();
    const std::thread::id UiThread = std::this_thread::get_id();

    std::vector<int> Ran;
    bool OnUiThread = true;
    std::thread Worker([&] {
        for (int i = 0; i < 100; ++i) {
            CHECK(App::Post([&, i] {
                OnUiThread = OnUiThread && std::this_thread::get_id() == UiThread;
                Ran.push_back(i);
            }));
        }
    });
    Worker.join();
    CHECK(Ran.empty());

    PumpMessages();
    REQUIRE(Ran.size() == 100);
    CHECK(OnUiThread);
    for (int i = 0; i < 100; ++i) {
        CHECK(Ran[i] == i);
    }

    CHECK_FALSE(App::Post({}));
}

TEST_CASE("A window asks before closing, and Destroy closes it without asking") {
    (void)SharedApp();
    Window Subject({.Title = "Close test", .ClientSize = {200.0f, 100.0f}});

    int Asked   = 0;
    bool Closed = false;
    Subject.SetOnCloseRequested([&Asked] {
        ++Asked;
        return false;
    });
    Subject.SetOnClosed([&Closed] { Closed = true; });

    Subject.Close();
    PumpMessages();
    CHECK(Asked == 1);
    CHECK(Subject.IsOpen());
    CHECK_FALSE(Closed);

    Subject.SetOnCloseRequested([&Asked] {
        ++Asked;
        return true;
    });
    Subject.Close();
    PumpMessages();
    CHECK(Asked == 2);
    CHECK_FALSE(Subject.IsOpen());
    CHECK(Closed);

    Window Other({.Title = "Destroy test", .ClientSize = {200.0f, 100.0f}});
    Other.SetOnCloseRequested([&Asked] {
        ++Asked;
        return false;
    });
    Other.Destroy();
    PumpMessages();
    CHECK(Asked == 2);
    CHECK_FALSE(Other.IsOpen());
}

TEST_CASE("Windows take their icon from an .ico file") {
    (void)SharedApp();
    Window Subject({.Title = "Icon test", .ClientSize = {200.0f, 100.0f}});
    const auto Handle = static_cast<HWND>(Subject.GetNativeHandle());

    // The test executable has no icon resources, so the default leaves the window without one.
    CHECK(SendMessageW(Handle, WM_GETICON, ICON_SMALL, 0) == 0);

    const std::filesystem::path IconFile = WriteIconFile();
    Subject.SetIconFile(IconFile);
    CHECK(SendMessageW(Handle, WM_GETICON, ICON_SMALL, 0) != 0);
    CHECK(SendMessageW(Handle, WM_GETICON, ICON_BIG, 0) != 0);
    std::filesystem::remove(IconFile);

    CHECK_THROWS(Subject.SetIconFile(IconFile));

    Subject.Destroy();
    PumpMessages();
}
