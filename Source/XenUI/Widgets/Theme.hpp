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
#include "XenUI/Core/Color.hpp"
#include "XenUI/Graphics/Text.hpp"
#include "XenUI/Tree/Element.hpp"

#include <cstdint>

namespace XenUI {
    enum class Appearance : uint8_t { Dark, Light };

    /// @brief Colors and metrics shared by the built-in controls. Provide with `Provide<Theme>`; controls fall
    /// back to Theme::Dark() when none is provided. Providing a different theme restyles every control that uses
    /// it, so switching themes at run time only takes providing the new one.
    struct Theme {
        Color Background    = Color::FromHex(0x1B1B20);
        Color Surface       = Color::FromHex(0x26262D);
        Color SurfaceRaised = Color::FromHex(0x30303A);
        /// @brief Title bars, sidebars and other frame-like areas.
        Color Chrome         = Color::FromHex(0x202026);
        Color Hover          = Color::FromHex(0x3A3A46);
        Color Pressed        = Color::FromHex(0x22222A);
        Color Border         = Color::FromHex(0x3A3A44);
        Color Text           = Color::FromHex(0xE6E6EB);
        Color TextMuted      = Color::FromHex(0x8E8E9A);
        Color TextDisabled   = Color::FromHex(0x5A5A66);
        Color Accent         = Color::FromHex(0x4DA3FF);
        Color AccentText     = Color::FromHex(0x0E1A2A);
        Color Track          = Color::FromHex(0x141418);
        Color Overlay        = Color::FromHex(0x000000, 0.0f);
        Color Danger         = Color::FromHex(0xE5484D);
        Color Warning        = Color::FromHex(0xF1C40F);
        Color Success        = Color::FromHex(0x2ECC71);
        Color ScrollbarThumb = Color::FromHex(0xFFFFFF, 0.28f);
        Color ScrollbarTrack = Color::FromHex(0xFFFFFF, 0.05f);

        Font BodyFont  = {.Size = 13.0f};
        Font SmallFont = {.Size = 11.0f};

        float CornerRadius  = 4.0f;
        float ControlHeight = 26.0f;
        bool IsDark         = true;

        [[nodiscard]] static Theme Dark() { return {}; }
        [[nodiscard]] XENUI_API static Theme Light();
        [[nodiscard]] static Theme For(Appearance Mode) { return Mode == Appearance::Light ? Light() : Dark(); }

        /// @brief The theme matching the Windows light or dark app setting. Tracked, so a component that builds
        /// with it rebuilds when the user changes the setting.
        [[nodiscard]] XENUI_API static Theme FromSystem();

        /// @brief The nearest provided theme, or the dark default.
        [[nodiscard]] static const Theme& Of(BuildContext& Context) {
            static const Theme kDefault = Dark();
            const Theme* Provided       = Context.Find<Theme>();
            return Provided != nullptr ? *Provided : kDefault;
        }

        bool operator==(const Theme&) const = default;
    };

    /// @brief The Windows app mode setting. Tracked; updated when the user changes it while the app runs.
    [[nodiscard]] XENUI_API Appearance GetSystemAppearance();

    /// @internal Re-reads the system setting. Windows call this when the system reports a theme change.
    XENUI_API void RefreshSystemAppearance();
}  // namespace XenUI
