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

#include "XenUI/Input/InputDispatcher.hpp"
#include "XenUI/Render/RenderInput.hpp"

#include <algorithm>
#include <cmath>

namespace XenUI {
    namespace {
        float Distance(const Vec2& From, const Vec2& To) {
            const Vec2 Delta = To - From;
            return std::sqrt(Delta.X * Delta.X + Delta.Y * Delta.Y);
        }
    }  // namespace

    InputHost::~InputHost() = default;

    InputDispatcher::InputDispatcher(RenderBox& Root) : _Root(Root), _Focus(Root) {}

    InputDispatcher::~InputDispatcher() {
        if (IsRelativeDragActive() && _Host != nullptr) { _Host->EndRelativePointer(); }
    }

    void InputDispatcher::HandlePointer(const PointerEvent& Event) {
        switch (Event.Phase) {
            case PointerPhase::Down:
                HandleDown(Event);
                break;
            case PointerPhase::Move:
                HandleMove(Event);
                break;
            case PointerPhase::Up:
                HandleUp(Event);
                break;
            case PointerPhase::Cancel:
                HandleCancel(Event.PointerId, Event);
                break;
            case PointerPhase::Wheel:
                HandleWheel(Event);
                break;
            case PointerPhase::Leave:
                _LastHoverEvent.reset();
                ExitAllHovered(Event);
                break;
        }
    }

    void InputDispatcher::HandleRelativeMotion(const Vec2& Delta, KeyModifiers Modifiers) {
        for (auto& [Id, Active] : _Sessions) {
            if (!Active.Relative || !Active.Winner) { continue; }
            const Target& Winner = Active.Detectors[*Active.Winner];
            if (!Winner.IsAlive()) { continue; }

            Active.TotalDelta += Delta;
            AsDetector(Winner).UpdateDrag({
              .LocalPosition = Active.DownPosition - Winner.Origin,
              .Delta         = Delta,
              .TotalDelta    = Active.TotalDelta,
              .Button        = Active.Button,
              .Modifiers     = Modifiers,
              .Relative      = true,
            });
        }
    }

    std::optional<DropEffect> InputHost::PerformSystemDrag(const DragData& Data, DropEffect Allowed) {
        (void)Data;
        (void)Allowed;
        return std::nullopt;
    }

    uint64_t InputDispatcher::AddKeyInterceptor(std::function<bool(const KeyEvent&)> Interceptor) {
        const uint64_t Id = _NextInterceptor++;
        _Interceptors.emplace_back(Id, std::move(Interceptor));
        return Id;
    }

    void InputDispatcher::RemoveKeyInterceptor(uint64_t Id) {
        std::erase_if(_Interceptors, [Id](const auto& Entry) { return Entry.first == Id; });
    }

    bool InputDispatcher::HandleKey(const KeyEvent& Event) {
        if (_DataDrag && Event.Pressed && Event.Code == KeyCode::Escape) {
            CancelDataDrag();
            return true;
        }
        for (const auto& [Id, Interceptor] : std::vector(_Interceptors)) {
            if (Interceptor(Event)) { return true; }
        }
        if (_Focus.DispatchKey(Event)) { return true; }

        const bool IsTraversal = Event.Pressed && Event.Code == KeyCode::Tab &&
                                 (Event.Modifiers == KeyModifiers::None || Event.Modifiers == KeyModifiers::Shift);
        if (IsTraversal) {
            _Focus.MoveFocus(Event.Modifiers == KeyModifiers::None);
            return true;
        }
        return false;
    }

    bool InputDispatcher::HandleText(const TextEvent& Event) {
        return _Focus.DispatchText(Event);
    }

    void InputDispatcher::CancelAll() {
        std::vector<uint32_t> Ids;
        for (const auto& [Id, Active] : _Sessions) {
            Ids.push_back(Id);
        }
        for (const uint32_t Id : Ids) {
            HandleCancel(Id, {.Phase = PointerPhase::Cancel, .PointerId = Id});
        }
    }

    void InputDispatcher::RefreshHover() {
        if (_LastHoverEvent && !IsRelativeDragActive()) { UpdateHover(*_LastHoverEvent); }
    }

    bool InputDispatcher::IsRelativeDragActive() const {
        return std::any_of(_Sessions.begin(), _Sessions.end(), [](const auto& Entry) {
            return Entry.second.Relative && Entry.second.Winner.has_value();
        });
    }

    void InputDispatcher::HandleDown(const PointerEvent& Event) {
        if (const auto Existing = _Sessions.find(Event.PointerId); Existing != _Sessions.end()) {
            DeliverToPath(Existing->second.Path, Event);
            return;
        }

        Session Active;
        Active.Path         = HitTest(Event.Position);
        Active.Button       = Event.Button;
        Active.DownPosition = Event.Position;
        Active.LastPosition = Event.Position;
        for (const Target& Entry : Active.Path) {
            const auto* Detector = dynamic_cast<RenderGestureDetector*>(Entry.Box);
            if (Detector != nullptr && (Detector->WantsTap(Event.Button) || Detector->WantsDrag(Event.Button))) {
                Active.Detectors.push_back(Entry);
            }
        }

        const std::vector<Target> Path = Active.Path;
        _Sessions[Event.PointerId]     = std::move(Active);
        DeliverToPath(Path, Event);

        for (const Target& Entry : Path) {
            auto* Focusable = Entry.IsAlive() ? dynamic_cast<RenderFocus*>(Entry.Box) : nullptr;
            if (Focusable != nullptr && Focusable->WantsFocusOnTap()) {
                Focusable->GetNode().RequestFocus();
                break;
            }
        }
    }

    void InputDispatcher::HandleMove(const PointerEvent& Event) {
        if (!IsRelativeDragActive()) { UpdateHover(Event); }

        const auto Found = _Sessions.find(Event.PointerId);
        if (Found == _Sessions.end()) {
            const std::vector<Target> Hovered = _Hovered;
            for (const Target& Entry : Hovered) {
                if (!Entry.IsAlive()) { continue; }
                if (auto* Receiver = dynamic_cast<PointerTarget*>(Entry.Box)) {
                    Receiver->HandlePointer(Localize(Event, Entry));
                }
            }
            return;
        }

        Session& Active = Found->second;
        if (Active.Relative) { return; }

        const std::vector<Target> Path = Active.Path;
        DeliverToPath(Path, Event);

        const auto Current = _Sessions.find(Event.PointerId);
        if (Current == _Sessions.end()) { return; }
        Session& Tracked = Current->second;
        if (Tracked.DataDrag) {
            Tracked.LastPosition = Event.Position;
            MoveDataDrag(Event);
            return;
        }

        if (!Tracked.Winner) {
            if (Distance(Event.Position, Tracked.DownPosition) > _Settings.DragThreshold) {
                Tracked.TapPossible = false;
                ClaimDrag(Tracked, Event);
            }
        } else {
            const Vec2 Delta = Event.Position - Tracked.LastPosition;
            Tracked.TotalDelta += Delta;
            const Target& Winner = Tracked.Detectors[*Tracked.Winner];
            if (Winner.IsAlive()) {
                AsDetector(Winner).UpdateDrag({
                  .LocalPosition = Event.Position - Winner.Origin,
                  .Delta         = Delta,
                  .TotalDelta    = Tracked.TotalDelta,
                  .Button        = Tracked.Button,
                  .Modifiers     = Event.Modifiers,
                });
            }
        }
        Tracked.LastPosition = Event.Position;
    }

    void InputDispatcher::HandleUp(const PointerEvent& Event) {
        const auto Found = _Sessions.find(Event.PointerId);
        if (Found == _Sessions.end()) { return; }

        if (Event.Button != Found->second.Button) {
            const std::vector<Target> Path = Found->second.Path;
            DeliverToPath(Path, Event);
            return;
        }

        Session Finished = std::move(Found->second);
        _Sessions.erase(Found);

        DeliverToPath(Finished.Path, Event);
        if (Finished.DataDrag && _DataDrag) {
            ActiveDataDrag Drag = std::move(*_DataDrag);
            _DataDrag.reset();
            Drag.Over = FindDropTarget(Drag.Over, Event.Position, *Drag.Data, Event.Modifiers, Drag.Allowed);
            DeliverDrop(Drag.Over, Event.Position, *Drag.Data, Event.Modifiers, Drag.Allowed);
            const DropEffect Result = Drag.Over.Effect;
            _DataDrag               = std::move(Drag);
            EndDataDrag(Result);
        } else if (Finished.Winner) {
            FinishDrag(Finished, Event, false);
        } else if (Finished.TapPossible) {
            for (const Target& Entry : Finished.Detectors) {
                if (!Entry.IsAlive() || !AsDetector(Entry).WantsTap(Finished.Button)) { continue; }
                AsDetector(Entry).RecognizeTap(Localize(Event, Entry),
                                               _Settings.DoubleTapInterval,
                                               _Settings.DoubleTapDistance);
                break;
            }
        }

        UpdateHover(Event);
    }

    void InputDispatcher::HandleCancel(uint32_t PointerId, const PointerEvent& Event) {
        const auto Found = _Sessions.find(PointerId);
        if (Found == _Sessions.end()) { return; }

        Session Cancelled = std::move(Found->second);
        _Sessions.erase(Found);

        PointerEvent Cancel = Event;
        Cancel.Phase        = PointerPhase::Cancel;
        Cancel.Position     = Cancelled.LastPosition;
        DeliverToPath(Cancelled.Path, Cancel);
        if (Cancelled.Winner) { FinishDrag(Cancelled, Cancel, true); }
        if (Cancelled.DataDrag) { CancelDataDrag(); }
        UpdateCursor();
    }

    void InputDispatcher::HandleWheel(const PointerEvent& Event) {
        for (const Target& Entry : HitTest(Event.Position)) {
            if (!Entry.IsAlive()) { continue; }
            auto* Receiver = dynamic_cast<PointerTarget*>(Entry.Box);
            if (Receiver != nullptr && Receiver->HandlePointer(Localize(Event, Entry))) { return; }
        }
    }

    void InputDispatcher::UpdateHover(const PointerEvent& Event) {
        PointerEvent Hover = Event;
        Hover.Phase        = PointerPhase::Move;
        _LastHoverEvent    = Hover;

        std::vector<Target> Current;
        for (const Target& Entry : HitTest(Event.Position)) {
            const auto* Receiver = dynamic_cast<PointerTarget*>(Entry.Box);
            if (Receiver != nullptr && Receiver->TracksHover()) { Current.push_back(Entry); }
        }

        const auto Contains = [](const std::vector<Target>& Targets, const Target& Candidate) {
            return std::any_of(Targets.begin(), Targets.end(), [&Candidate](const Target& Entry) {
                return Entry.Box == Candidate.Box;
            });
        };

        const std::vector<Target> Previous = std::exchange(_Hovered, Current);
        for (const Target& Entry : Previous) {
            if (!Entry.IsAlive() || Contains(Current, Entry)) { continue; }
            dynamic_cast<PointerTarget*>(Entry.Box)->HandleHoverExit(Localize(Hover, Entry));
        }
        for (auto It = Current.rbegin(); It != Current.rend(); ++It) {
            if (!It->IsAlive() || Contains(Previous, *It)) { continue; }
            dynamic_cast<PointerTarget*>(It->Box)->HandleHoverEnter(Localize(Hover, *It));
        }

        UpdateCursor();
    }

    void InputDispatcher::ExitAllHovered(const PointerEvent& Event) {
        const std::vector<Target> Previous = std::exchange(_Hovered, {});
        for (const Target& Entry : Previous) {
            if (Entry.IsAlive()) { dynamic_cast<PointerTarget*>(Entry.Box)->HandleHoverExit(Localize(Event, Entry)); }
        }
        UpdateCursor();
    }

    void InputDispatcher::UpdateCursor() {
        CursorShape Shape = CursorShape::Arrow;
        if (_DataDrag) {
            Shape = _DataDrag->Over.Effect == DropEffect::None ? CursorShape::NotAllowed : CursorShape::Arrow;
        } else if (IsRelativeDragActive()) {
            Shape = CursorShape::Hidden;
        } else {
            for (const Target& Entry : _Hovered) {
                if (!Entry.IsAlive()) { continue; }
                if (const auto Requested = dynamic_cast<PointerTarget*>(Entry.Box)->GetCursor()) {
                    Shape = *Requested;
                    break;
                }
            }
        }

        if (Shape == _Cursor) { return; }
        _Cursor = Shape;
        if (_Host != nullptr) { _Host->SetCursor(Shape); }
    }

    void InputDispatcher::DeliverToPath(const std::vector<Target>& Path, const PointerEvent& Event) {
        for (const Target& Entry : Path) {
            if (!Entry.IsAlive()) { continue; }
            if (auto* Receiver = dynamic_cast<PointerTarget*>(Entry.Box)) {
                Receiver->HandlePointer(Localize(Event, Entry));
            }
        }
    }

    void InputDispatcher::ClaimDrag(Session& Active, const PointerEvent& Event) {
        for (const Target& Entry : Active.Path) {
            if (!Entry.IsAlive()) { continue; }
            if (const auto* Detector = dynamic_cast<RenderGestureDetector*>(Entry.Box);
                Detector != nullptr && Detector->WantsDrag(Active.Button)) {
                for (size_t i = 0; i < Active.Detectors.size(); ++i) {
                    if (Active.Detectors[i].Box == Entry.Box) { Active.Winner = i; }
                }
                break;
            }
            if (dynamic_cast<RenderDragSource*>(Entry.Box) != nullptr && Active.Button == MouseButton::Primary &&
                BeginDataDrag(Active, Entry, Event)) {
                return;
            }
        }
        if (!Active.Winner) { return; }

        const Target Winner             = Active.Detectors[*Active.Winner];
        RenderGestureDetector& Detector = AsDetector(Winner);
        Active.Relative                 = Detector.UsesRelativeDrag() && _Host != nullptr;
        Active.TotalDelta               = Event.Position - Active.DownPosition;

        const Vec2 Start = Active.DownPosition - Winner.Origin;
        Detector.StartDrag({
          .LocalPosition = Start,
          .Button        = Active.Button,
          .Modifiers     = Event.Modifiers,
          .Relative      = Active.Relative,
        });
        Detector.UpdateDrag({
          .LocalPosition = Active.Relative ? Start : Event.Position - Winner.Origin,
          .Delta         = Active.TotalDelta,
          .TotalDelta    = Active.TotalDelta,
          .Button        = Active.Button,
          .Modifiers     = Event.Modifiers,
          .Relative      = Active.Relative,
        });

        if (Active.Relative) {
            _Host->BeginRelativePointer();
            UpdateCursor();
        }
    }

    void InputDispatcher::FinishDrag(Session& Active, const PointerEvent& Event, bool Cancelled) {
        const Target& Winner = Active.Detectors[*Active.Winner];
        if (Active.Relative && _Host != nullptr) { _Host->EndRelativePointer(); }
        if (!Winner.IsAlive()) { return; }

        AsDetector(Winner).EndDrag({
          .LocalPosition = Active.Relative ? Active.DownPosition - Winner.Origin : Event.Position - Winner.Origin,
          .TotalDelta    = Active.TotalDelta,
          .Button        = Active.Button,
          .Modifiers     = Event.Modifiers,
          .Relative      = Active.Relative,
          .Cancelled     = Cancelled,
        });
    }

    bool InputDispatcher::BeginDataDrag(Session& Active, const Target& Source, const PointerEvent& Event) {
        const RenderDragSource::Callbacks& Callbacks = static_cast<RenderDragSource&>(*Source.Box).GetCallbacks();
        std::optional<DragData> Data                 = Callbacks.CreateData ? Callbacks.CreateData() : std::nullopt;
        if (!Data) { return false; }

        uint32_t PointerId = 0;
        for (const auto& [Id, Candidate] : _Sessions) {
            if (&Candidate == &Active) { PointerId = Id; }
        }
        Active.DataDrag    = true;
        Active.TapPossible = false;
        _DataDrag          = ActiveDataDrag {
          .PointerId = PointerId,
          .Source    = Source,
          .Data      = std::make_shared<DragData>(std::move(*Data)),
          .Allowed   = Callbacks.Allowed,
        };
        MoveDataDrag(Event);
        return true;
    }

    void InputDispatcher::MoveDataDrag(const PointerEvent& Event) {
        if (!_DataDrag) { return; }
        ActiveDataDrag& Drag = *_DataDrag;
        const Size Bounds    = _Root.GetSize();
        const bool Outside   = !Rect {0.0f, 0.0f, Bounds.Width, Bounds.Height}.Contains(Event.Position);

        if (Outside && _Host != nullptr && Drag.Data->IsExportable()) {
            LeaveDropTarget(Drag.Over);
            if (Drag.Source.IsAlive()) {
                const auto& OnExit = static_cast<RenderDragSource&>(*Drag.Source.Box).GetCallbacks().OnExitWindow;
                if (OnExit) { OnExit(); }
            }
            const std::shared_ptr<DragData> Data = Drag.Data;
            const uint32_t PointerId             = Drag.PointerId;
            if (const std::optional<DropEffect> Result = _Host->PerformSystemDrag(*Data, Drag.Allowed)) {
                _Sessions.erase(PointerId);
                EndDataDrag(*Result);
                return;
            }
        }

        Drag.Over = FindDropTarget(Drag.Over, Event.Position, *Drag.Data, Event.Modifiers, Drag.Allowed);
        if (Drag.Source.IsAlive()) {
            const auto& OnMove = static_cast<RenderDragSource&>(*Drag.Source.Box).GetCallbacks().OnMove;
            if (OnMove) { OnMove(Event.Position, Drag.Over.Effect); }
        }
        UpdateCursor();
    }

    void InputDispatcher::EndDataDrag(DropEffect Result) {
        if (!_DataDrag) { return; }
        const ActiveDataDrag Drag = std::move(*_DataDrag);
        _DataDrag.reset();
        if (Drag.Source.IsAlive()) {
            const auto& OnEnd = static_cast<RenderDragSource&>(*Drag.Source.Box).GetCallbacks().OnEnd;
            if (OnEnd) { OnEnd(Result); }
        }
        UpdateCursor();
    }

    void InputDispatcher::CancelDataDrag() {
        if (!_DataDrag) { return; }
        LeaveDropTarget(_DataDrag->Over);
        if (const auto Found = _Sessions.find(_DataDrag->PointerId); Found != _Sessions.end()) {
            Found->second.DataDrag = false;
        }
        EndDataDrag(DropEffect::None);
    }

    InputDispatcher::DropHover InputDispatcher::FindDropTarget(const DropHover& Current,
                                                               const Vec2& Position,
                                                               const DragData& Data,
                                                               KeyModifiers Modifiers,
                                                               DropEffect Allowed) {
        DropHover Next;
        for (const Target& Entry : HitTest(Position)) {
            auto* Drop = dynamic_cast<RenderDropTarget*>(Entry.Box);
            if (Drop == nullptr || !Entry.IsAlive() || !Drop->GetCallbacks().OnOver) { continue; }
            const DropEffect Wanted = Drop->GetCallbacks().OnOver(
              {.Data = &Data, .LocalPosition = Position - Entry.Origin, .Modifiers = Modifiers, .Allowed = Allowed});
            DropEffect Effect = DropEffect::None;
            for (const DropEffect Candidate : {DropEffect::Move, DropEffect::Copy, DropEffect::Link}) {
                if (Allows(Wanted, Candidate) && Allows(Allowed, Candidate)) {
                    Effect = Candidate;
                    break;
                }
            }
            if (Effect != DropEffect::None) {
                Next = {Entry, Effect};
                break;
            }
        }
        if (Current.Box.Box != nullptr && Current.Box.Box != Next.Box.Box) {
            DropHover Previous = Current;
            LeaveDropTarget(Previous);
        }
        return Next;
    }

    void InputDispatcher::LeaveDropTarget(DropHover& Hover) {
        if (Hover.Box.IsAlive()) {
            const auto& OnLeave = static_cast<RenderDropTarget&>(*Hover.Box.Box).GetCallbacks().OnLeave;
            if (OnLeave) { OnLeave(); }
        }
        Hover = {};
    }

    void InputDispatcher::DeliverDrop(
      const DropHover& Hover, const Vec2& Position, const DragData& Data, KeyModifiers Modifiers, DropEffect Allowed) {
        if (Hover.Effect == DropEffect::None || !Hover.Box.IsAlive()) { return; }
        const RenderDropTarget::Callbacks Callbacks = static_cast<RenderDropTarget&>(*Hover.Box.Box).GetCallbacks();
        if (Callbacks.OnDrop) {
            Callbacks.OnDrop({.Data          = &Data,
                              .LocalPosition = Position - Hover.Box.Origin,
                              .Modifiers     = Modifiers,
                              .Allowed       = Hover.Effect & Allowed});
        }
        if (Callbacks.OnLeave) { Callbacks.OnLeave(); }
    }

    DropEffect InputDispatcher::DragOverExternal(const Vec2& Position,
                                                 const DragData& Data,
                                                 KeyModifiers Modifiers,
                                                 DropEffect Allowed) {
        _ExternalOver = FindDropTarget(_ExternalOver, Position, Data, Modifiers, Allowed);
        return _ExternalOver.Effect;
    }

    void InputDispatcher::DragLeaveExternal() {
        LeaveDropTarget(_ExternalOver);
    }

    DropEffect InputDispatcher::DropExternal(const Vec2& Position,
                                             const DragData& Data,
                                             KeyModifiers Modifiers,
                                             DropEffect Allowed) {
        _ExternalOver           = FindDropTarget(_ExternalOver, Position, Data, Modifiers, Allowed);
        const DropHover Hovered = _ExternalOver;
        _ExternalOver           = {};
        DeliverDrop(Hovered, Position, Data, Modifiers, Allowed);
        return Hovered.Effect;
    }

    WindowRegion InputDispatcher::HitTestWindowRegion(const Vec2& Position) const {
        for (const Target& Entry : HitTest(Position)) {
            if (const auto* Button = dynamic_cast<const RenderCaptionButton*>(Entry.Box)) {
                return Button->GetRegion();
            }
            if (dynamic_cast<const RenderWindowDragArea*>(Entry.Box) != nullptr) { return WindowRegion::Caption; }
            if (dynamic_cast<const PointerTarget*>(Entry.Box) != nullptr) { return WindowRegion::Client; }
        }
        return WindowRegion::Client;
    }

    std::vector<InputDispatcher::Target> InputDispatcher::HitTest(const Vec2& Position) const {
        HitTestResult Result(Position);
        _Root.HitTest(Result, Position);

        std::vector<Target> Path;
        Path.reserve(Result.GetEntries().size());
        for (const HitTestResult::Entry& Entry : Result.GetEntries()) {
            Path.push_back({Entry.Target, Entry.Lifetime, Entry.Origin});
        }
        return Path;
    }

    PointerEvent InputDispatcher::Localize(const PointerEvent& Event, const Target& Receiver) {
        PointerEvent Local  = Event;
        Local.LocalPosition = Event.Position - Receiver.Origin;
        return Local;
    }

    RenderGestureDetector& InputDispatcher::AsDetector(const Target& Entry) {
        return static_cast<RenderGestureDetector&>(*Entry.Box);
    }
}  // namespace XenUI
