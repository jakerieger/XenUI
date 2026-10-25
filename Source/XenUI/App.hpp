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

#include "XenUI/Api.hpp"

#include <functional>
#include <memory>

namespace XenUI {
    class AppContext;

    /// @brief Process-wide application object. Exactly one may exist, and it must outlive every Window.
    class XENUI_API App {
    public:
        App();
        ~App();

        App(const App&)            = delete;
        App& operator=(const App&) = delete;
        App(App&&)                 = delete;
        App& operator=(App&&)      = delete;

        /// @brief Runs the message loop until every window has closed or Quit is called.
        /// @return The exit code passed to Quit, or 0.
        int Run();

        void Quit(int ExitCode = 0);

        /// @brief Runs Work on the UI thread, soon after the call. Safe to call from any thread, which makes it the way
        /// for worker threads to update signals and widgets. Posted work runs in the order it was posted, also while a
        /// modal loop such as a file dialog is open. Work that captures objects must not outlive them.
        /// @return False when no App exists, in which case Work is dropped.
        static bool Post(std::function<void()> Work);

    private:
        std::unique_ptr<AppContext> _Context;
    };
}  // namespace XenUI
