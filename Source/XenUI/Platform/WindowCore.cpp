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

#include "XenUI/Platform/WindowCore.hpp"
#include "XenUI/AppContext.hpp"
#include "XenUI/Core/Timer.hpp"
#include "XenUI/Graphics/DeviceLost.hpp"
#include "XenUI/Widgets/WidgetHost.hpp"

#include <chrono>
#include <utility>

namespace XenUI {
    WindowCore::WindowCore(const Color& Background, bool IsPopup)
        : _Background(Background), _Compositor([this] { RequestFrame(); }), _IsPopup(IsPopup) {}

    WindowCore::~WindowCore() = default;

    void WindowCore::SetBackground(const Color& NewBackground) {
        _Background = NewBackground;
        _Compositor.AddFullDamage();
    }

    void WindowCore::SetContent(const Node& Root) {
        if (!_Content) {
            _Content = std::make_unique<WidgetHost>();
            _Content->SetRecordsFrames(false);
            _Content->SetOnUpdateRequested([this] { RequestFrame(); });
            _Content->GetInput().SetHost(this);
            if (_IsPopup) {
                _Content->SetSizeToContent(_PopupMaximum);
            } else {
                ConfigureContent(*_Content);
            }
        }
        _Compositor.SetRoot(_Content->GetRootLayer());
        _Content->SetRoot(_IsPopup ? Root : WrapContent(Root));
    }

    void WindowCore::SetRootLayer(std::shared_ptr<Layer> Root) {
        _Content.reset();
        _Compositor.SetRoot(std::move(Root));
    }

    void WindowCore::SetFrameCallback(FrameCallback Callback) {
        _FrameCallback = std::move(Callback);
        RequestFrame();
    }

    void WindowCore::SetShowDamageRegions(bool Enabled) {
        _Compositor.SetShowDamage(Enabled);
    }

    Size WindowCore::MeasurePopup(const Size& Maximum) {
        _PopupMaximum = Maximum;
        if (!_Content) { return {}; }
        _Content->SetSizeToContent(Maximum);
        _Content->Update();
        return _Content->GetLaidOutSize();
    }

    InputDispatcher* WindowCore::GetInput() const {
        return _Content ? &_Content->GetInput() : nullptr;
    }

    bool WindowCore::HasPendingFrame() const {
        return HasRenderTarget() && !_Minimized && (_FrameRequested || _Compositor.HasPendingDamage());
    }

    void WindowCore::RenderFrame() {
        if (!HasRenderTarget()) { return; }

        const double Started  = TimerQueue::Get().Now();
        const auto CallbackAt = std::chrono::steady_clock::now();
        RunFrameCallback();
        const float CallbackMs =
          std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - CallbackAt).count();
        UpdateContent();
        try {
            RenderComposition();
        } catch (const DeviceLostError&) {
            AppContext::Get().HandleDeviceLost();
            return;
        }

        if (_Content) {
            const UpdateTimings& Update        = _Content->GetLastUpdate();
            const Compositor::Timings& Drawing = _Compositor.GetLastTimings();
            _Content->GetFrameStatistics().Record({
              .Time       = Started,
              .CallbackMs = CallbackMs,
              .BuildMs    = Update.BuildMs,
              .LayoutMs   = Update.LayoutMs,
              .PaintMs    = Update.PaintMs,
              .DrawMs     = Drawing.DrawMs,
              .PresentMs  = Drawing.PresentMs,
              .WaitMs     = Drawing.WaitMs,
              .Rebuilt    = Update.Rebuilt,
              .LaidOut    = Update.LaidOut,
              .Repainted  = Update.Repainted,
              .Damage     = Drawing.Damage,
            });
        }
    }

    void WindowCore::UpdateContent() {
        if (!_Content) { return; }
        if (_IsPopup) {
            _Content->SetSizeToContent(_PopupMaximum);
            _Content->Update();
        } else {
            _Content->SetViewportSize(GetClientSize());
            _Content->Update();
        }
        OnContentUpdated();
    }

    void WindowCore::RunFrameCallback() {
        _FrameRequested = false;
        if (!_FrameCallback) { return; }

        const double Now = AppContext::Get().GetTime();
        const FrameInfo Info {
          .Time      = Now,
          .DeltaTime = _Animating ? Now - _LastFrameTime : 0.0,
          .Index     = _FrameIndex++,
        };
        _FrameCallback(Info);

        _Animating     = _FrameRequested;
        _LastFrameTime = Now;
    }
}  // namespace XenUI
