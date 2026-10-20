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

#include <Windows.h>

#include <memory>
#include <optional>

namespace XenUI {
    class WindowImpl;
    struct AccessibilityCore;

    /// @internal Serves a window's accessibility tree to UI Automation, so screen readers such as Narrator can
    /// read and operate its widgets. Does nothing until a client asks for the window's provider.
    class AccessibilityBridge {
    public:
        explicit AccessibilityBridge(WindowImpl& Window);
        ~AccessibilityBridge();

        AccessibilityBridge(const AccessibilityBridge&)            = delete;
        AccessibilityBridge& operator=(const AccessibilityBridge&) = delete;

        /// @brief Answers WM_GETOBJECT for the UI Automation root.
        [[nodiscard]] std::optional<LRESULT> HandleGetObject(WPARAM WParam, LPARAM LParam);

        /// @brief Called after the window's content was rebuilt and laid out. Refreshes the tree and raises
        /// events when clients are listening.
        void ContentUpdated();

        /// @brief Releases every provider, before the window is destroyed.
        void Disconnect();

    private:
        std::shared_ptr<AccessibilityCore> _Core;
    };
}  // namespace XenUI
