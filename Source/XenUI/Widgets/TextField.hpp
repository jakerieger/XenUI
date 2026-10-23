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
#include "XenUI/Input/TextEditingController.hpp"
#include "XenUI/Render/RenderEditable.hpp"
#include "XenUI/Tree/Widget.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace XenUI {
    /// @brief The bare editable line inside a TextField, without decoration or keyboard handling.
    struct RawEditableText {
        TextEditingController* Controller = nullptr;
        EditableStyle Style;
        bool Focused                     = false;
        bool Enabled                     = true;
        const Signal<bool>* CaretVisible = nullptr;
        std::string Label;
        bool Multiline = false;

        [[nodiscard]] std::unique_ptr<RenderEditableText> CreateRenderObject() const {
            auto Result = std::make_unique<RenderEditableText>();
            UpdateRenderObject(*Result);
            return Result;
        }

        void UpdateRenderObject(RenderEditableText& Target) const {
            Target.SetController(Controller);
            Target.SetStyle(Style);
            Target.SetFocused(Focused);
            Target.SetEnabled(Enabled);
            Target.SetCaretVisible(CaretVisible);
            Target.SetAccessibleLabel(Label);
            Target.SetMultiline(Multiline);
        }
    };

    /// @brief A single-line text input. Supports selection with the mouse and keyboard, word movement with Ctrl,
    /// clipboard shortcuts (Ctrl+C/X/V and Ctrl/Shift+Insert), undo and redo, and input-method composition.
    /// Printable keys are consumed while it has focus, so shortcuts such as Space do not fire while typing.
    struct XENUI_API TextField {
        /// @brief Optional externally owned controller, to read or change the text and selection.
        TextEditingController* Controller = nullptr;
        /// @brief Optional two-way binding: edits write the signal, and changes to it replace the text.
        Signal<std::string>* Bind = nullptr;
        std::string InitialText;
        std::function<void(const std::string&)> OnChanged;
        /// @brief Called on Enter. Without it, Enter passes to the field's ancestors.
        std::function<void(const std::string&)> OnSubmit;
        /// @brief Called after the field gains or loses focus, for example to commit an inline edit on blur.
        std::function<void(bool Focused)> OnFocusChanged;
        /// @brief Sees keys before editing does; return true to consume one.
        KeyCallback OnKey;
        std::string Placeholder;
        std::optional<XenUI::Font> TextFont;
        FocusNode* FocusTarget = nullptr;
        bool AutoFocus         = false;
        bool Enabled           = true;
        bool SelectAllOnFocus  = false;
        /// @brief Limit in characters; 0 for none.
        size_t MaxLength = 0;
        std::optional<float> Width;
        /// @brief Shown before the text, such as a search icon.
        Node Leading;
        /// @brief The name screen readers announce; the placeholder when empty.
        std::string Label;
        /// @brief Visible lines. More than one makes a multi-line editor: text wraps and scrolls vertically, Enter
        /// inserts a line break, Ctrl+Enter calls OnSubmit, and Up, Down, Home, End, Page Up and Page Down move by
        /// visual lines (Ctrl+Home and Ctrl+End go to the start and end of the text).
        int Lines = 1;

        class XENUI_API State : public ComponentState<TextField> {
        public:
            static constexpr double kBlinkInterval = 0.53;

        protected:
            void Init() override;
            void WidgetUpdated(const TextField& Previous) override;
            void Dispose() override;
            Node Build(BuildContext& Context) override;

        private:
            [[nodiscard]] TextEditingController& GetController();
            [[nodiscard]] FocusNode& GetFocus();
            void Attach();
            bool HandleKey(const KeyEvent& Event);
            bool HandleText(const TextEvent& Event);
            void TextChanged();
            void FocusChanged(bool Focused);
            void RestartBlink();
            void Copy();
            void Paste();

            TextEditingController _InternalController;
            FocusNode _InternalFocus;
            Signal<bool> _CaretOn = true;
            Timer _Blink;
            Timer _FocusNotice;
            bool _WasFocused = false;
            bool _Syncing    = false;
        };
    };

    /// @brief A multi-line text editor: a TextField that shows several lines, wraps text and scrolls vertically.
    /// Enter inserts a line break and Ctrl+Enter calls OnSubmit.
    struct XENUI_API TextArea {
        TextEditingController* Controller = nullptr;
        Signal<std::string>* Bind         = nullptr;
        std::string InitialText;
        std::function<void(const std::string&)> OnChanged;
        /// @brief Called on Ctrl+Enter.
        std::function<void(const std::string&)> OnSubmit;
        std::function<void(bool Focused)> OnFocusChanged;
        std::string Placeholder;
        std::optional<XenUI::Font> TextFont;
        FocusNode* FocusTarget = nullptr;
        bool AutoFocus         = false;
        bool Enabled           = true;
        /// @brief Limit in characters; 0 for none.
        size_t MaxLength = 0;
        /// @brief Visible lines, which set the height.
        int Lines = 5;
        std::optional<float> Width;
        /// @brief The name screen readers announce; the placeholder when empty.
        std::string Label;

        [[nodiscard]] Node Build(BuildContext& Context) const;
    };
}  // namespace XenUI
