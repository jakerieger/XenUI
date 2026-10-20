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

#include "XenUI/Widgets/Scroll.hpp"
#include "XenUI/Widgets/Theme.hpp"

namespace XenUI {
    void ListViewElement::Mount(Element* Parent, BuildOwner& Owner, const Node& Widget) {
        RenderElementBase::Mount(Parent, Owner, Widget);
        _RenderObject = std::make_unique<RenderVirtualList>();
        GetList().SetRangeBuilder([this](int First, int Count) { BuildRange(First, Count); });
        Configure();
    }

    void ListViewElement::Update(const Node& NewWidget) {
        RenderElementBase::Update(NewWidget);
        Configure();
        InvalidateCache();
    }

    void ListViewElement::PerformRebuild() {
        InvalidateCache();
    }

    void ListViewElement::Configure() {
        const ListView& Description = GetWidget().As<ListView>();
        RenderVirtualList& List     = GetList();
        List.SetItemCount(Description.ItemCount);
        List.SetItemExtent(Description.ItemExtent);
        List.SetController(Description.Controller);
        List.SetDirection(Description.Direction);
        List.SetShowScrollbar(Description.ShowScrollbar);
        BuildContext Context(*this);
        const Theme& Style = Theme::Of(Context);
        List.SetScrollbarColors(Style.ScrollbarThumb, Style.ScrollbarTrack);
    }

    void ListViewElement::InvalidateCache() {
        _Cache.clear();
        ClearDependencies();
        GetList().InvalidateItems();
    }

    void ListViewElement::BuildRange(int First, int Count) {
        const ListView& Description = GetWidget().As<ListView>();
        std::erase_if(_Cache, [First, Count](const auto& Entry) {
            return Entry.first < First || Entry.first >= First + Count;
        });

        NodeList Items;
        Items.reserve(static_cast<size_t>(Count));
        {
            TrackingScope Scope(*this, false);
            for (int Index = First; Index < First + Count; ++Index) {
                auto Cached = _Cache.find(Index);
                if (Cached == _Cache.end()) {
                    const Node Built = Description.ItemBuilder ? Description.ItemBuilder(Index) : Node {};
                    Cached           = _Cache.emplace(Index, Keyed(Index, Built)).first;
                }
                Items.push_back(Cached->second);
            }
        }

        UpdateMultipleChildren(Items);
        CollectRenderChildren();
    }
}  // namespace XenUI
