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

#include "XenUI/Widgets/WidgetHost.hpp"
#include "XenUI/Tree/Provide.hpp"
#include "XenUI/Widgets/WindowChrome.hpp"
#include "XenUI/Core/Error.hpp"
#include "XenUI/Widgets/Overlay.hpp"
#include "XenUI/Core/Timer.hpp"

#include <chrono>

namespace XenUI {
    WidgetHost::WidgetHost()
        : _View(std::make_unique<RenderView>()), _Input(std::make_unique<InputDispatcher>(*_View)),
          _Tree(std::make_unique<ElementTree>()) {
        _Pipeline.SetFocusManager(&_Input->GetFocusManager());
        _View->Attach(_Pipeline);
        _Pipeline.ScheduleLayout(*_View);
        _Pipeline.SchedulePaint(*_View);

        _Pipeline.SetOnNeedsUpdate([this] { RequestUpdate(); });
        _Tree->SetOnBuildScheduled([this] { RequestUpdate(); });
        _Tree->SetOnRootRenderObjectChanged([this](RenderObject* Root) {
            auto* Box = dynamic_cast<RenderBox*>(Root);
            if (Root != nullptr && Box == nullptr) { throw Error("The root widget must produce a RenderBox"); }
            _View->SetChild(Box);
        });
    }

    WidgetHost::~WidgetHost() {
        _View->SetChild(nullptr);
        _Tree.reset();
    }

    void WidgetHost::SetRoot(const Node& Root) {
        _Tree->SetRoot(Provide<DeveloperHandle> {
          .Value = {this},
          .Child =
            Provide<WindowChromeHandle> {
              .Value = {_Chrome},
              .Child = OverlayHost {.Child = Root, .Expand = !_View->IsSizedToContent(), .Presenter = _Presenter},
            },
        });
        RequestUpdate();
    }

    void WidgetHost::SetViewportSize(const Size& Viewport) {
        _View->SetViewportSize(Viewport);
    }

    void WidgetHost::SetSizeToContent(const Size& Maximum) {
        _View->SetSizeToContent(Maximum);
    }

    void WidgetHost::Update() {
        using Clock        = std::chrono::steady_clock;
        const auto Elapsed = [](Clock::time_point From, Clock::time_point To) {
            return std::chrono::duration<float, std::milli>(To - From).count();
        };
        const double Started    = TimerQueue::Get().Now();
        FrameCounters& Counters = FrameCounters::Current();
        Counters                = {};

        const Clock::time_point Start = Clock::now();
        _Tree->Build();
        const Clock::time_point Built = Clock::now();
        _Pipeline.FlushLayout();
        const Clock::time_point LaidOut = Clock::now();
        _Pipeline.FlushPaint();
        const Clock::time_point Painted = Clock::now();
        _Input->RefreshHover();

        _LastUpdate = {
          .BuildMs   = Elapsed(Start, Built),
          .LayoutMs  = Elapsed(Built, LaidOut),
          .PaintMs   = Elapsed(LaidOut, Painted),
          .Rebuilt   = Counters.Rebuilt,
          .LaidOut   = Counters.LaidOut,
          .Repainted = Counters.Repainted,
        };
        if (_RecordsFrames) {
            _Statistics.Record({
              .Time      = Started,
              .BuildMs   = _LastUpdate.BuildMs,
              .LayoutMs  = _LastUpdate.LayoutMs,
              .PaintMs   = _LastUpdate.PaintMs,
              .Rebuilt   = _LastUpdate.Rebuilt,
              .LaidOut   = _LastUpdate.LaidOut,
              .Repainted = _LastUpdate.Repainted,
            });
        }
    }

    bool WidgetHost::NeedsUpdate() const {
        return _Tree->NeedsBuild() || _Pipeline.HasPendingWork();
    }

    void WidgetHost::SetOnUpdateRequested(std::function<void()> Callback) {
        _OnUpdateRequested = std::move(Callback);
    }

    const std::shared_ptr<Layer>& WidgetHost::GetRootLayer() const {
        return _View->GetLayer();
    }

    RenderBox* WidgetHost::GetRootBox() const {
        RenderBox* Layers = _View->GetFirstChild();
        return Layers != nullptr ? Layers->GetFirstChild() : nullptr;
    }

    void WidgetHost::RequestUpdate() {
        if (_OnUpdateRequested) { _OnUpdateRequested(); }
    }
}  // namespace XenUI
