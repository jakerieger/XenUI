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
#include "XenUI/Tree/Widget.hpp"
#include "XenUI/Widgets/Controls.hpp"
#include "XenUI/Widgets/Overlay.hpp"

#include <functional>
#include <string>
#include <vector>

namespace XenUI {
    struct DialogAction {
        std::string Label;
        std::function<void()> OnPressed;
        ButtonVariant Variant = ButtonVariant::Secondary;
    };

    /// @brief The panel of a modal dialog: a title, content and a row of action buttons. It takes focus when shown
    /// and keeps Tab inside itself. Escape calls OnDismiss; Enter presses the first primary action unless a focused
    /// control uses it. Show it with ShowDialog, which adds the dimmed barrier behind it.
    struct XENUI_API Dialog {
        std::string Title;
        Node Content;
        std::vector<DialogAction> Actions;
        std::function<void()> OnDismiss;
        float Width = 420.0f;

        [[nodiscard]] Node Build(BuildContext& Context) const;
    };

    /// @brief Shows Description centered over the window, above a barrier that dims and blocks the content
    /// behind it. The dialog stays open while the returned entry is held; reset it to close the dialog, usually
    /// from the actions and OnDismiss. Clicking the barrier calls OnDismiss when DismissOnBarrier is set. Focus
    /// returns to where it was when the dialog closes.
    [[nodiscard]] XENUI_API OverlayEntry ShowDialog(OverlayController& Overlay,
                                                    const PopupStyle& Style,
                                                    Dialog Description,
                                                    bool DismissOnBarrier = true);
}  // namespace XenUI
