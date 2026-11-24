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

#include "XenUI/Input/Events.hpp"
#include "XenUI/Widgets/Overlay.hpp"

#include <Windows.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace XenUI {
    class WindowImpl;

    /// @brief Posted to the owner window to destroy closed popups outside of their own window procedures.
    inline constexpr UINT kDestroyPopupsMessage = WM_APP + 1;

    /// @brief Native popup windows owned by one window. Popups never take activation: menus receive keyboard
    /// input routed from the owner, and close when the owner is clicked, moved, resized or deactivated.
    class PopupManager final : public PopupPresenter {
    public:
        explicit PopupManager(WindowImpl& Owner);
        ~PopupManager() override;

        PopupManager(const PopupManager&)            = delete;
        PopupManager& operator=(const PopupManager&) = delete;

        uint64_t OpenPopup(PopupRequest Request) override;
        void ClosePopup(uint64_t Id) override;

        /// @brief Dismisses open menus as if the user clicked elsewhere.
        void DismissMenus();
        void DismissAll();

        [[nodiscard]] bool HasMenu() const;

        /// @brief Delivers keyboard input to the topmost menu. Returns whether it was handled.
        bool RouteKey(const KeyEvent& Event);
        bool RouteText(const TextEvent& Event);

        void DestroyClosed();

    private:
        struct Popup {
            uint64_t Id    = 0;
            PopupKind Kind = PopupKind::Menu;
            std::function<void()> OnDismissed;
            std::unique_ptr<WindowImpl> Window;
        };

        void Dismiss(bool MenusOnly);
        [[nodiscard]] Popup* FindTopMenu();

        WindowImpl& _Owner;
        std::vector<Popup> _Open;
        std::vector<std::unique_ptr<WindowImpl>> _Closing;
        uint64_t _NextId = 1;
    };
}  // namespace XenUI
