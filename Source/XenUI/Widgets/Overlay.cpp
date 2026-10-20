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

#include "XenUI/Widgets/Overlay.hpp"

#include <algorithm>
#include <utility>

namespace XenUI {
    PopupPresenter::~PopupPresenter() = default;

    OverlayController* OverlayController::Of(BuildContext& Context) {
        OverlayController* const* Found = Context.Find<OverlayController*>();
        return Found != nullptr ? *Found : nullptr;
    }

    OverlayEntry OverlayController::Show(std::function<Node()> Builder) {
        const uint64_t Id = _NextId++;
        _Entries.Modify([&](std::vector<Entry>& Entries) { Entries.push_back({Id, std::move(Builder)}); });
        return OverlayEntry(*this, Id);
    }

    OverlayEntry OverlayController::ShowPopup(PopupRequest Request) {
        if (_Presenter != nullptr) {
            const uint64_t Id = _NextId++;
            _NativePopups[Id] = _Presenter->OpenPopup(std::move(Request));
            return OverlayEntry(*this, Id);
        }

        if (Request.Kind == PopupKind::Tooltip) {
            return Show([Anchor = Request.Anchor, Placement = Request.Placement, Builder = std::move(Request.Builder)] {
                return IgnorePointer {.Child =
                                        AnchoredPopup {.Anchor = Anchor, .Placement = Placement, .Child = Builder()}};
            });
        }

        return Show([Anchor    = Request.Anchor,
                     Placement = Request.Placement,
                     Builder   = std::move(Request.Builder),
                     Dismissed = std::move(Request.OnDismissed)] {
            return Stack {
              .Fit = StackFit::Expand,
              .Children =
                {
                  ModalBarrier {.OnDismiss = Dismissed},
                  AnchoredPopup {.Anchor = Anchor, .Placement = Placement, .Child = Builder()},
                },
            };
        });
    }

    void OverlayController::Remove(uint64_t Id) {
        if (const auto Native = _NativePopups.find(Id); Native != _NativePopups.end()) {
            const uint64_t PopupId = Native->second;
            _NativePopups.erase(Native);
            if (_Presenter != nullptr) { _Presenter->ClosePopup(PopupId); }
            return;
        }

        _Entries.Modify([Id](std::vector<Entry>& Entries) {
            std::erase_if(Entries, [Id](const Entry& Candidate) { return Candidate.Id == Id; });
        });
    }

    OverlayEntry::~OverlayEntry() {
        Reset();
    }

    OverlayEntry::OverlayEntry(OverlayEntry&& Other) noexcept
        : _Owner(std::exchange(Other._Owner, nullptr)), _Id(std::exchange(Other._Id, 0)) {}

    OverlayEntry& OverlayEntry::operator=(OverlayEntry&& Other) noexcept {
        if (this != &Other) {
            Reset();
            _Owner = std::exchange(Other._Owner, nullptr);
            _Id    = std::exchange(Other._Id, 0);
        }
        return *this;
    }

    void OverlayEntry::Reset() {
        if (_Owner == nullptr) { return; }
        std::exchange(_Owner, nullptr)->Remove(_Id);
        _Id = 0;
    }

    Node OverlayHost::State::Build(BuildContext& Context) {
        (void)Context;
        const OverlayHost& Props = GetWidget();
        _Controller.SetPresenter(Props.Presenter);

        NodeList Layers {Props.Child};
        for (const OverlayController::Entry& Entry : _Controller.GetEntries()) {
            Layers.push_back(Keyed(static_cast<int64_t>(Entry.Id), Entry.Builder()));
        }
        return Provide<OverlayController*> {
          .Value = &_Controller,
          .Child = Stack {.Fit = Props.Expand ? StackFit::Expand : StackFit::Loose, .Children = std::move(Layers)},
        };
    }

    Node ModalBarrier::Build(BuildContext& Context) const {
        (void)Context;
        return Listener {
          .OnDown =
            [Dismiss = OnDismiss](const PointerEvent&) {
                if (Dismiss) { Dismiss(); }
            },
          .Behavior = HitTestBehavior::Opaque,
          .Child    = Tint ? Node(DecoratedBox {.Background = *Tint}) : Node(SizedBox {}),
        };
    }

    Rect GetGlobalBounds(const Element& Source) {
        const auto* Box = dynamic_cast<const RenderBox*>(Source.FindRenderObject());
        if (Box == nullptr) { return {}; }
        return Rect::FromOriginSize(Box->GetGlobalOffset(), Box->GetSize());
    }
}  // namespace XenUI
