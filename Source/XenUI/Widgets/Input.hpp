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

#include "XenUI/Input/Events.hpp"
#include "XenUI/Input/Focus.hpp"
#include "XenUI/Input/Shortcuts.hpp"
#include "XenUI/Render/RenderInput.hpp"
#include "XenUI/Tree/Widget.hpp"

#include <memory>
#include <optional>
#include <vector>

namespace XenUI {
    /// @brief Raw pointer events for pointers pressed on the child, plus wheel events over it. Wheel events that
    /// OnWheel reports as unused continue outward, for example to an enclosing ScrollView.
    struct Listener {
        PointerCallback OnDown;
        PointerCallback OnMove;
        PointerCallback OnUp;
        PointerCallback OnCancel;
        WheelCallback OnWheel;
        HitTestBehavior Behavior = HitTestBehavior::DeferToChild;
        Node Child;

        [[nodiscard]] std::unique_ptr<RenderListener> CreateRenderObject() const {
            auto Result = std::make_unique<RenderListener>();
            UpdateRenderObject(*Result);
            return Result;
        }

        void UpdateRenderObject(RenderListener& Target) const {
            Target.SetCallbacks({OnDown, OnMove, OnUp, OnCancel, OnWheel});
            Target.SetBehavior(Behavior);
        }
    };

    /// @brief Hover notifications and cursor shape for the area of its child.
    struct MouseRegion {
        PointerCallback OnEnter;
        PointerCallback OnExit;
        PointerCallback OnHover;
        std::optional<CursorShape> Cursor;
        HitTestBehavior Behavior = HitTestBehavior::DeferToChild;
        Node Child;

        [[nodiscard]] std::unique_ptr<RenderMouseRegion> CreateRenderObject() const {
            auto Result = std::make_unique<RenderMouseRegion>();
            UpdateRenderObject(*Result);
            return Result;
        }

        void UpdateRenderObject(RenderMouseRegion& Target) const {
            Target.SetCallbacks({OnEnter, OnExit, OnHover});
            Target.SetCursor(Cursor);
            Target.SetBehavior(Behavior);
        }
    };

    /// @brief Taps, double taps and drags. OnTap fires for every tap, including the second of a double tap.
    struct GestureDetector {
        TapCallback OnTap;
        TapCallback OnDoubleTap;
        TapCallback OnSecondaryTap;
        DragCallback OnDragStart;
        DragCallback OnDragUpdate;
        DragCallback OnDragEnd;
        MouseButton DragButton = MouseButton::Primary;
        /// @brief Hides the cursor during drags and reports raw mouse movement that is not stopped by screen
        /// edges. Suited to knobs and faders.
        bool RelativeDrag        = false;
        HitTestBehavior Behavior = HitTestBehavior::DeferToChild;
        Node Child;

        [[nodiscard]] std::unique_ptr<RenderGestureDetector> CreateRenderObject() const {
            auto Result = std::make_unique<RenderGestureDetector>();
            UpdateRenderObject(*Result);
            return Result;
        }

        void UpdateRenderObject(RenderGestureDetector& Target) const {
            Target.SetCallbacks(
              {OnTap, OnDoubleTap, OnSecondaryTap, OnDragStart, OnDragUpdate, OnDragEnd, DragButton, RelativeDrag});
            Target.SetBehavior(Behavior);
        }
    };

    /// @brief Makes its child focusable. Key and text events reach OnKey/OnText while this or a descendant has
    /// focus; return true to mark them handled. Tab and Shift+Tab move focus between Focus widgets.
    struct Focus {
        /// @brief Optional externally owned node, to observe or control focus from a component.
        FocusNode* Target    = nullptr;
        bool AutoFocus       = false;
        bool FocusOnTap      = true;
        bool CanRequestFocus = true;
        KeyCallback OnKey;
        TextCallback OnText;
        /// @brief Receives input-method compositions while focused; see TextInputClient.
        TextInputClient* TextInput = nullptr;
        HitTestBehavior Behavior   = HitTestBehavior::DeferToChild;
        /// @brief Keeps Tab and Shift+Tab cycling among the focusable widgets inside, as a modal dialog does.
        bool TrapsFocus = false;
        Node Child;

        [[nodiscard]] std::unique_ptr<RenderFocus> CreateRenderObject() const {
            auto Result = std::make_unique<RenderFocus>();
            UpdateRenderObject(*Result);
            return Result;
        }

        void UpdateRenderObject(RenderFocus& Render) const {
            Render.SetNode(Target);
            Render.SetOptions(AutoFocus, FocusOnTap, CanRequestFocus);
            Render.SetHandlers(OnKey, OnText);
            Render.SetTextInput(TextInput);
            Render.SetBehavior(Behavior);
            Render.SetTrapsFocus(TrapsFocus);
        }
    };

    /// @brief Key bindings active while focus is inside the child.
    struct Shortcuts {
        std::vector<Shortcut> Bindings;
        Node Child;

        [[nodiscard]] std::unique_ptr<RenderShortcuts> CreateRenderObject() const {
            auto Result = std::make_unique<RenderShortcuts>();
            UpdateRenderObject(*Result);
            return Result;
        }

        void UpdateRenderObject(RenderShortcuts& Target) const { Target.SetShortcuts(Bindings); }
    };
}  // namespace XenUI
