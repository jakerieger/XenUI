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
#include "XenUI/Render/RenderBox.hpp"

#include <cstdint>

namespace XenUI {
    /// @brief What a point in a window with a custom title bar is, as far as the system is concerned.
    enum class WindowRegion : uint8_t { Client, Caption, Minimize, Maximize, Close };

    /// @brief Marks its area as part of the title bar: pressing there drags the window, double-clicking maximizes
    /// it and right-clicking opens the system menu, wherever no interactive widget sits above it.
    class XENUI_API RenderWindowDragArea final : public RenderBox {
    protected:
        [[nodiscard]] bool HitTestSelf(const Vec2& Position) const override {
            (void)Position;
            return true;
        }
    };

    /// @brief Reports its area to the system as a minimize, maximize or close button, so the window keeps native
    /// behavior such as Snap Layouts on the maximize button. Presses arrive through the window, not as pointer
    /// events.
    class XENUI_API RenderCaptionButton final : public RenderBox {
    public:
        void SetRegion(WindowRegion Region) { _Region = Region; }
        [[nodiscard]] WindowRegion GetRegion() const { return _Region; }
        [[nodiscard]] bool DescribeSemantics(SemanticsProperties& Properties) const override;

    protected:
        [[nodiscard]] bool HitTestSelf(const Vec2& Position) const override {
            (void)Position;
            return true;
        }

    private:
        WindowRegion _Region = WindowRegion::Close;
    };
}  // namespace XenUI
