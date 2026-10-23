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
#include "XenUI/Render/RenderBox.hpp"
#include "XenUI/Render/RenderWindowChrome.hpp"

#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace XenUI {
    class PointerTarget;
    class RenderGestureDetector;

    /// @brief Platform services the dispatcher needs from the window it serves.
    class XENUI_API InputHost {
    public:
        virtual ~InputHost();

        virtual void SetCursor(CursorShape Shape) = 0;

        /// @brief Hides and pins the cursor and starts delivering raw mouse deltas to HandleRelativeMotion.
        virtual void BeginRelativePointer() = 0;
        virtual void EndRelativePointer()   = 0;

        /// @brief Continues a data drag that left the window through the system, so other applications can receive
        /// it. Blocks until the drop and returns its effect, or nothing when the host has no system drag support.
        virtual std::optional<DropEffect> PerformSystemDrag(const DragData& Data, DropEffect Allowed);
    };

    struct GestureSettings {
        /// @brief Distance a pointer may travel before a press stops being a tap and becomes a drag.
        float DragThreshold      = 4.0f;
        double DoubleTapInterval = 0.5;
        float DoubleTapDistance  = 6.0f;
    };

    /// @brief Routes window input into the render tree: hit testing, pointer capture, gesture arbitration,
    /// hover tracking, cursors, and keyboard input through the focus manager.
    class XENUI_API InputDispatcher {
    public:
        explicit InputDispatcher(RenderBox& Root);
        ~InputDispatcher();

        InputDispatcher(const InputDispatcher&)            = delete;
        InputDispatcher& operator=(const InputDispatcher&) = delete;

        void SetHost(InputHost* Host) { _Host = Host; }
        void SetGestureSettings(const GestureSettings& Settings) { _Settings = Settings; }

        /// @param Event A window-space event.
        void HandlePointer(const PointerEvent& Event);

        /// @brief Raw movement in device-independent pixels while a relative drag is active.
        void HandleRelativeMotion(const Vec2& Delta, KeyModifiers Modifiers);

        /// @return true if the key was handled, so the platform should not process it further.
        bool HandleKey(const KeyEvent& Event);

        /// @brief Adds a handler that sees every key before focused widgets do; return true to consume the key. For
        /// window-wide tools, such as DeveloperTools, that must work wherever focus is. Returns an id for
        /// RemoveKeyInterceptor.
        uint64_t AddKeyInterceptor(std::function<bool(const KeyEvent&)> Interceptor);
        void RemoveKeyInterceptor(uint64_t Id);
        bool HandleText(const TextEvent& Event);

        /// @brief Cancels every active pointer, for example when the window loses focus.
        void CancelAll();

        /// @brief A drag from another application, or from this one through the system, moved over the window.
        /// Returns the effect a drop at Position would have.
        DropEffect
        DragOverExternal(const Vec2& Position, const DragData& Data, KeyModifiers Modifiers, DropEffect Allowed);
        void DragLeaveExternal();
        DropEffect DropExternal(const Vec2& Position, const DragData& Data, KeyModifiers Modifiers, DropEffect Allowed);

        /// @brief What the system should treat Position as in a window with a custom title bar: a caption button,
        /// a draggable caption, or ordinary client area.
        [[nodiscard]] WindowRegion HitTestWindowRegion(const Vec2& Position) const;

        /// @brief Whether a data drag started in this window is in progress.
        [[nodiscard]] bool IsDataDragActive() const { return _DataDrag.has_value(); }

        /// @brief Re-evaluates hover state at the last pointer position, after the layout under it changed.
        void RefreshHover();

        [[nodiscard]] FocusManager& GetFocusManager() { return _Focus; }
        [[nodiscard]] bool IsRelativeDragActive() const;

    private:
        struct Target {
            RenderBox* Box = nullptr;
            std::weak_ptr<void> Lifetime;
            Vec2 Origin;

            [[nodiscard]] bool IsAlive() const { return Box != nullptr && !Lifetime.expired(); }
        };

        struct Session {
            std::vector<Target> Path;
            std::vector<Target> Detectors;
            std::optional<size_t> Winner;
            MouseButton Button = MouseButton::None;
            Vec2 DownPosition;
            Vec2 LastPosition;
            Vec2 TotalDelta;
            bool TapPossible = true;
            bool Relative    = false;
            bool DataDrag    = false;
        };

        struct DropHover {
            Target Box;
            DropEffect Effect = DropEffect::None;
        };

        struct ActiveDataDrag {
            uint32_t PointerId = 0;
            Target Source;
            std::shared_ptr<DragData> Data;
            DropEffect Allowed = DropEffect::Copy;
            DropHover Over;
        };

        void HandleDown(const PointerEvent& Event);
        void HandleMove(const PointerEvent& Event);
        void HandleUp(const PointerEvent& Event);
        void HandleCancel(uint32_t PointerId, const PointerEvent& Event);
        void HandleWheel(const PointerEvent& Event);
        void UpdateHover(const PointerEvent& Event);
        void ExitAllHovered(const PointerEvent& Event);
        void UpdateCursor();
        void DeliverToPath(const std::vector<Target>& Path, const PointerEvent& Event);
        void ClaimDrag(Session& Active, const PointerEvent& Event);
        void FinishDrag(Session& Active, const PointerEvent& Event, bool Cancelled);
        bool BeginDataDrag(Session& Active, const Target& Source, const PointerEvent& Event);
        void MoveDataDrag(const PointerEvent& Event);
        void EndDataDrag(DropEffect Result);
        void CancelDataDrag();
        [[nodiscard]] DropHover FindDropTarget(const DropHover& Current,
                                               const Vec2& Position,
                                               const DragData& Data,
                                               KeyModifiers Modifiers,
                                               DropEffect Allowed);
        static void LeaveDropTarget(DropHover& Hover);
        static void DeliverDrop(const DropHover& Hover,
                                const Vec2& Position,
                                const DragData& Data,
                                KeyModifiers Modifiers,
                                DropEffect Allowed);

        [[nodiscard]] std::vector<Target> HitTest(const Vec2& Position) const;
        [[nodiscard]] static PointerEvent Localize(const PointerEvent& Event, const Target& Receiver);
        [[nodiscard]] static RenderGestureDetector& AsDetector(const Target& Entry);

        RenderBox& _Root;
        FocusManager _Focus;
        InputHost* _Host = nullptr;
        GestureSettings _Settings;
        std::vector<std::pair<uint64_t, std::function<bool(const KeyEvent&)>>> _Interceptors;
        uint64_t _NextInterceptor = 1;
        std::unordered_map<uint32_t, Session> _Sessions;
        std::vector<Target> _Hovered;
        std::optional<PointerEvent> _LastHoverEvent;
        CursorShape _Cursor = CursorShape::Arrow;
        std::optional<ActiveDataDrag> _DataDrag;
        DropHover _ExternalOver;
    };
}  // namespace XenUI
