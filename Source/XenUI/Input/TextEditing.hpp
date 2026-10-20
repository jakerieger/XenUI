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

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace XenUI {
    /// @brief A selection as UTF-8 byte offsets. Anchor stays put while Caret moves when a selection is extended.
    struct TextSelection {
        size_t Anchor = 0;
        size_t Caret  = 0;

        [[nodiscard]] size_t GetStart() const { return Anchor < Caret ? Anchor : Caret; }
        [[nodiscard]] size_t GetEnd() const { return Anchor < Caret ? Caret : Anchor; }
        [[nodiscard]] bool IsCollapsed() const { return Anchor == Caret; }

        bool operator==(const TextSelection&) const = default;
    };

    enum class TextUnit : uint8_t { Character, Word, Line };

    /// @brief Editing logic for a single block of UTF-8 text: insertion, deletion, caret movement, selection and
    /// undo, with no rendering. Typing and deleting in a run are grouped into one undo step.
    class XENUI_API TextEditor {
    public:
        static constexpr size_t kUndoLimit = 200;

        explicit TextEditor(std::string Initial = {});

        [[nodiscard]] const std::string& GetText() const { return _Text; }
        [[nodiscard]] const TextSelection& GetSelection() const { return _Selection; }
        [[nodiscard]] std::string GetSelectedText() const;

        /// @brief Replaces the text, puts the caret at the end and clears undo history.
        void SetText(std::string Text);
        void SetSelection(TextSelection Selection);

        /// @brief Caret positions from the text layout, so the caret never splits a cluster. When empty, every
        /// code point boundary is a stop.
        void SetCaretStops(std::vector<size_t> Stops);

        /// @brief Limit in code points; 0 for none.
        void SetMaxLength(size_t MaxLength) { _MaxLength = MaxLength; }
        /// @brief Strips line breaks from inserted text and turns them into spaces.
        void SetSingleLine(bool SingleLine) { _SingleLine = SingleLine; }

        /// @brief Replaces the selection with Text. Returns whether the text changed.
        bool Insert(std::string_view Text);
        /// @brief Deletes the selection, or one unit before or after the caret. Returns whether the text changed.
        bool Delete(TextUnit Unit, bool Forward);

        void Move(TextUnit Unit, bool Forward, bool Extend);
        void SelectAll();
        /// @brief Selects the word or run of spaces or punctuation around Offset.
        void SelectWord(size_t Offset);

        [[nodiscard]] bool CanUndo() const { return !_Undo.empty(); }
        [[nodiscard]] bool CanRedo() const { return !_Redo.empty(); }
        bool Undo();
        bool Redo();
        /// @brief Ends the current undo group, so the next edit starts a new step.
        void BreakUndoGroup() { _Group = EditKind::None; }

    private:
        enum class EditKind : uint8_t { None, Typing, Deleting, Other };

        struct Snapshot {
            std::string Text;
            TextSelection Selection;
        };

        [[nodiscard]] size_t Snap(size_t Offset) const;
        [[nodiscard]] size_t StepCharacter(size_t Offset, bool Forward) const;
        [[nodiscard]] size_t StepWord(size_t Offset, bool Forward) const;
        [[nodiscard]] size_t Step(size_t Offset, TextUnit Unit, bool Forward) const;
        void Record(EditKind Kind);
        void Replace(size_t Start, size_t End, std::string_view With);

        std::string _Text;
        TextSelection _Selection;
        std::vector<size_t> _Stops;
        std::vector<Snapshot> _Undo;
        std::vector<Snapshot> _Redo;
        EditKind _Group   = EditKind::None;
        size_t _MaxLength = 0;
        bool _SingleLine  = true;
    };

    /// @brief Number of code points in UTF-8 text.
    [[nodiscard]] XENUI_API size_t CountCodePoints(std::string_view Text);
}  // namespace XenUI
