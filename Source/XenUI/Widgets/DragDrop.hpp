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
#include "XenUI/Reactive/Signal.hpp"
#include "XenUI/Render/RenderInput.hpp"
#include "XenUI/Tree/Widget.hpp"
#include "XenUI/Widgets/Overlay.hpp"

#include <functional>
#include <memory>

namespace XenUI {
    struct RawDragSource {
        RenderDragSource::Callbacks Callbacks;
        HitTestBehavior Behavior = HitTestBehavior::DeferToChild;
        Node Child;

        [[nodiscard]] std::unique_ptr<RenderDragSource> CreateRenderObject() const {
            auto Result = std::make_unique<RenderDragSource>();
            UpdateRenderObject(*Result);
            return Result;
        }

        void UpdateRenderObject(RenderDragSource& Target) const {
            Target.SetCallbacks(Callbacks);
            Target.SetBehavior(Behavior);
        }
    };

    /// @brief Makes its child draggable. Moving the pointer past the drag threshold builds the payload with Data and
    /// shows Feedback under the pointer; drop targets in any XenUI window can receive it. When the payload has Files
    /// or Text, dragging out of the window continues as a system drag, so Explorer and other applications can take
    /// it too. Deeper gesture detectors and drag sources take drags first, so nested draggables work.
    struct XENUI_API DragSource {
        DragDataCallback Data;
        /// @brief The effects a drop may have. Copy is the safe choice for files, since Explorer moves files on Move.
        DropEffect Allowed = DropEffect::Copy;
        /// @brief Drawn next to the pointer while the drag is inside the window.
        Node Feedback;
        std::function<void(DropEffect Result)> OnDragEnd;
        HitTestBehavior Behavior = HitTestBehavior::DeferToChild;
        Node Child;

        class XENUI_API State : public ComponentState<DragSource> {
        protected:
            Node Build(BuildContext& Context) override;
            void Dispose() override { _Feedback.Reset(); }

        private:
            OverlayController* _Overlay = nullptr;
            OverlayEntry _Feedback;
            Signal<Vec2> _Pointer;
            Signal<bool> _Accepted = false;
        };
    };

    /// @brief Receives drags over its child. OnOver decides the effect of a drop at a position and is called as
    /// the drag moves; OnLeave follows when the drag moves away, is dropped or is cancelled.
    struct DropTarget {
        DropOverCallback OnOver;
        std::function<void()> OnLeave;
        DropCallback OnDrop;
        HitTestBehavior Behavior = HitTestBehavior::DeferToChild;
        Node Child;

        [[nodiscard]] std::unique_ptr<RenderDropTarget> CreateRenderObject() const {
            auto Result = std::make_unique<RenderDropTarget>();
            UpdateRenderObject(*Result);
            return Result;
        }

        void UpdateRenderObject(RenderDropTarget& Target) const {
            Target.SetCallbacks({OnOver, OnLeave, OnDrop});
            Target.SetBehavior(Behavior);
        }
    };
}  // namespace XenUI
