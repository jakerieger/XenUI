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

#include "XenUI/Platform/Linux/PopupManager.hpp"
#include "XenUI/Platform/Linux/WindowImpl.hpp"
#include "XenUI/Render/RenderOverlay.hpp"
#include "XenUI/Widgets/Theme.hpp"
#include "XenUI/Widgets/WidgetHost.hpp"

#include <algorithm>
#include <cmath>

namespace XenUI {
    namespace {
        struct PopupContent {
            std::function<Node()> Builder;

            [[nodiscard]] Node Build(BuildContext& Context) const {
                (void)Context;
                return Builder ? Builder() : Node {};
            }
        };
    }  // namespace

    PopupManager::PopupManager(WindowImpl& Owner) : _Owner(Owner) {}

    PopupManager::~PopupManager() {
        _Open.clear();
        DestroyClosed(true);
    }

    uint64_t PopupManager::OpenPopup(PopupRequest Request) {
        SDL_Window* Owner = _Owner.GetSdlWindow();
        if (Owner == nullptr) { return 0; }

        // Work in the owner's window coordinates, which is what SDL places popups in. The usable area of the
        // owner's display is moved into the same space; on Wayland, where windows can't learn their position, the
        // compositor keeps popups on screen itself.
        const float ToCoordinates = 1.0f / _Owner.GetCoordinateScale();
        const Rect Anchor {Request.Anchor.X * ToCoordinates,
                           Request.Anchor.Y * ToCoordinates,
                           Request.Anchor.Width * ToCoordinates,
                           Request.Anchor.Height * ToCoordinates};
        int OwnerX = 0;
        int OwnerY = 0;
        SDL_GetWindowPosition(Owner, &OwnerX, &OwnerY);
        SDL_Rect Usable {};
        Rect WorkArea {-1.0e6f, -1.0e6f, 2.0e6f, 2.0e6f};
        if (SDL_GetDisplayUsableBounds(SDL_GetDisplayForWindow(Owner), &Usable)) {
            WorkArea = {static_cast<float>(Usable.x - OwnerX),
                        static_cast<float>(Usable.y - OwnerY),
                        static_cast<float>(Usable.w),
                        static_cast<float>(Usable.h)};
        }

        auto Window = std::make_unique<WindowImpl>(PopupWindowOptions {
          .Owner      = Owner,
          .Tooltip    = Request.Kind == PopupKind::Tooltip,
          .Background = Request.Background.value_or(Theme::Dark().SurfaceRaised),
        });

        Window->SetContent(PopupContent {std::move(Request.Builder)});
        const Size Content = Window->MeasurePopup(WorkArea.GetSize() / ToCoordinates);
        const Size Extent {std::ceil(Content.Width * ToCoordinates), std::ceil(Content.Height * ToCoordinates)};
        const Vec2 TopLeft = ComputePopupPosition(Anchor, Extent, WorkArea, Request.Placement);
        Window->PlacePopup({std::round(TopLeft.X), std::round(TopLeft.Y), Extent.Width, Extent.Height});

        const uint64_t Id = _NextId++;
        _Open.push_back({Id, Request.Kind, std::move(Request.OnDismissed), std::move(Window)});
        return Id;
    }

    void PopupManager::ClosePopup(uint64_t Id) {
        const auto Found =
          std::find_if(_Open.begin(), _Open.end(), [Id](const Popup& Entry) { return Entry.Id == Id; });
        if (Found == _Open.end()) { return; }

        Found->Window->Hide();
        _Closing.push_back(std::move(Found->Window));
        _Open.erase(Found);
    }

    void PopupManager::DismissMenus() {
        Dismiss(true);
    }

    void PopupManager::DismissAll() {
        Dismiss(false);
    }

    bool PopupManager::HasMenu() const {
        return std::any_of(_Open.begin(), _Open.end(), [](const Popup& Entry) {
            return Entry.Kind == PopupKind::Menu;
        });
    }

    bool PopupManager::RouteKey(const KeyEvent& Event) {
        Popup* Menu = FindTopMenu();
        if (Menu == nullptr) { return false; }
        WidgetHost* Host = Menu->Window->GetContentHost();
        return Host != nullptr && Host->GetInput().HandleKey(Event);
    }

    bool PopupManager::RouteText(const TextEvent& Event) {
        Popup* Menu = FindTopMenu();
        if (Menu == nullptr) { return false; }
        WidgetHost* Host = Menu->Window->GetContentHost();
        return Host != nullptr && Host->GetInput().HandleText(Event);
    }

    void PopupManager::DestroyClosed(bool Force) {
        // A menu item's action may open a file dialog, whose modal loop runs this while the popup is still handling
        // the click; destroying it then frees the input dispatcher delivering that click.
        std::erase_if(_Closing, [Force](const std::unique_ptr<WindowImpl>& Window) {
            return Force || !Window->IsDispatching();
        });
    }

    void PopupManager::Dismiss(bool MenusOnly) {
        std::vector<std::pair<uint64_t, std::function<void()>>> Targets;
        for (const Popup& Entry : _Open) {
            if (!MenusOnly || Entry.Kind == PopupKind::Menu) { Targets.emplace_back(Entry.Id, Entry.OnDismissed); }
        }

        for (const auto& [Id, Dismissed] : Targets) {
            if (Dismissed) { Dismissed(); }
            ClosePopup(Id);
        }
    }

    PopupManager::Popup* PopupManager::FindTopMenu() {
        for (auto It = _Open.rbegin(); It != _Open.rend(); ++It) {
            if (It->Kind == PopupKind::Menu) { return &*It; }
        }
        return nullptr;
    }
}  // namespace XenUI
