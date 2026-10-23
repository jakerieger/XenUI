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
#include "XenUI/Render/RenderScroll.hpp"
#include "XenUI/Tree/Widget.hpp"

#include <functional>
#include <set>
#include <string>
#include <vector>

namespace XenUI {
    struct TreeItem {
        /// @brief Identifies the item across rebuilds; unique within the tree.
        std::string Id;
        std::string Label;
        bool HasChildren = false;
    };

    /// @brief An expandable hierarchy, such as folders and files. Items are supplied on demand by ChildrenOf, which
    /// is only asked for the children of expanded items, and rows are virtualized, so very large trees stay fast.
    /// Signals read inside ChildrenOf are tracked, so the tree updates when its data changes. When the selection
    /// changes, including from outside through Selected, the tree scrolls to show it.
    ///
    /// Click to select and double-click to activate. With the tree focused, Up and Down move the selection, Right
    /// expands or moves to the first child, Left collapses or moves to the parent, Home and End jump to the ends,
    /// and Enter activates.
    struct TreeView {
        /// @brief Returns the children of the item with this id, or the top-level items for an empty id.
        std::function<std::vector<TreeItem>(const std::string& Parent)> ChildrenOf;
        /// @brief Optional external selection (an item id, empty for none); otherwise kept internally.
        Signal<std::string>* Selected = nullptr;
        /// @brief Optional external set of expanded item ids; otherwise kept internally.
        Signal<std::set<std::string>>* Expanded = nullptr;
        std::function<void(const std::string& Id)> OnSelected;
        /// @brief Called on double-click or Enter.
        std::function<void(const std::string& Id)> OnActivated;
        /// @brief Custom row content after the expand arrow; the label as text when unset.
        std::function<Node(const TreeItem& Item)> RowContent;
        float RowHeight              = 24.0f;
        float Indent                 = 16.0f;
        ScrollController* Controller = nullptr;
        /// @brief The name screen readers announce.
        std::string Label;

        class XENUI_API State : public ComponentState<TreeView> {
        public:
            struct Row {
                TreeItem Item;
                int Depth = 0;
                std::string Parent;
            };

            /// @internal
            [[nodiscard]] const std::vector<Row>& GetRows() const { return _Rows; }
            /// @internal
            [[nodiscard]] Signal<std::string>& GetSelection();
            /// @internal
            [[nodiscard]] Signal<std::set<std::string>>& GetExpansion();
            /// @internal
            void Select(const std::string& Id, bool Reveal);
            /// @internal
            void SetExpanded(const std::string& Id, bool Open);
            /// @internal
            void Activate(const std::string& Id);
            /// @internal
            void FocusTree() { _Focus.RequestFocus(); }

        protected:
            Node Build(BuildContext& Context) override;

        private:
            void Flatten(const std::string& Parent, int Depth, const std::set<std::string>& Open);
            [[nodiscard]] int IndexOf(const std::string& Id) const;
            bool HandleKey(const KeyEvent& Event);

            std::vector<Row> _Rows;
            Signal<std::string> _Selected;
            Signal<std::set<std::string>> _Expanded;
            ScrollController _Scroll;
            FocusNode _Focus;
            std::string _Revealed;
            Timer _Reveal;
        };
    };
}  // namespace XenUI
