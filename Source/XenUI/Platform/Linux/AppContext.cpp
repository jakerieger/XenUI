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

#include "XenUI/AppContext.hpp"
#include "XenUI/Core/Animation.hpp"
#include "XenUI/Core/Timer.hpp"
#include "XenUI/Platform/Linux/SdlPlatform.hpp"
#include "XenUI/Platform/Linux/WindowImpl.hpp"

#include <algorithm>
#include <cmath>

namespace XenUI {
    namespace {
        constexpr int32_t kAnimationWaitMilliseconds = 16;
        constexpr double kMaxWaitMilliseconds        = 2147483647.0;
    }  // namespace

    struct AppContext::PlatformState {
        PlatformState() { Sdl::Startup(); }
        ~PlatformState() { Sdl::Shutdown(); }

        PlatformState(const PlatformState&)            = delete;
        PlatformState& operator=(const PlatformState&) = delete;

        bool QuitRequested = false;
        int ExitCode       = 0;
    };

    void AppContext::PlatformStateDeleter::operator()(PlatformState* State) const {
        delete State;
    }

    AppContext::PlatformState* AppContext::CreatePlatformState() {
        return new PlatformState();
    }

    void* AppContext::CreateDispatcher() {
        return _Platform.get();
    }

    void AppContext::DestroyDispatcher() {}

    void AppContext::WakeDispatcher(void*) {
        Sdl::Wake();
    }

    int AppContext::Run() {
        if (_Windows.empty()) { return 0; }
        _Platform->QuitRequested = false;

        while (true) {
            const bool FrameDue = std::any_of(_Windows.begin(), _Windows.end(), [](const WindowCore* Entry) {
                return Entry->HasPendingFrame();
            });
            int32_t Timeout = FrameDue ? 0 : AnimationTicker::Get().HasActive() ? kAnimationWaitMilliseconds : -1;
            if (const std::optional<double> NextDue = TimerQueue::Get().GetNextDue()) {
                const double Remaining = std::max(0.0, *NextDue - TimerQueue::Get().Now());
                const auto Due = static_cast<int32_t>(std::min(std::ceil(Remaining * 1000.0), kMaxWaitMilliseconds));
                Timeout        = Timeout < 0 ? Due : std::min(Timeout, Due);
            }

            Sdl::PumpEvents(Timeout, true);
            RethrowPendingException();
            if (_Platform->QuitRequested) { return _Platform->ExitCode; }

            AnimationTicker::Get().Tick();
            RenderNextReadyWindow();
            RethrowPendingException();
            TimerQueue::Get().RunDue();
            RethrowPendingException();
            if (_Platform->QuitRequested) { return _Platform->ExitCode; }
        }
    }

    void AppContext::RenderReadyWindows() {
        AnimationTicker::Get().Tick();
        RenderNextReadyWindow();
        TimerQueue::Get().RunDue();
    }

    void AppContext::Quit(int ExitCode) {
        _Platform->QuitRequested = true;
        _Platform->ExitCode      = ExitCode;
        Sdl::Wake();
    }

    // Swaps are paced by the swap interval, so every window with a frame due renders now rather than one at a time.
    void AppContext::RenderNextReadyWindow() {
        const std::vector<WindowCore*> Windows = _Windows;
        for (WindowCore* Core : Windows) {
            // A frame callback may close other windows.
            if (std::find(_Windows.begin(), _Windows.end(), Core) == _Windows.end()) { continue; }
            if (Core->HasPendingFrame()) { Core->RenderFrame(); }
        }
    }
}  // namespace XenUI
