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

#include "XenUI/Input/Shortcuts.hpp"

#include <algorithm>

namespace XenUI {
    void ShortcutMatcher::SetShortcuts(std::vector<Shortcut> Shortcuts) {
        _Shortcuts = std::move(Shortcuts);
        _Prefix.clear();
    }

    bool ShortcutMatcher::HandleKey(const KeyEvent& Event) {
        if (!Event.Pressed || IsModifierKey(Event.Code)) { return false; }
        if (!_Prefix.empty() && Event.Time - _PrefixTime > kChordTimeout) { _Prefix.clear(); }

        if (TryMatch(Event)) { return true; }
        if (_Prefix.empty()) { return false; }

        _Prefix.clear();
        return TryMatch(Event);
    }

    bool ShortcutMatcher::TryMatch(const KeyEvent& Event) {
        const size_t Step = _Prefix.size();
        bool Advances     = false;

        for (const Shortcut& Candidate : _Shortcuts) {
            if (Candidate.Keys.size() <= Step || !std::equal(_Prefix.begin(), _Prefix.end(), Candidate.Keys.begin())) {
                continue;
            }
            if (!Candidate.Keys[Step].Matches(Event)) { continue; }

            if (Candidate.Keys.size() == Step + 1) {
                _Prefix.clear();
                const std::function<void()> Action = Candidate.Action;
                if (Action) { Action(); }
                return true;
            }
            Advances = true;
        }

        if (!Advances) { return false; }
        _Prefix.push_back({Event.Code, Event.Modifiers});
        _PrefixTime = Event.Time;
        return true;
    }
}  // namespace XenUI
