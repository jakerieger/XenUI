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

#include "XenUI/Input/TextEditing.hpp"

#include <algorithm>

namespace XenUI {
    namespace {
        enum class CharacterClass : uint8_t { Space, Word, Punctuation };

        bool IsContinuation(char Byte) {
            return (static_cast<unsigned char>(Byte) & 0xC0u) == 0x80u;
        }

        size_t NextCodePoint(const std::string& Text, size_t Offset) {
            if (Offset >= Text.size()) { return Text.size(); }
            ++Offset;
            while (Offset < Text.size() && IsContinuation(Text[Offset])) {
                ++Offset;
            }
            return Offset;
        }

        size_t PreviousCodePoint(const std::string& Text, size_t Offset) {
            if (Offset == 0) { return 0; }
            --Offset;
            while (Offset > 0 && IsContinuation(Text[Offset])) {
                --Offset;
            }
            return Offset;
        }

        char32_t DecodeAt(const std::string& Text, size_t Offset) {
            const auto Lead = static_cast<unsigned char>(Text[Offset]);
            if (Lead < 0x80) { return Lead; }
            const size_t End = NextCodePoint(Text, Offset);
            char32_t Value   = Lead & (Lead >= 0xF0 ? 0x07u : Lead >= 0xE0 ? 0x0Fu : 0x1Fu);
            for (size_t i = Offset + 1; i < End; ++i) {
                Value = (Value << 6) | (static_cast<unsigned char>(Text[i]) & 0x3Fu);
            }
            return Value;
        }

        CharacterClass Classify(char32_t Value) {
            if (Value == U' ' || Value == U'\t' || Value == U'\n' || Value == 0xA0 || Value == 0x3000) {
                return CharacterClass::Space;
            }
            const bool Alphanumeric =
              (Value >= U'0' && Value <= U'9') || (Value >= U'a' && Value <= U'z') || (Value >= U'A' && Value <= U'Z');
            return Alphanumeric || Value == U'_' || Value >= 0x80 ? CharacterClass::Word : CharacterClass::Punctuation;
        }

        std::string Sanitize(std::string_view Text, bool SingleLine) {
            std::string Result;
            Result.reserve(Text.size());
            for (size_t i = 0; i < Text.size(); ++i) {
                const char Byte = Text[i];
                if (Byte == '\r') {
                    if (i + 1 < Text.size() && Text[i + 1] == '\n') { continue; }
                    Result.push_back(SingleLine ? ' ' : '\n');
                } else if (Byte == '\n') {
                    Result.push_back(SingleLine ? ' ' : '\n');
                } else if (static_cast<unsigned char>(Byte) >= 0x20 || Byte == '\t') {
                    Result.push_back(Byte);
                }
            }
            return Result;
        }

        /// @brief The longest prefix of Text holding at most Limit code points.
        size_t PrefixBytes(std::string_view Text, size_t Limit) {
            size_t Count = 0;
            for (size_t i = 0; i < Text.size(); ++i) {
                if (!IsContinuation(Text[i])) {
                    if (Count == Limit) { return i; }
                    ++Count;
                }
            }
            return Text.size();
        }
    }  // namespace

    size_t CountCodePoints(std::string_view Text) {
        return static_cast<size_t>(
          std::count_if(Text.begin(), Text.end(), [](char Byte) { return !IsContinuation(Byte); }));
    }

    TextEditor::TextEditor(std::string Initial) {
        SetText(std::move(Initial));
    }

    std::string TextEditor::GetSelectedText() const {
        return _Text.substr(_Selection.GetStart(), _Selection.GetEnd() - _Selection.GetStart());
    }

    void TextEditor::SetText(std::string Text) {
        _Text = Sanitize(Text, _SingleLine);
        _Stops.clear();
        _Selection = {_Text.size(), _Text.size()};
        _Undo.clear();
        _Redo.clear();
        _Group = EditKind::None;
    }

    void TextEditor::SetSelection(TextSelection Selection) {
        _Selection = {Snap(Selection.Anchor), Snap(Selection.Caret)};
        BreakUndoGroup();
    }

    void TextEditor::SetCaretStops(std::vector<size_t> Stops) {
        _Stops = std::move(Stops);
        if (!_Stops.empty() && _Stops.back() != _Text.size()) { _Stops.clear(); }
        _Selection = {Snap(_Selection.Anchor), Snap(_Selection.Caret)};
    }

    size_t TextEditor::Snap(size_t Offset) const {
        Offset = std::min(Offset, _Text.size());
        if (_Stops.empty()) {
            while (Offset > 0 && Offset < _Text.size() && IsContinuation(_Text[Offset])) {
                --Offset;
            }
            return Offset;
        }
        const auto Above = std::upper_bound(_Stops.begin(), _Stops.end(), Offset);
        return Above == _Stops.begin() ? 0 : *(Above - 1);
    }

    size_t TextEditor::StepCharacter(size_t Offset, bool Forward) const {
        if (_Stops.empty()) { return Forward ? NextCodePoint(_Text, Offset) : PreviousCodePoint(_Text, Offset); }
        if (Forward) {
            const auto Next = std::upper_bound(_Stops.begin(), _Stops.end(), Offset);
            return Next == _Stops.end() ? _Text.size() : *Next;
        }
        const auto Next = std::lower_bound(_Stops.begin(), _Stops.end(), Offset);
        return Next == _Stops.begin() ? 0 : *(Next - 1);
    }

    size_t TextEditor::StepWord(size_t Offset, bool Forward) const {
        if (Forward) {
            if (Offset >= _Text.size()) { return _Text.size(); }
            const CharacterClass Start = Classify(DecodeAt(_Text, Offset));
            if (Start != CharacterClass::Space) {
                while (Offset < _Text.size() && Classify(DecodeAt(_Text, Offset)) == Start) {
                    Offset = NextCodePoint(_Text, Offset);
                }
            }
            while (Offset < _Text.size() && Classify(DecodeAt(_Text, Offset)) == CharacterClass::Space) {
                Offset = NextCodePoint(_Text, Offset);
            }
            return Snap(Offset);
        }
        while (Offset > 0 && Classify(DecodeAt(_Text, PreviousCodePoint(_Text, Offset))) == CharacterClass::Space) {
            Offset = PreviousCodePoint(_Text, Offset);
        }
        if (Offset == 0) { return 0; }
        const CharacterClass Start = Classify(DecodeAt(_Text, PreviousCodePoint(_Text, Offset)));
        while (Offset > 0 && Classify(DecodeAt(_Text, PreviousCodePoint(_Text, Offset))) == Start) {
            Offset = PreviousCodePoint(_Text, Offset);
        }
        return Snap(Offset);
    }

    size_t TextEditor::Step(size_t Offset, TextUnit Unit, bool Forward) const {
        switch (Unit) {
            case TextUnit::Character:
                return StepCharacter(Offset, Forward);
            case TextUnit::Word:
                return StepWord(Offset, Forward);
            case TextUnit::Line:
                return Forward ? _Text.size() : 0;
        }
        return Offset;
    }

    void TextEditor::Record(EditKind Kind) {
        if (_Group != Kind || Kind == EditKind::Other) {
            _Undo.push_back({_Text, _Selection});
            if (_Undo.size() > kUndoLimit) { _Undo.erase(_Undo.begin()); }
        }
        _Redo.clear();
        _Group = Kind;
    }

    void TextEditor::Replace(size_t Start, size_t End, std::string_view With) {
        _Text.replace(Start, End - Start, With);
        _Stops.clear();
        const size_t Caret = Start + With.size();
        _Selection         = {Caret, Caret};
    }

    bool TextEditor::Insert(std::string_view Text) {
        std::string Clean  = Sanitize(Text, _SingleLine);
        const size_t Start = _Selection.GetStart();
        const size_t End   = _Selection.GetEnd();
        if (_MaxLength > 0) {
            const size_t Kept =
              CountCodePoints(_Text) - CountCodePoints(std::string_view(_Text).substr(Start, End - Start));
            const size_t Space = _MaxLength > Kept ? _MaxLength - Kept : 0;
            Clean.resize(PrefixBytes(Clean, Space));
        }
        if (Clean.empty() && Start == End) { return false; }

        const bool Typing = Start == End && CountCodePoints(Clean) == 1;
        if (Typing && Classify(DecodeAt(Clean, 0)) == CharacterClass::Space) { BreakUndoGroup(); }
        Record(Typing ? EditKind::Typing : EditKind::Other);
        Replace(Start, End, Clean);
        if (!Typing) { BreakUndoGroup(); }
        return true;
    }

    bool TextEditor::Delete(TextUnit Unit, bool Forward) {
        size_t Start = _Selection.GetStart();
        size_t End   = _Selection.GetEnd();
        if (Start == End) {
            const size_t Target = Step(_Selection.Caret, Unit, Forward);
            Start               = std::min(Target, _Selection.Caret);
            End                 = std::max(Target, _Selection.Caret);
        }
        if (Start == End) { return false; }
        Record(_Selection.IsCollapsed() && Unit == TextUnit::Character ? EditKind::Deleting : EditKind::Other);
        Replace(Start, End, {});
        return true;
    }

    void TextEditor::Move(TextUnit Unit, bool Forward, bool Extend) {
        BreakUndoGroup();
        if (!Extend && !_Selection.IsCollapsed() && Unit == TextUnit::Character) {
            const size_t Edge = Forward ? _Selection.GetEnd() : _Selection.GetStart();
            _Selection        = {Edge, Edge};
            return;
        }
        const size_t Caret = Step(_Selection.Caret, Unit, Forward);
        _Selection         = {Extend ? _Selection.Anchor : Caret, Caret};
    }

    void TextEditor::SelectAll() {
        BreakUndoGroup();
        _Selection = {0, _Text.size()};
    }

    void TextEditor::SelectWord(size_t Offset) {
        BreakUndoGroup();
        Offset = Snap(Offset);
        if (_Text.empty()) {
            _Selection = {};
            return;
        }
        const size_t Probe          = Offset < _Text.size() ? Offset : PreviousCodePoint(_Text, Offset);
        const CharacterClass Target = Classify(DecodeAt(_Text, Probe));
        size_t Start                = Probe;
        size_t End                  = NextCodePoint(_Text, Probe);
        while (Start > 0 && Classify(DecodeAt(_Text, PreviousCodePoint(_Text, Start))) == Target) {
            Start = PreviousCodePoint(_Text, Start);
        }
        while (End < _Text.size() && Classify(DecodeAt(_Text, End)) == Target) {
            End = NextCodePoint(_Text, End);
        }
        _Selection = {Snap(Start), Snap(End) == Snap(Start) ? End : Snap(End)};
    }

    bool TextEditor::Undo() {
        if (_Undo.empty()) { return false; }
        _Redo.push_back({_Text, _Selection});
        Snapshot Previous = std::move(_Undo.back());
        _Undo.pop_back();
        _Text      = std::move(Previous.Text);
        _Selection = Previous.Selection;
        _Stops.clear();
        _Group = EditKind::None;
        return true;
    }

    bool TextEditor::Redo() {
        if (_Redo.empty()) { return false; }
        _Undo.push_back({_Text, _Selection});
        Snapshot Next = std::move(_Redo.back());
        _Redo.pop_back();
        _Text      = std::move(Next.Text);
        _Selection = Next.Selection;
        _Stops.clear();
        _Group = EditKind::None;
        return true;
    }
}  // namespace XenUI
