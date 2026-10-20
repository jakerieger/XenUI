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
#include "XenUI/Core/Math.hpp"

#include <cstddef>
#include <string_view>

namespace XenUI {
    /// @brief Something that edits text, as seen by the platform's input methods. A Focus widget exposes one while
    /// it has focus; the window then shows input-method compositions inline through it and places candidate
    /// lists at its caret. Committed text arrives as ordinary text events.
    class XENUI_API TextInputClient {
    public:
        virtual ~TextInputClient();

        /// @brief The caret in window coordinates (device-independent pixels).
        [[nodiscard]] virtual Rect GetCaretBounds() const = 0;

        /// @brief Text being composed, to show at the selection, with the composition's caret as a UTF-8 offset
        /// into it. An empty Text ends the composition.
        virtual void UpdateComposition(std::string_view Text, size_t Caret) = 0;
    };
}  // namespace XenUI
