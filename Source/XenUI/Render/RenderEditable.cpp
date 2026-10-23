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

#include "XenUI/Render/RenderEditable.hpp"
#include "XenUI/Render/Semantics.hpp"
#include "XenUI/Core/Matrix.hpp"
#include "XenUI/Graphics/Painter.hpp"

#include <algorithm>
#include <cmath>

namespace XenUI {
    namespace {
        constexpr double kMultiClickInterval = 0.5;
        constexpr float kMultiClickDistance  = 6.0f;

        float Distance(const Vec2& From, const Vec2& To) {
            return std::hypot(To.X - From.X, To.Y - From.Y);
        }

        std::string
        ComposeDisplay(const std::string& Text, const TextSelection& Selection, const std::string& Composition) {
            if (Composition.empty()) { return Text; }
            return Text.substr(0, Selection.GetStart()) + Composition + Text.substr(Selection.GetEnd());
        }
    }  // namespace

    bool RenderEditableText::DescribeSemantics(SemanticsProperties& Properties) const {
        if (_Controller == nullptr) { return false; }
        Properties = {
          .Role       = SemanticsRole::Edit,
          .Label      = _AccessibleLabel.empty() ? _Style.Placeholder : _AccessibleLabel,
          .Value      = _Controller->PeekText(),
          .Enabled    = _Enabled,
          .OnSetValue = [Controller = _Controller](const std::string& Text) { Controller->SetText(Text); },
        };
        return true;
    }

    RenderEditableText::~RenderEditableText() {
        if (_Controller != nullptr && _Controller->GetNavigator() == this) { _Controller->SetNavigator(nullptr); }
    }

    void RenderEditableText::SetController(TextEditingController* Controller) {
        if (Controller == _Controller) { return; }
        if (_Controller != nullptr && _Controller->GetNavigator() == this) { _Controller->SetNavigator(nullptr); }
        _Controller  = Controller;
        _LayoutValid = false;
        if (_Controller != nullptr) { _Controller->SetNavigator(this); }
        MarkNeedsLayout();
    }

    void RenderEditableText::SetMultiline(bool Multiline) {
        if (Multiline == _Multiline) { return; }
        _Multiline   = Multiline;
        _LayoutValid = false;
        _Placeholder = {};
        _Scroll      = 0.0f;
        _ScrollY     = 0.0f;
        MarkNeedsLayout();
    }

    std::string RenderEditableText::GetDisplay() const {
        if (_Controller == nullptr) { return {}; }
        return ComposeDisplay(_Controller->PeekText(),
                              _Controller->GetEditor().GetSelection(),
                              _Controller->PeekComposition());
    }

    float RenderEditableText::GetWrapWidth() const {
        return _Multiline ? std::max(GetSize().Width - kCaretWidth, 1.0f) : kUnbounded;
    }

    float RenderEditableText::GetMaxScrollY() const {
        return _Multiline ? std::max(0.0f, std::ceil(_Layout.GetMeasuredSize().Height) - GetSize().Height) : 0.0f;
    }

    size_t RenderEditableText::GetVerticalTarget(size_t Offset, int Lines, float& PreferredX) {
        UpdateLayout(GetDisplay());
        const Rect Caret = _Layout.GetCaretBounds(Offset);
        if (PreferredX < 0.0f) { PreferredX = Caret.X; }
        const float Target = Caret.Y + Caret.Height * 0.5f + static_cast<float>(Lines) * GetLineHeight();
        if (Target < 0.0f) { return 0; }
        if (Target > _Layout.GetMeasuredSize().Height) { return _LayoutText.size(); }
        return _Layout.HitTest({PreferredX, Target});
    }

    size_t RenderEditableText::GetLineEdge(size_t Offset, bool End) {
        UpdateLayout(GetDisplay());
        const Rect Caret = _Layout.GetCaretBounds(Offset);
        size_t Result    = _Layout.HitTest({End ? kUnbounded : 0.0f, Caret.Y + Caret.Height * 0.5f});
        if (End && Result > 0 && Result <= _LayoutText.size() && _LayoutText[Result - 1] == '\n' && Result > Offset) {
            --Result;
        }
        return Result;
    }

    int RenderEditableText::GetVisibleLineCount() const {
        return std::max(1, static_cast<int>(GetSize().Height / std::max(GetLineHeight(), 1.0f)));
    }

    void RenderEditableText::SetStyle(EditableStyle Style) {
        const bool FontChanged = !(Style.TextFont == _Style.TextFont);
        const bool HintChanged = Style.Placeholder != _Style.Placeholder;
        _Style                 = std::move(Style);
        if (FontChanged || HintChanged) {
            _LayoutValid = false;
            _LineHeight  = -1.0f;
            _Placeholder = {};
            MarkNeedsLayout();
        }
        MarkNeedsPaint();
    }

    void RenderEditableText::SetFocused(bool Focused) {
        if (Focused == _Focused) { return; }
        _Focused = Focused;
        MarkNeedsPaint();
    }

    void RenderEditableText::SetEnabled(bool Enabled) {
        if (Enabled == _Enabled) { return; }
        _Enabled = Enabled;
        MarkNeedsPaint();
    }

    void RenderEditableText::SetCaretVisible(const Signal<bool>* Visible) {
        _CaretVisible = Visible;
        MarkNeedsPaint();
    }

    bool RenderEditableText::HitTest(HitTestResult& Result, const Vec2& Position) {
        if (!_Enabled || !Rect::FromOriginSize({}, GetSize()).Contains(Position)) { return false; }
        Result.Add(*this, Position);
        return true;
    }

    size_t RenderEditableText::GetOffsetAt(const Vec2& Position) const {
        if (_Multiline) {
            return _Layout.HitTest(
              {Position.X,
               std::clamp(Position.Y + _ScrollY, 0.0f, std::max(_Layout.GetMeasuredSize().Height - 1.0f, 0.0f))});
        }
        return _Layout.HitTest(
          {Position.X + _Scroll, std::clamp(Position.Y - _TextTop, 0.0f, std::max(GetLineHeight() - 1.0f, 0.0f))});
    }

    bool RenderEditableText::HandlePointer(const PointerEvent& Event) {
        if (Event.Phase == PointerPhase::Wheel) {
            if (!_Multiline || Event.WheelDelta.Y == 0.0f) { return false; }
            const float Before = _ScrollY;
            _ScrollY = std::clamp(_ScrollY - Event.WheelDelta.Y * GetLineHeight() * 3.0f, 0.0f, GetMaxScrollY());
            if (_ScrollY == Before) { return false; }
            MarkNeedsPaint();
            return true;
        }
        if (_Controller == nullptr || !_Enabled || _Controller->IsComposing()) { return false; }
        switch (Event.Phase) {
            case PointerPhase::Down: {
                if (Event.Button != MouseButton::Primary) { return false; }
                const bool Repeated = Event.Time - _LastClickTime <= kMultiClickInterval &&
                                      Distance(Event.LocalPosition, _LastClickPosition) <= kMultiClickDistance;
                _ClickCount         = Repeated ? std::min<uint32_t>(_ClickCount + 1, 3) : 1;
                _LastClickTime      = Event.Time;
                _LastClickPosition  = Event.LocalPosition;

                const size_t Offset = GetOffsetAt(Event.LocalPosition);
                const bool Extend   = HasFlags(Event.Modifiers, KeyModifiers::Shift) && _ClickCount == 1;
                _Selecting          = true;
                _SelectingWords     = _ClickCount == 2;
                _Controller->Edit([&](TextEditor& Editor) {
                    if (_ClickCount == 3) {
                        Editor.SelectAll();
                    } else if (_ClickCount == 2) {
                        Editor.SelectWord(Offset);
                    } else {
                        Editor.SetSelection({Extend ? Editor.GetSelection().Anchor : Offset, Offset});
                    }
                    return false;
                });
                return true;
            }
            case PointerPhase::Move: {
                if (!_Selecting || _ClickCount == 3) { return false; }
                const size_t Offset = GetOffsetAt(Event.LocalPosition);
                _Controller->Edit([&](TextEditor& Editor) {
                    const TextSelection Current = Editor.GetSelection();
                    if (_SelectingWords) {
                        TextEditor Probe(Editor.GetText());
                        Probe.SelectWord(Offset);
                        const size_t Anchor = Current.Anchor;
                        Editor.SetSelection(
                          Offset < Anchor
                            ? TextSelection {std::max(Anchor, Current.Caret), Probe.GetSelection().GetStart()}
                            : TextSelection {std::min(Anchor, Current.Caret), Probe.GetSelection().GetEnd()});
                    } else {
                        Editor.SetSelection({Current.Anchor, Offset});
                    }
                    return false;
                });
                return true;
            }
            case PointerPhase::Up:
            case PointerPhase::Cancel:
                _Selecting = false;
                return true;
            default:
                return false;
        }
    }

    void RenderEditableText::OnDependencyChanged() {
        const BoxConstraints& Constraints = GetConstraints();
        if (_Multiline && Constraints.MinHeight < Constraints.MaxHeight) {
            MarkNeedsLayout();
        } else if (Constraints.HasBoundedWidth()) {
            MarkNeedsPaint();
        } else {
            MarkNeedsLayout();
        }
    }

    float RenderEditableText::GetLineHeight() const {
        if (_LineHeight < 0.0f) { _LineHeight = std::ceil(TextLayout("Ag", _Style.TextFont).GetMeasuredSize().Height); }
        return _LineHeight;
    }

    void RenderEditableText::UpdateLayout(const std::string& Display) {
        const float Width = GetWrapWidth();
        if (_LayoutValid && Display == _LayoutText && Width == _LayoutWidth) { return; }
        _Layout      = _Multiline
                         ? TextLayout(Display, _Style.TextFont, {.Wrapping = TextWrapping::Wrap}, {Width, kUnbounded})
                         : TextLayout(Display, _Style.TextFont);
        _LayoutText  = Display;
        _LayoutWidth = Width;
        _LayoutValid = true;
    }

    void RenderEditableText::PerformLayout() {
        const BoxConstraints& Constraints = GetConstraints();
        const float LineHeight            = GetLineHeight();
        if (_Multiline) {
            const float Width = Constraints.HasBoundedWidth() ? Constraints.MaxWidth : 300.0f;
            SetSize({Width, GetSize().Height});
            if (_LayoutWidth != GetWrapWidth()) { _Placeholder = {}; }
            UpdateLayout(GetDisplay());
            SetSize(Constraints.Constrain({Width, std::max(std::ceil(_Layout.GetMeasuredSize().Height), LineHeight)}));
            _TextTop = 0.0f;
            _ScrollY = std::clamp(_ScrollY, 0.0f, GetMaxScrollY());
            return;
        }
        float Width = Constraints.MaxWidth;
        if (!Constraints.HasBoundedWidth()) {
            std::string Display;
            if (_Controller != nullptr) {
                Display = ComposeDisplay(_Controller->PeekText(),
                                         _Controller->GetEditor().GetSelection(),
                                         _Controller->PeekComposition());
            }
            UpdateLayout(Display);
            Width = std::ceil(std::max(_Layout.GetMeasuredSize().Width,
                                       _Placeholder.IsEmpty() ? 0.0f : _Placeholder.GetMeasuredSize().Width)) +
                    kCaretWidth;
        }
        SetSize(Constraints.Constrain({Width, LineHeight}));
        _TextTop = std::floor((GetSize().Height - LineHeight) * 0.5f);
    }

    void RenderEditableText::Paint(PaintContext& Context) {
        if (_Controller == nullptr) { return; }
        std::string Text;
        std::string Composition;
        TextSelection Selection;
        bool CaretOn = true;
        {
            TrackingScope Scope(*this);
            Text        = _Controller->GetText();
            Selection   = _Controller->GetSelection();
            Composition = _Controller->GetComposition();
            CaretOn     = _CaretVisible == nullptr || _CaretVisible->Get();
        }

        const bool Composing      = !Composition.empty();
        const std::string Display = ComposeDisplay(Text, Selection, Composition);
        const bool Rebuilt        = !_LayoutValid || Display != _LayoutText;
        UpdateLayout(Display);
        if (Rebuilt && !Composing) { _Controller->GetEditor().SetCaretStops(_Layout.GetCaretStops()); }
        if (_Placeholder.IsEmpty() && !_Style.Placeholder.empty() && !_Multiline) {
            _Placeholder = TextLayout(_Style.Placeholder, _Style.TextFont);
        }

        if (_Placeholder.IsEmpty() && !_Style.Placeholder.empty() && _Multiline) {
            _Placeholder = TextLayout(_Style.Placeholder,
                                      _Style.TextFont,
                                      {.Wrapping = TextWrapping::Wrap},
                                      {GetWrapWidth(), kUnbounded});
        }

        const size_t CaretOffset =
          Composing ? Selection.GetStart() + _Controller->GetCompositionCaret() : Selection.Caret;
        const Rect Caret   = _Layout.GetCaretBounds(CaretOffset);
        const float Width  = GetSize().Width;
        const float Height = GetSize().Height;
        if (_Multiline) {
            if (CaretOffset != _ShownCaret || Display != _ShownText) {
                if (Caret.Y - _ScrollY < 0.0f) { _ScrollY = Caret.Y; }
                if (Caret.Y + Caret.Height - _ScrollY > Height) { _ScrollY = Caret.Y + Caret.Height - Height; }
            }
            _ScrollY = std::clamp(_ScrollY, 0.0f, GetMaxScrollY());
            _Scroll  = 0.0f;
        } else {
            const float Visible = std::max(Width - kCaretWidth, 0.0f);
            if (Caret.X - _Scroll > Visible) { _Scroll = Caret.X - Visible; }
            if (Caret.X - _Scroll < 0.0f) { _Scroll = Caret.X; }
            _Scroll = std::clamp(_Scroll, 0.0f, std::max(0.0f, _Layout.GetMeasuredSize().Width + kCaretWidth - Width));
        }
        _ShownCaret = CaretOffset;
        _ShownText  = Display;

        Painter& Canvas = Context.GetPainter();
        Canvas.PushClip({0.0f, 0.0f, Width, Height});
        Canvas.PushTransform(Matrix3x2::Translation(-_Scroll, _TextTop - _ScrollY));

        if (_Focused && !Composing && !Selection.IsCollapsed()) {
            for (const Rect& Range : _Layout.GetRangeBounds(Selection.GetStart(), Selection.GetEnd())) {
                Canvas.FillRect(Range, _Style.SelectionColor);
            }
        }
        if (Display.empty() && !_Placeholder.IsEmpty()) {
            Canvas.DrawTextLayout(_Placeholder, {}, _Style.PlaceholderColor);
        } else {
            Canvas.DrawTextLayout(_Layout, {}, _Enabled ? _Style.Foreground : _Style.PlaceholderColor);
        }
        if (Composing) {
            for (const Rect& Range :
                 _Layout.GetRangeBounds(Selection.GetStart(), Selection.GetStart() + Composition.size())) {
                Canvas.FillRect({Range.X, Range.Y + Range.Height - 1.5f, Range.Width, 1.5f}, _Style.Foreground);
            }
        }
        if (_Focused && _Enabled && CaretOn && (Selection.IsCollapsed() || Composing)) {
            Canvas.FillRect({Caret.X, Caret.Y, kCaretWidth, Caret.Height}, _Style.CaretColor);
        }

        Canvas.PopTransform();
        Canvas.PopClip();
        _Controller->SetCaretBounds(
          Rect::FromOriginSize(GetGlobalOffset() + Vec2 {Caret.X - _Scroll, Caret.Y + _TextTop - _ScrollY},
                               {kCaretWidth, Caret.Height}));
    }
}  // namespace XenUI
