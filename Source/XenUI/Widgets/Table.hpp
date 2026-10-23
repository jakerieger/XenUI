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
#include "XenUI/Graphics/Text.hpp"
#include "XenUI/Input/Focus.hpp"
#include "XenUI/Reactive/Signal.hpp"
#include "XenUI/Render/RenderScroll.hpp"
#include "XenUI/Tree/Widget.hpp"

#include <functional>
#include <string>
#include <vector>

namespace XenUI {
    struct TableColumn {
        std::string Title;
        /// @brief Starting width. The last column always fills the space left.
        float Width             = 120.0f;
        float MinWidth          = 40.0f;
        TextAlignment Alignment = TextAlignment::Leading;
        /// @brief Whether clicking the header asks to sort by this column.
        bool Sortable = true;
    };

    struct TableSort {
        /// @brief The sorted column, or -1 for none.
        int Column     = -1;
        bool Ascending = true;

        bool operator==(const TableSort&) const = default;
    };

    /// @brief Rows of data in resizable columns. Rows are virtualized, so tables with many thousands of rows stay
    /// fast; every row has the same height. Drag a header divider to resize a column. Clicking a sortable header
    /// sets the sort indicator and calls OnSort: the table does not reorder data itself, so sort your rows and have
    /// CellText read them in that order.
    ///
    /// Click a row to select it and double-click to activate it. With the table focused, Up, Down, Page Up,
    /// Page Down, Home and End move the selection and Enter activates. Give the table a bounded height.
    struct Table {
        std::vector<TableColumn> Columns;
        int RowCount = 0;
        /// @brief Text of a cell.
        std::function<std::string(int Row, int Column)> CellText;
        /// @brief Custom cell content; takes precedence over CellText when set.
        std::function<Node(int Row, int Column)> CellBuilder;
        /// @brief Optional external selection (a row index, -1 for none); otherwise kept internally.
        Signal<int>* Selected = nullptr;
        std::function<void(int Row)> OnSelected;
        /// @brief Called on double-click or Enter.
        std::function<void(int Row)> OnActivated;
        /// @brief Optional external sort state; otherwise kept internally.
        Signal<TableSort>* Sort = nullptr;
        std::function<void(const TableSort& Sort)> OnSort;
        float RowHeight              = 26.0f;
        float HeaderHeight           = 28.0f;
        ScrollController* Controller = nullptr;
        /// @brief The name screen readers announce.
        std::string Label;

        class XENUI_API State : public ComponentState<Table> {
        public:
            /// @internal
            [[nodiscard]] Signal<int>& GetSelection();
            /// @internal
            [[nodiscard]] float GetWidth(size_t Column) const;
            /// @internal
            void Select(int Row, bool Reveal);
            /// @internal
            void Activate(int Row);
            /// @internal
            void FocusTable() { _Focus.RequestFocus(); }
            /// @internal
            void ToggleSort(int Column);
            /// @internal
            void Resize(size_t Column, float Delta);
            /// @internal
            [[nodiscard]] const std::vector<float>& ReadWidths() const { return _Widths.Get(); }

        protected:
            void Init() override;
            void WidgetUpdated(const Table& Previous) override;
            Node Build(BuildContext& Context) override;

        private:
            [[nodiscard]] Signal<TableSort>& GetSort();
            bool HandleKey(const KeyEvent& Event);
            void ResetWidths();

            Signal<std::vector<float>> _Widths;
            Signal<int> _Selected = -1;
            Signal<TableSort> _Sort;
            ScrollController _Scroll;
            FocusNode _Focus;
        };
    };
}  // namespace XenUI
