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

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace XenUI {
    /// @brief The system clipboard. Text is UTF-8 with \n line endings; custom formats carry raw bytes under a
    /// name shared by every application that registers it. UI thread only.
    class XENUI_API Clipboard {
    public:
        static bool SetText(std::string_view Text);
        [[nodiscard]] static std::optional<std::string> GetText();
        [[nodiscard]] static bool HasText();

        /// @brief Replaces the clipboard with Data in a custom format, plus an optional plain-text version for
        /// other applications.
        static bool SetData(std::string_view Format,
                            std::span<const uint8_t> Data,
                            std::optional<std::string_view> Text = std::nullopt);
        [[nodiscard]] static std::optional<std::vector<uint8_t>> GetData(std::string_view Format);
        [[nodiscard]] static bool HasData(std::string_view Format);
    };
}  // namespace XenUI
