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

#include <filesystem>
#include <string_view>

namespace XenUI {
    /// @brief Hands URLs, files and folders to the Windows shell.
    class XENUI_API Shell {
    public:
        /// @brief Opens a URL in the default browser, or a file or folder with its default handler.
        static bool Open(std::string_view Target);
        /// @brief Opens an Explorer window on the folder containing Item, with Item selected.
        static bool Reveal(const std::filesystem::path& Item);
    };
}  // namespace XenUI
