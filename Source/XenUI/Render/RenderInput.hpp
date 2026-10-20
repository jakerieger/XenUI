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
#include "XenUI/Input/DragDrop.hpp"
#include "XenUI/Input/Events.hpp"
#include "XenUI/Input/Focus.hpp"
#include "XenUI/Input/TextInput.hpp"
#include "XenUI/Input/Shortcuts.hpp"
#include "XenUI/Render/RenderBox.hpp"

#include <functional>
#include <optional>

namespace XenUI {
    /// @brief Implemented by render boxes that receive pointer input from the InputDispatcher.
    class XENUI_API PointerTarget {
    public:
        virtual ~PointerTarget();

        /// @brief Receives raw events for pointers that went down on this target, and wheel events over it.
        /// @return true to stop a wheel event from reaching targets further up.
        virtual bool HandlePointer(const PointerEvent& Event);

        [[nodiscard]] virtual bool TracksHover() const { return false; }
        virtual void HandleHoverEnter(const PointerEvent& Event);
        virtual void HandleHoverExit(const PointerEvent& Event);
        [[nodiscard]] virtual std::optional<CursorShape> GetCursor() const { return std::nullopt; }
    };

    class XENUI_API RenderInteractive : public RenderBox,
                                        public PointerTarget {
    public:
        void SetBehavior(HitTestBehavior Behavior) { _Behavior = Behavior; }
        bool HitTest(HitTestResult& Result, const Vec2& Position) override;

    private:
        HitTestBehavior _Behavior = HitTestBehavior::DeferToChild;
    };

    using PointerCallback = std::function<void(const PointerEvent&)>;

    /// @brief Returns whether the wheel event was used; unused events continue to the next target outward.
    using WheelCallback = std::function<bool(const PointerEvent&)>;

    class XENUI_API RenderListener final : public RenderInteractive {
    public:
        struct Callbacks {
            PointerCallback OnDown;
            PointerCallback OnMove;
            PointerCallback OnUp;
            PointerCallback OnCancel;
            WheelCallback OnWheel;
        };

        void SetCallbacks(Callbacks NewCallbacks) { _Callbacks = std::move(NewCallbacks); }
        bool HandlePointer(const PointerEvent& Event) override;

    private:
        Callbacks _Callbacks;
    };

    class XENUI_API RenderMouseRegion final : public RenderInteractive {
    public:
        struct Callbacks {
            PointerCallback OnEnter;
            PointerCallback OnExit;
            PointerCallback OnHover;
        };

        void SetCallbacks(Callbacks NewCallbacks) { _Callbacks = std::move(NewCallbacks); }
        void SetCursor(std::optional<CursorShape> Cursor) { _Cursor = Cursor; }

        [[nodiscard]] bool TracksHover() const override { return true; }
        void HandleHoverEnter(const PointerEvent& Event) override;
        void HandleHoverExit(const PointerEvent& Event) override;
        bool HandlePointer(const PointerEvent& Event) override;
        [[nodiscard]] std::optional<CursorShape> GetCursor() const override { return _Cursor; }

    private:
        Callbacks _Callbacks;
        std::optional<CursorShape> _Cursor;
    };

    using TapCallback  = std::function<void(const TapEvent&)>;
    using DragCallback = std::function<void(const DragEvent&)>;

    /// @brief Recognizes taps, double taps and drags. When several detectors are under a pointer, the deepest one
    /// interested in a gesture wins it: moving past the drag threshold hands the pointer to the deepest detector
    /// that handles drags, and releasing without moving hands it to the deepest one that handles taps.
    class XENUI_API RenderGestureDetector final : public RenderInteractive {
    public:
        struct Callbacks {
            TapCallback OnTap;
            TapCallback OnDoubleTap;
            TapCallback OnSecondaryTap;
            DragCallback OnDragStart;
            DragCallback OnDragUpdate;
            DragCallback OnDragEnd;
            MouseButton DragButton = MouseButton::Primary;
            /// @brief Hides the cursor and reports raw, unaccelerated and unbounded mouse movement while dragging.
            bool RelativeDrag = false;
        };

        void SetCallbacks(Callbacks NewCallbacks) { _Callbacks = std::move(NewCallbacks); }

        [[nodiscard]] bool WantsTap(MouseButton Button) const;
        [[nodiscard]] bool WantsDrag(MouseButton Button) const;
        [[nodiscard]] bool UsesRelativeDrag() const { return _Callbacks.RelativeDrag; }

        /// @internal
        void RecognizeTap(const PointerEvent& Up, double DoubleTapInterval, float DoubleTapDistance);
        /// @internal
        void StartDrag(const DragEvent& Event);
        /// @internal
        void UpdateDrag(const DragEvent& Event);
        /// @internal
        void EndDrag(const DragEvent& Event);

    private:
        Callbacks _Callbacks;
        double _LastTapTime = -1.0;
        Vec2 _LastTapPosition;
        uint32_t _TapCount = 0;
    };

    /// @brief Starts a data drag when the pointer moves past the drag threshold, if no deeper detector or source
    /// takes the drag first. The dispatcher then routes the drag to drop targets until the pointer is released.
    class XENUI_API RenderDragSource final : public RenderInteractive {
    public:
        struct Callbacks {
            DragDataCallback CreateData;
            DropEffect Allowed = DropEffect::Copy;
            /// @brief Window position of the pointer and the effect a drop there would have.
            std::function<void(const Vec2& Position, DropEffect Effect)> OnMove;
            /// @brief The drag left the window and continues through the system.
            std::function<void()> OnExitWindow;
            /// @brief The effect of the drop, or None when cancelled or refused.
            std::function<void(DropEffect Result)> OnEnd;
        };

        void SetCallbacks(Callbacks NewCallbacks) { _Callbacks = std::move(NewCallbacks); }
        [[nodiscard]] const Callbacks& GetCallbacks() const { return _Callbacks; }

    private:
        Callbacks _Callbacks;
    };

    /// @brief Receives data dragged over it, from this application or another one.
    class XENUI_API RenderDropTarget final : public RenderInteractive {
    public:
        struct Callbacks {
            DropOverCallback OnOver;
            std::function<void()> OnLeave;
            DropCallback OnDrop;
        };

        void SetCallbacks(Callbacks NewCallbacks) { _Callbacks = std::move(NewCallbacks); }
        [[nodiscard]] const Callbacks& GetCallbacks() const { return _Callbacks; }

    private:
        Callbacks _Callbacks;
    };

    using KeyCallback  = std::function<bool(const KeyEvent&)>;
    using TextCallback = std::function<bool(const TextEvent&)>;

    class XENUI_API RenderFocus final : public RenderInteractive {
    public:
        RenderFocus();
        ~RenderFocus() override;

        /// @brief Uses an externally owned node, or an internal one when nullptr.
        void SetNode(FocusNode* External);
        void SetOptions(bool AutoFocus, bool FocusOnTap, bool CanRequestFocus);
        void SetHandlers(KeyCallback OnKey, TextCallback OnText);
        void SetTextInput(TextInputClient* Client) { _TextInput = Client; }

        [[nodiscard]] TextInputClient* GetTextInput() const { return _TextInput; }

        [[nodiscard]] FocusNode& GetNode() const { return *_Node; }
        [[nodiscard]] bool CanRequestFocus() const { return _CanRequestFocus && GetOwner() != nullptr; }

        [[nodiscard]] bool WantsFocusOnTap() const { return _FocusOnTap && CanRequestFocus(); }

        bool HandleKey(const KeyEvent& Event);
        bool HandleText(const TextEvent& Event);

        /// @internal Called by a FocusNode being destroyed while still attached.
        void ReleaseNode(FocusNode& Node);

    protected:
        void OnAttached() override;
        void OnDetached() override;

    private:
        void AttachNode();
        void DetachNode();

        FocusNode _InternalNode;
        FocusNode* _Node = &_InternalNode;
        KeyCallback _OnKey;
        TextCallback _OnText;
        TextInputClient* _TextInput = nullptr;
        bool _AutoFocus             = false;
        bool _AutoFocusDone         = false;
        bool _FocusOnTap            = true;
        bool _CanRequestFocus       = true;
    };

    class XENUI_API RenderShortcuts final : public RenderBox {
    public:
        void SetShortcuts(std::vector<Shortcut> Shortcuts);
        bool HandleKey(const KeyEvent& Event) { return _Matcher.HandleKey(Event); }
        /// @brief Matches only shortcuts marked Preempts.
        bool HandlePreemptingKey(const KeyEvent& Event) { return _Preempting.HandleKey(Event); }

    private:
        ShortcutMatcher _Matcher;
        ShortcutMatcher _Preempting;
    };
}  // namespace XenUI
