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

#include "XenUI/Widgets/Theme.hpp"
#include "XenUI/Reactive/Signal.hpp"

#include <Windows.h>

namespace XenUI {
    namespace {
        Appearance ReadSystemAppearance() {
            DWORD Light          = 0;
            DWORD Length         = sizeof(Light);
            const LSTATUS Result = RegGetValueW(HKEY_CURRENT_USER,
                                                L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                                                L"AppsUseLightTheme",
                                                RRF_RT_REG_DWORD,
                                                nullptr,
                                                &Light,
                                                &Length);
            return Result == ERROR_SUCCESS && Light != 0 ? Appearance::Light : Appearance::Dark;
        }

        Signal<Appearance>& SystemAppearance() {
            static Signal<Appearance> Current = ReadSystemAppearance();
            return Current;
        }
    }  // namespace

    Theme Theme::Light() {
        return {
          .Background     = Color::FromHex(0xF5F5F7),
          .Surface        = Color::FromHex(0xFFFFFF),
          .SurfaceRaised  = Color::FromHex(0xFBFBFC),
          .Chrome         = Color::FromHex(0xECECEF),
          .Hover          = Color::FromHex(0xE6E6EB),
          .Pressed        = Color::FromHex(0xD9D9E0),
          .Border         = Color::FromHex(0xD2D2DA),
          .Text           = Color::FromHex(0x1B1B20),
          .TextMuted      = Color::FromHex(0x65656F),
          .TextDisabled   = Color::FromHex(0xA6A6B0),
          .Accent         = Color::FromHex(0x0A64D8),
          .AccentText     = Color::FromHex(0xFFFFFF),
          .Track          = Color::FromHex(0xE3E3E8),
          .Overlay        = Color::FromHex(0x000000, 0.0f),
          .Danger         = Color::FromHex(0xD13438),
          .Warning        = Color::FromHex(0xC28A00),
          .Success        = Color::FromHex(0x1E8E4E),
          .ScrollbarThumb = Color::FromHex(0x000000, 0.28f),
          .ScrollbarTrack = Color::FromHex(0x000000, 0.05f),
          .IsDark         = false,
        };
    }

    Theme Theme::FromSystem() {
        return For(GetSystemAppearance());
    }

    Appearance GetSystemAppearance() {
        return SystemAppearance().Get();
    }

    void RefreshSystemAppearance() {
        SystemAppearance() = ReadSystemAppearance();
    }
}  // namespace XenUI
