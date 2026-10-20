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
    TextEditingController::TextEditingController(std::string Initial) : _Editor(std::move(Initial)) {}

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
        _Revision = _Revision.Peek() + 1;
        if (TextChanged && _OnTextChanged) { _OnTextChanged(); }
    }
}  // namespace XenUI
