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
#include "XenUI/Core/Timer.hpp"
#include "XenUI/Input/Focus.hpp"
#include "XenUI/Reactive/Signal.hpp"
#include "XenUI/Tree/Widget.hpp"
#include "XenUI/Widgets/Controls.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace XenUI {
    /// @brief One top-level menu of a MenuBar.
    struct Menu {
        std::string Label;
        std::vector<MenuItem> Items;
    };

    /// @brief A row of menus, such as File, Edit and View. Click a menu, or press Enter, Space or Down on a focused
    /// one, to open it. While a menu is open, hovering another menu or pressing Left and Right switches to it. Like
    /// native menu bars, clicking a menu does not take keyboard focus from the focused control; Tab reaches the menus.
    /// Put it in a custom title bar or along the top of the window.
    struct MenuBar {
        std::vector<Menu> Menus;
        float Height    = 28.0f;
        float MenuWidth = 220.0f;

        class XENUI_API State : public ComponentState<MenuBar> {
        public:
            /// @internal Bounds of each menu's button, in window coordinates; filled in by the buttons.
            std::vector<std::function<Rect()>> Anchors;

        protected:
            Node Build(BuildContext& Context) override;
            void Dispose() override;

        private:
            void Open(int Index);
            void Close();
            void Activate(int Index);
            void Hover(int Index);
            void Step(int Index, int Direction);

            OverlayController* _Overlay = nullptr;
            PopupStyle _Style;
            OverlayEntry _Entry;
            Signal<int> _Open = -1;
            int _JustClosed   = -1;
            Timer _CloseGuard;
            std::vector<std::unique_ptr<FocusNode>> _Focus;
        };
    };
}  // namespace XenUI
