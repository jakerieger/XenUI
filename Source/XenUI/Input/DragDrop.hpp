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

#include "XenUI/Core/Math.hpp"
#include "XenUI/Input/Events.hpp"

#include <any>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace XenUI {
    /// @brief What a drop does with the dragged data. Combine with | to describe what a source allows.
    enum class DropEffect : uint8_t { None = 0, Copy = 1, Move = 2, Link = 4 };

    [[nodiscard]] constexpr DropEffect operator|(DropEffect Left, DropEffect Right) {
        return static_cast<DropEffect>(static_cast<uint8_t>(Left) | static_cast<uint8_t>(Right));
    }

    [[nodiscard]] constexpr DropEffect operator&(DropEffect Left, DropEffect Right) {
        return static_cast<DropEffect>(static_cast<uint8_t>(Left) & static_cast<uint8_t>(Right));
    }

    [[nodiscard]] constexpr bool Allows(DropEffect Set, DropEffect Effect) {
        return Effect != DropEffect::None && (Set & Effect) == Effect;
    }

    /// @brief The payload of a drag. Value carries any in-process object; Files and Text are what other
    /// applications see when the drag leaves the window, and what they provide when they drag into it.
    struct DragData {
        std::any Value;
        std::vector<std::filesystem::path> Files;
        std::optional<std::string> Text;
        /// @brief True when the data came from another application, so Value is empty.
        bool External = false;

        template<class T>
        [[nodiscard]] const T* Get() const {
            return std::any_cast<T>(&Value);
        }

        /// @brief Whether other applications can receive this data.
        [[nodiscard]] bool IsExportable() const { return !Files.empty() || Text.has_value(); }
    };

    struct DropEvent {
        const DragData* Data = nullptr;
        /// @brief Position in the receiving box's coordinate space.
        Vec2 LocalPosition;
        KeyModifiers Modifiers = KeyModifiers::None;
        /// @brief The effects the source allows.
        DropEffect Allowed = DropEffect::Copy;
    };

    /// @brief Returns the effect a drop here would have, or None to refuse the data. Called as the drag moves.
    using DropOverCallback = std::function<DropEffect(const DropEvent&)>;
    using DropCallback     = std::function<void(const DropEvent&)>;

    /// @brief Builds the payload when a drag starts, or returns nothing to refuse the drag.
    using DragDataCallback = std::function<std::optional<DragData>()>;
}  // namespace XenUI
