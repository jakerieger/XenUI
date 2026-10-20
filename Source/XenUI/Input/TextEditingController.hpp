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
#include "XenUI/Input/TextEditing.hpp"
#include "XenUI/Input/TextInput.hpp"
#include "XenUI/Reactive/Signal.hpp"

#include <functional>
#include <string>

namespace XenUI {
    /// @brief The text, selection and input-method composition of a text field. Own one to read or change a field's
    /// contents from a component; reads through Get* are tracked, so components and render objects update when
    /// the text or selection changes.
    class XENUI_API TextEditingController final : public TextInputClient {
    public:
        explicit TextEditingController(std::string Initial = {});

        [[nodiscard]] const std::string& GetText() const;
        [[nodiscard]] const std::string& PeekText() const { return _Editor.GetText(); }
        [[nodiscard]] TextSelection GetSelection() const;
        [[nodiscard]] const std::string& GetComposition() const;
        [[nodiscard]] const std::string& PeekComposition() const { return _Composition; }
        [[nodiscard]] size_t GetCompositionCaret() const { return _CompositionCaret; }
        [[nodiscard]] bool IsComposing() const { return !_Composition.empty(); }

        /// @brief Replaces the text, putting the caret at the end and clearing undo history.
        void SetText(std::string Text);
        void SetSelection(TextSelection Selection);

        /// @brief Runs an editing operation and notifies observers. Return true from Operation when it changed
        /// the text, which also calls the text-changed callback.
        bool Edit(const std::function<bool(TextEditor&)>& Operation);

        /// @brief Called after every change to the text, whether by editing or SetText.
        void SetOnTextChanged(std::function<void()> Callback) { _OnTextChanged = std::move(Callback); }

        /// @internal The editor, for updating caret stops after layout without notifying observers.
        [[nodiscard]] TextEditor& GetEditor() { return _Editor; }
        /// @internal Set by the field's render object after it paints.
        void SetCaretBounds(const Rect& Bounds) { _CaretBounds = Bounds; }

        [[nodiscard]] Rect GetCaretBounds() const override { return _CaretBounds; }
        void UpdateComposition(std::string_view Text, size_t Caret) override;

    private:
        void Notify(bool TextChanged);

        TextEditor _Editor;
        Signal<uint64_t> _Revision = 0;
        std::string _Composition;
        size_t _CompositionCaret = 0;
        Rect _CaretBounds;
        std::function<void()> _OnTextChanged;
    };
}  // namespace XenUI
