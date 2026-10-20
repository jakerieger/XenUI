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

#include "XenUI/Render/RenderWindowChrome.hpp"
#include "XenUI/Render/Semantics.hpp"

#include <Windows.h>

namespace XenUI {
    bool RenderCaptionButton::DescribeSemantics(SemanticsProperties& Properties) const {
        const HWND Window = GetOwner() != nullptr ? static_cast<HWND>(GetOwner()->GetNativeParent()) : nullptr;
        if (Window == nullptr) { return false; }

        const bool Maximized = IsZoomed(Window) != FALSE;
        WPARAM Command       = SC_CLOSE;
        const char* Label    = "Close";
        switch (_Region) {
            case WindowRegion::Minimize:
                Command = SC_MINIMIZE;
                Label   = "Minimize";
                break;
            case WindowRegion::Maximize:
                Command = Maximized ? SC_RESTORE : SC_MAXIMIZE;
                Label   = Maximized ? "Restore" : "Maximize";
                break;
            default:
                break;
        }
        Properties = {
          .Role     = SemanticsRole::Button,
          .Label    = Label,
          .OnInvoke = [Window, Command] { PostMessageW(Window, WM_SYSCOMMAND, Command, 0); },
        };
        return true;
    }
}  // namespace XenUI
