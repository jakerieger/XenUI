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
        _Tree->SetRoot(Provide<WindowChromeHandle> {
          .Value = {_Chrome},
          .Child = OverlayHost {.Child = Root, .Expand = !_View->IsSizedToContent(), .Presenter = _Presenter},
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
        _Tree->Build();
        _Pipeline.FlushLayout();
        _Pipeline.FlushPaint();
        _Input->RefreshHover();
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
