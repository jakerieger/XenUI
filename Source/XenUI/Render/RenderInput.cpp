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

#include "XenUI/Render/RenderInput.hpp"

#include <cmath>

namespace XenUI {
    namespace {
        float Distance(const Vec2& From, const Vec2& To) {
            const Vec2 Delta = To - From;
            return std::sqrt(Delta.X * Delta.X + Delta.Y * Delta.Y);
        }
    }  // namespace

    PointerTarget::~PointerTarget() = default;

    bool PointerTarget::HandlePointer(const PointerEvent& Event) {
        (void)Event;
        return false;
    }

    void PointerTarget::HandleHoverEnter(const PointerEvent& Event) {
        (void)Event;
    }

    void PointerTarget::HandleHoverExit(const PointerEvent& Event) {
        (void)Event;
    }

    bool RenderInteractive::HitTest(HitTestResult& Result, const Vec2& Position) {
        if (!Rect {0.0f, 0.0f, GetSize().Width, GetSize().Height}.Contains(Position)) { return false; }

        const bool ChildHit = HitTestChildren(Result, Position);
        if (ChildHit || _Behavior != HitTestBehavior::DeferToChild) { Result.Add(*this, Position); }
        return ChildHit || _Behavior == HitTestBehavior::Opaque;
    }

    bool RenderListener::HandlePointer(const PointerEvent& Event) {
        const auto Invoke = [&Event](const PointerCallback& Callback) {
            if (Callback) { Callback(Event); }
        };

        switch (Event.Phase) {
            case PointerPhase::Down:
                Invoke(_Callbacks.OnDown);
                return false;
            case PointerPhase::Move:
                Invoke(_Callbacks.OnMove);
                return false;
            case PointerPhase::Up:
                Invoke(_Callbacks.OnUp);
                return false;
            case PointerPhase::Cancel:
                Invoke(_Callbacks.OnCancel);
                return false;
            case PointerPhase::Wheel:
                return _Callbacks.OnWheel && _Callbacks.OnWheel(Event);
            default:
                return false;
        }
    }

    void RenderMouseRegion::HandleHoverEnter(const PointerEvent& Event) {
        if (_Callbacks.OnEnter) { _Callbacks.OnEnter(Event); }
    }

    void RenderMouseRegion::HandleHoverExit(const PointerEvent& Event) {
        if (_Callbacks.OnExit) { _Callbacks.OnExit(Event); }
    }

    bool RenderMouseRegion::HandlePointer(const PointerEvent& Event) {
        if (Event.Phase == PointerPhase::Move && _Callbacks.OnHover) { _Callbacks.OnHover(Event); }
        return false;
    }

    bool RenderGestureDetector::WantsTap(MouseButton Button) const {
        if (Button == MouseButton::Primary) { return _Callbacks.OnTap || _Callbacks.OnDoubleTap; }
        if (Button == MouseButton::Secondary) { return static_cast<bool>(_Callbacks.OnSecondaryTap); }
        return false;
    }

    bool RenderGestureDetector::WantsDrag(MouseButton Button) const {
        return Button == _Callbacks.DragButton &&
               (_Callbacks.OnDragStart || _Callbacks.OnDragUpdate || _Callbacks.OnDragEnd);
    }

    void
    RenderGestureDetector::RecognizeTap(const PointerEvent& Up, double DoubleTapInterval, float DoubleTapDistance) {
        if (Up.Button == MouseButton::Secondary) {
            if (_Callbacks.OnSecondaryTap) {
                _Callbacks.OnSecondaryTap({Up.LocalPosition, Up.Button, Up.Modifiers, 1});
            }
            return;
        }

        const bool Continues = _TapCount > 0 && Up.Time - _LastTapTime <= DoubleTapInterval &&
                               Distance(_LastTapPosition, Up.LocalPosition) <= DoubleTapDistance;
        _TapCount        = Continues ? _TapCount + 1 : 1;
        _LastTapTime     = Up.Time;
        _LastTapPosition = Up.LocalPosition;

        const TapEvent Tap {Up.LocalPosition, Up.Button, Up.Modifiers, _TapCount};
        const Callbacks Current = _Callbacks;
        if (Current.OnTap) { Current.OnTap(Tap); }
        if (_TapCount == 2 && Current.OnDoubleTap) { Current.OnDoubleTap(Tap); }
    }

    void RenderGestureDetector::StartDrag(const DragEvent& Event) {
        _TapCount = 0;
        if (_Callbacks.OnDragStart) { _Callbacks.OnDragStart(Event); }
    }

    void RenderGestureDetector::UpdateDrag(const DragEvent& Event) {
        if (_Callbacks.OnDragUpdate) { _Callbacks.OnDragUpdate(Event); }
    }

    void RenderGestureDetector::EndDrag(const DragEvent& Event) {
        if (_Callbacks.OnDragEnd) { _Callbacks.OnDragEnd(Event); }
    }

    RenderFocus::RenderFocus() = default;

    RenderFocus::~RenderFocus() {
        DetachNode();
    }

    void RenderFocus::SetNode(FocusNode* External) {
        FocusNode* const Next = External != nullptr ? External : &_InternalNode;
        if (Next == _Node) { return; }

        const bool WasFocused = _Node->_Manager != nullptr && _Node->_Manager->GetFocused() == _Node;
        DetachNode();
        _Node = Next;
        if (GetOwner() != nullptr) { AttachNode(); }
        if (WasFocused) { _Node->RequestFocus(); }
    }

    void RenderFocus::SetOptions(bool AutoFocus, bool FocusOnTap, bool CanRequestFocus) {
        _AutoFocus       = AutoFocus;
        _FocusOnTap      = FocusOnTap;
        _CanRequestFocus = CanRequestFocus;
        if (_AutoFocus && !_AutoFocusDone && GetOwner() != nullptr) {
            _AutoFocusDone = true;
            _Node->RequestFocus();
        }
    }

    void RenderFocus::SetHandlers(KeyCallback OnKey, TextCallback OnText) {
        _OnKey  = std::move(OnKey);
        _OnText = std::move(OnText);
    }

    bool RenderFocus::HandleKey(const KeyEvent& Event) {
        return _OnKey && _OnKey(Event);
    }

    bool RenderFocus::HandleText(const TextEvent& Event) {
        return _OnText && _OnText(Event);
    }

    void RenderFocus::ReleaseNode(FocusNode& Node) {
        if (&Node != _Node) { return; }
        DetachNode();
        _Node = &_InternalNode;
        if (GetOwner() != nullptr) { AttachNode(); }
    }

    void RenderFocus::OnAttached() {
        RenderInteractive::OnAttached();
        AttachNode();
        if (_AutoFocus && !_AutoFocusDone) {
            _AutoFocusDone = true;
            _Node->RequestFocus();
        }
    }

    void RenderFocus::OnDetached() {
        DetachNode();
        RenderInteractive::OnDetached();
    }

    void RenderFocus::AttachNode() {
        _Node->_Attached = this;
        _Node->_Manager  = GetOwner()->GetFocusManager();
        if (_Node->_FocusRequested && _Node->_Manager != nullptr) {
            _Node->_FocusRequested = false;
            _Node->_Manager->SetFocus(_Node);
        }
    }

    void RenderFocus::DetachNode() {
        if (_Node->_Attached != this) { return; }
        if (_Node->_Manager != nullptr) { _Node->_Manager->Forget(*_Node); }
        _Node->_Attached = nullptr;
        _Node->_Manager  = nullptr;
    }

    void RenderShortcuts::SetShortcuts(std::vector<Shortcut> Shortcuts) {
        std::vector<Shortcut> Preempting;
        std::vector<Shortcut> Ordinary;
        for (Shortcut& Entry : Shortcuts) {
            (Entry.Preempts ? Preempting : Ordinary).push_back(std::move(Entry));
        }
        _Matcher.SetShortcuts(std::move(Ordinary));
        _Preempting.SetShortcuts(std::move(Preempting));
    }
}  // namespace XenUI
