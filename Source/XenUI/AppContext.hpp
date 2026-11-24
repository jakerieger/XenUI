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

#include <chrono>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <vector>

namespace XenUI {
    class WindowCore;

    /// @brief Process-wide state behind App: the graphics device, the open windows and the run loop.
    ///
    /// The window list, posted work, exceptions and clock are shared. The run loop and the dispatcher that wakes
    /// it are per platform, in Platform/<Platform>/AppContext.cpp.
    class AppContext {
    public:
        AppContext();
        ~AppContext();

        AppContext(const AppContext&)            = delete;
        AppContext& operator=(const AppContext&) = delete;

        static AppContext& Get();
        static AppContext* TryGet();

        /// @brief Runs the message loop until Quit or the last window closes. Returns the exit code.
        int Run();
        void Quit(int ExitCode);

        /// @brief Renders every window whose next frame is due, without waiting. For modal loops the platform runs.
        void RenderReadyWindows();

        void OnWindowOpened(WindowCore& Opened);
        void OnWindowClosed(WindowCore& Closed);

        /// @brief Rebuilds the graphics device and every window's swap chain after the GPU was lost.
        void HandleDeviceLost();

        /// @brief Stores an exception thrown from a callback so Run rethrows it on the UI thread.
        void ReportException(std::exception_ptr Exception);

        /// @brief Queues Work to run on the UI thread. Safe from any thread; returns false when no App can run it.
        static bool Post(std::function<void()> Work);
        void RunPosted();

        /// @brief Seconds since the App was created.
        [[nodiscard]] double GetTime() const;

        [[nodiscard]] GraphicsDevice& GetGraphicsDevice() { return _GraphicsDevice; }

    private:
        /// @brief Per-platform state, such as the COM apartment and dispatcher window on Windows.
        struct PlatformState;
        struct PlatformStateDeleter {
            void operator()(PlatformState* State) const;
        };

        // Implemented per platform.
        [[nodiscard]] static PlatformState* CreatePlatformState();
        /// @return The target that WakeDispatcher wakes.
        [[nodiscard]] void* CreateDispatcher();
        void DestroyDispatcher();
        static void WakeDispatcher(void* Target);
        void RenderNextReadyWindow();

        void RethrowPendingException();

        /// @note Declared before the graphics device, so platform setup (COM on Windows) happens before the device
        /// is created and is torn down after it.
        std::unique_ptr<PlatformState, PlatformStateDeleter> _Platform;
        GraphicsDevice _GraphicsDevice;
        std::vector<WindowCore*> _Windows;
        std::exception_ptr _PendingException;
        std::chrono::steady_clock::time_point _ClockStart;
    };
}  // namespace XenUI
