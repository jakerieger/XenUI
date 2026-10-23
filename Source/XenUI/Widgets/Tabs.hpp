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
#include "XenUI/Input/Focus.hpp"
#include "XenUI/Reactive/Signal.hpp"
#include "XenUI/Tree/Widget.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace XenUI {
    struct Tab {
        std::string Label;
        Node Content;
    };

    /// @brief A row of tabs above the content of the selected one. Only the selected tab's content is built.
    /// Switch tabs by clicking, with Left and Right while a tab has focus, or with Ctrl+Tab and Ctrl+Shift+Tab from
    /// anywhere inside. In a column with bounded height the content fills the space left under the tabs.
    struct TabView {
        std::vector<Tab> Tabs;
        /// @brief Optional external selection; otherwise it starts at InitialIndex and is kept internally.
        Signal<int>* Selected = nullptr;
        int InitialIndex      = 0;
        std::function<void(int)> OnChanged;

        class XENUI_API State : public ComponentState<TabView> {
        protected:
            void Init() override;
            Node Build(BuildContext& Context) override;

        private:
            [[nodiscard]] Signal<int>& GetSelection();
            void Choose(int Index, bool MoveFocus);

            Signal<int> _Selected = 0;
            std::vector<std::unique_ptr<FocusNode>> _Focus;
        };
    };
}  // namespace XenUI
