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

#include "XenUI/Input/TextEditingController.hpp"

#include <algorithm>

namespace XenUI {
    TextLayoutNavigator::~TextLayoutNavigator() = default;

    TextEditingController::TextEditingController(std::string Initial) : _Editor(std::move(Initial)) {}

    bool TextEditingController::MoveVertical(int Lines, bool Extend) {
        if (_Navigator == nullptr) { return false; }
        const TextSelection Current = _Editor.GetSelection();
        float Column                = _PreferredX;
        const size_t Target         = _Navigator->GetVerticalTarget(Current.Caret, Lines, Column);
        _Editor.SetSelection({Extend ? Current.Anchor : Target, Target});
        Notify(false);
        _PreferredX = Column;
        return true;
    }

    bool TextEditingController::MoveToLineEdge(bool End, bool Extend) {
        if (_Navigator == nullptr) { return false; }
        const TextSelection Current = _Editor.GetSelection();
        const size_t Target         = _Navigator->GetLineEdge(Current.Caret, End);
        _Editor.SetSelection({Extend ? Current.Anchor : Target, Target});
        Notify(false);
        return true;
    }

    const std::string& TextEditingController::GetText() const {
        (void)_Revision.Get();
        return _Editor.GetText();
    }

    TextSelection TextEditingController::GetSelection() const {
        (void)_Revision.Get();
        return _Editor.GetSelection();
    }

    const std::string& TextEditingController::GetComposition() const {
        (void)_Revision.Get();
        return _Composition;
    }

    void TextEditingController::SetText(std::string Text) {
        _Editor.SetText(std::move(Text));
        Notify(true);
    }

    void TextEditingController::SetSelection(TextSelection Selection) {
        _Editor.SetSelection(Selection);
        Notify(false);
    }

    bool TextEditingController::Edit(const std::function<bool(TextEditor&)>& Operation) {
        const bool Changed = Operation(_Editor);
        Notify(Changed);
        return Changed;
    }

    void TextEditingController::UpdateComposition(std::string_view Text, size_t Caret) {
        if (Text == _Composition && Caret == _CompositionCaret) { return; }
        _Composition      = Text;
        _CompositionCaret = std::min(Caret, _Composition.size());
        Notify(false);
    }

    void TextEditingController::Notify(bool TextChanged) {
        _PreferredX = -1.0f;
        _Revision   = _Revision.Peek() + 1;
        if (TextChanged && _OnTextChanged) { _OnTextChanged(); }
    }
}  // namespace XenUI
