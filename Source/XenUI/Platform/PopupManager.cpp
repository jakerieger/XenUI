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

#include "XenUI/Platform/PopupManager.hpp"
#include "XenUI/Platform/WindowImpl.hpp"
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

        Rect ToRect(const RECT& Bounds) {
            return Rect::FromEdges(static_cast<float>(Bounds.left),
                                   static_cast<float>(Bounds.top),
                                   static_cast<float>(Bounds.right),
                                   static_cast<float>(Bounds.bottom));
        }
    }  // namespace

    PopupManager::PopupManager(WindowImpl& Owner) : _Owner(Owner) {}

    PopupManager::~PopupManager() = default;

    uint64_t PopupManager::OpenPopup(PopupRequest Request) {
        const float OwnerScale = _Owner.GetDpiScale();
        POINT ClientOrigin {0, 0};
        ClientToScreen(_Owner.GetHwnd(), &ClientOrigin);
        const Rect Anchor {static_cast<float>(ClientOrigin.x) + Request.Anchor.X * OwnerScale,
                           static_cast<float>(ClientOrigin.y) + Request.Anchor.Y * OwnerScale,
                           Request.Anchor.Width * OwnerScale,
                           Request.Anchor.Height * OwnerScale};

        const POINT AnchorPoint {std::lround(Anchor.X), std::lround(Anchor.Y)};
        MONITORINFO Monitor {.cbSize = sizeof(MONITORINFO)};
        GetMonitorInfoW(MonitorFromPoint(AnchorPoint, MONITOR_DEFAULTTONEAREST), &Monitor);
        const Rect WorkArea = ToRect(Monitor.rcWork);

        auto Window = std::make_unique<WindowImpl>(PopupWindowOptions {
          .Owner        = _Owner.GetHwnd(),
          .Position     = {std::lround(Anchor.Left()), std::lround(Anchor.Bottom())},
          .ClickThrough = Request.Kind == PopupKind::Tooltip,
          .Background   = Request.Background.value_or(Theme::Dark().SurfaceRaised),
        });

        const float Scale = Window->GetDpiScale();
        Window->SetContent(PopupContent {std::move(Request.Builder)});
        const Size Content = Window->MeasurePopup(WorkArea.GetSize() / Scale);
        const Size Pixels {std::ceil(Content.Width * Scale), std::ceil(Content.Height * Scale)};
        const Vec2 TopLeft = ComputePopupPosition(Anchor, Pixels, WorkArea, Request.Placement);

        Window->PlacePopup({std::lround(TopLeft.X),
                            std::lround(TopLeft.Y),
                            std::lround(TopLeft.X + Pixels.Width),
                            std::lround(TopLeft.Y + Pixels.Height)});

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
        if (_Owner.GetHwnd() != nullptr) { PostMessageW(_Owner.GetHwnd(), kDestroyPopupsMessage, 0, 0); }
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

    void PopupManager::DestroyClosed() {
        _Closing.clear();
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
