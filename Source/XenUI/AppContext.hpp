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

#pragma once

#include "XenUI/Graphics/GraphicsDevice.hpp"

#include <cstdint>
#include <exception>
#include <vector>

namespace XenUI {
    class WindowImpl;

    class ComApartment {
    public:
        ComApartment();
        ~ComApartment();

        ComApartment(const ComApartment&)            = delete;
        ComApartment& operator=(const ComApartment&) = delete;

    private:
        bool _Initialized = false;
    };

    class AppContext {
    public:
        AppContext();
        ~AppContext();

        AppContext(const AppContext&)            = delete;
        AppContext& operator=(const AppContext&) = delete;

        /// @throws Error if no App exists.
        static AppContext& Get();
        static AppContext* TryGet();

        /// @brief Pumps messages and renders requested frames, paced by each window's swapchain. Sleeps when idle.
        int Run();
        void Quit(int ExitCode);

        /// @brief Renders every window whose next frame is due without waiting, and runs due timers. Keeps the UI
        /// alive inside modal loops that the run loop does not see, such as a system drag.
        void RenderReadyWindows();

        void OnWindowOpened(WindowImpl& Opened);
        void OnWindowClosed(WindowImpl& Closed);

        /// @brief Rebuilds the GPU device and every window's swapchain after the device was removed or reset.
        void HandleDeviceLost();

        /// @brief Defers an exception caught inside a window procedure so Run can rethrow it outside of user32 frames.
        void ReportException(std::exception_ptr Exception);

        /// @brief Seconds since the App was created.
        [[nodiscard]] double GetTime() const;

        [[nodiscard]] GraphicsDevice& GetGraphicsDevice() { return _GraphicsDevice; }

    private:
        void RethrowPendingException();
        void RenderNextReadyWindow();

        ComApartment _ComApartment;
        GraphicsDevice _GraphicsDevice;
        std::vector<WindowImpl*> _Windows;
        std::exception_ptr _PendingException;
        int64_t _ClockStart = 0;
        double _ClockPeriod = 0.0;
    };
}  // namespace XenUI
