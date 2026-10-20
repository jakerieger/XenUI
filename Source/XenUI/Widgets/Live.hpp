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

#include "XenUI/Graphics/LiveCanvas.hpp"
#include "XenUI/Render/RenderLive.hpp"

#include <memory>
#include <optional>

namespace XenUI {
    /// @brief Immediate-mode drawing for high-frequency content such as scopes, meters, waveforms and spectrograms.
    /// OnDraw records primitives into a LiveCanvas that is rendered with instanced Direct3D draws. Set Continuous to
    /// redraw every frame (for data read from another thread); otherwise it redraws when signals it read change.
    struct LiveView {
        LiveDrawer OnDraw;
        bool Continuous = false;
        std::optional<float> Width;
        std::optional<float> Height;

        [[nodiscard]] std::unique_ptr<RenderLiveView> CreateRenderObject() const {
            auto Result = std::make_unique<RenderLiveView>();
            UpdateRenderObject(*Result);
            return Result;
        }

        void UpdateRenderObject(RenderLiveView& Target) const {
            Target.SetDrawer(OnDraw, Continuous);
            Target.SetPreferredSize(Width, Height);
        }
    };
}  // namespace XenUI
