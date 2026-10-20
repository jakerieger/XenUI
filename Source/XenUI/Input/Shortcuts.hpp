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
#include "XenUI/Input/Events.hpp"

#include <functional>
#include <vector>

namespace XenUI {
    /// @brief An action triggered by a key combination, or by a sequence of them pressed in turn (a chord,
    /// such as Ctrl+K followed by Ctrl+C).
    struct Shortcut {
        std::vector<KeyCombination> Keys;
        std::function<void()> Action;
        /// @brief Runs before the focused widget sees the key, so a focused button or list cannot take it, as with
        /// Space for a transport. Text fields still receive keys that type characters.
        bool Preempts = false;
    };

    class XENUI_API ShortcutMatcher {
    public:
        /// @brief Seconds allowed between the steps of a chord before it is abandoned.
        static constexpr double kChordTimeout = 1.5;

        void SetShortcuts(std::vector<Shortcut> Shortcuts);

        /// @return true if the key completed a shortcut or advanced a chord in progress.
        bool HandleKey(const KeyEvent& Event);

        [[nodiscard]] bool IsChordPending() const { return !_Prefix.empty(); }

    private:
        bool TryMatch(const KeyEvent& Event);

        std::vector<Shortcut> _Shortcuts;
        std::vector<KeyCombination> _Prefix;
        double _PrefixTime = 0.0;
    };
}  // namespace XenUI
