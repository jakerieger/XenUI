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
#include "XenUI/Core/Color.hpp"
#include "XenUI/Tree/Widget.hpp"
#include "XenUI/Widgets/MenuBar.hpp"

#include <optional>
#include <string>
#include <vector>

namespace XenUI {
    /// @brief A complete title bar for a window created with CustomTitleBar: menus on the left, the title, your own
    /// widgets on the right and the minimize, maximize and close buttons. With menus, the title is centered in the
    /// bar; without them, it sits on the left. The whole bar drags the window, while the menus and actions still
    /// work normally. Place it at the top of the window's content.
    struct XENUI_API TitleBar {
        std::string Title;
        /// @brief Menus shown on the left, such as File, Edit and View.
        std::vector<Menu> Menus;
        /// @brief Widgets on the right, before the window controls, such as a theme selector or a search field.
        NodeList Actions;
        float Height    = 34.0f;
        float MenuWidth = 220.0f;
        /// @brief The bar's color; the theme's chrome color when unset.
        std::optional<Color> Background;

        [[nodiscard]] Node Build(BuildContext& Context) const;
    };
}  // namespace XenUI
