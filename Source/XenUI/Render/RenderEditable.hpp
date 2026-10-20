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
#include "XenUI/Core/Color.hpp"
#include "XenUI/Graphics/Text.hpp"
#include "XenUI/Input/TextEditingController.hpp"
#include "XenUI/Reactive/Reactive.hpp"
#include "XenUI/Render/RenderInput.hpp"

#include <string>

namespace XenUI {
    struct EditableStyle {
        Font TextFont;
        Color Foreground;
        Color PlaceholderColor;
        Color SelectionColor;
        Color CaretColor;
        std::string Placeholder;
    };

    /// @brief A single line of editable text: draws the text, selection, composition and caret of a controller and
    /// turns presses and drags into caret placement and selection. Scrolls horizontally to keep the caret in view.
    /// Observes the controller directly, so typing and caret blinking repaint without rebuilding widgets.
    class XENUI_API RenderEditableText final : public RenderInteractive,
                                               public Observer {
    public:
        static constexpr float kCaretWidth = 1.5f;

        void SetController(TextEditingController* Controller);
        /// @brief The name screen readers announce; the placeholder when empty.
        void SetAccessibleLabel(std::string Label) { _AccessibleLabel = std::move(Label); }
        [[nodiscard]] bool DescribeSemantics(SemanticsProperties& Properties) const override;
        void SetStyle(EditableStyle Style);
        void SetFocused(bool Focused);
        void SetEnabled(bool Enabled);
        void SetCaretVisible(const Signal<bool>* Visible);

        [[nodiscard]] bool IsRepaintBoundary() const override { return true; }
        bool HitTest(HitTestResult& Result, const Vec2& Position) override;
        bool HandlePointer(const PointerEvent& Event) override;
        [[nodiscard]] std::optional<CursorShape> GetCursor() const override { return CursorShape::IBeam; }
        void Paint(PaintContext& Context) override;
        void OnDependencyChanged() override;

        /// @brief The text offset under a point in this box's coordinates.
        [[nodiscard]] size_t GetOffsetAt(const Vec2& Position) const;
        [[nodiscard]] float GetScrollOffset() const { return _Scroll; }

    protected:
        void PerformLayout() override;

    private:
        void UpdateLayout(const std::string& Display);
        [[nodiscard]] float GetLineHeight() const;

        TextEditingController* _Controller = nullptr;
        std::string _AccessibleLabel;
        EditableStyle _Style;
        const Signal<bool>* _CaretVisible = nullptr;
        bool _Focused                     = false;
        bool _Enabled                     = true;

        TextLayout _Layout;
        std::string _LayoutText;
        bool _LayoutValid = false;
        TextLayout _Placeholder;
        mutable float _LineHeight = -1.0f;
        float _Scroll             = 0.0f;
        float _TextTop            = 0.0f;

        bool _Selecting       = false;
        bool _SelectingWords  = false;
        uint32_t _ClickCount  = 0;
        double _LastClickTime = -1.0;
        Vec2 _LastClickPosition;
    };
}  // namespace XenUI
