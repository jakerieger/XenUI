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
#include "XenUI/Render/RenderScroll.hpp"
#include "XenUI/Tree/Widget.hpp"
#include "XenUI/Widgets/Theme.hpp"
#include "XenUI/Widgets/Basic.hpp"

#include <functional>
#include <memory>
#include <unordered_map>

namespace XenUI {
    struct ClipRect {
        Node Child;

        [[nodiscard]] std::unique_ptr<RenderClipRect> CreateRenderObject() const {
            return std::make_unique<RenderClipRect>();
        }
        void UpdateRenderObject(RenderClipRect& Target) const { (void)Target; }
    };

    /// @brief ScrollView without the repaint boundary around its content. Prefer ScrollView.
    struct RawScrollView {
        ScrollController* Controller = nullptr;
        Axis Direction               = Axis::Vertical;
        bool ShowScrollbar           = true;
        Node Child;
        Color ThumbColor = Color::FromHex(0xFFFFFF, 0.28f);
        Color TrackColor = Color::FromHex(0xFFFFFF, 0.05f);

        [[nodiscard]] std::unique_ptr<RenderScrollView> CreateRenderObject() const {
            auto Result = std::make_unique<RenderScrollView>();
            UpdateRenderObject(*Result);
            return Result;
        }

        void UpdateRenderObject(RenderScrollView& Target) const {
            Target.SetController(Controller);
            Target.SetDirection(Direction);
            Target.SetShowScrollbar(ShowScrollbar);
            Target.SetScrollbarColors(ThumbColor, TrackColor);
        }
    };

    /// @brief Scrolls a child that may be larger than the viewport. Scrolling moves the child's layer without
    /// repainting or relaying out its content.
    struct ScrollView {
        ScrollController* Controller = nullptr;
        Axis Direction               = Axis::Vertical;
        bool ShowScrollbar           = true;
        Node Child;

        [[nodiscard]] Node Build(BuildContext& Context) const {
            const Theme& Style = Theme::Of(Context);
            return RawScrollView {Controller,
                                  Direction,
                                  ShowScrollbar,
                                  RepaintBoundary {.Child = Child},
                                  Style.ScrollbarThumb,
                                  Style.ScrollbarTrack};
        }
    };

    class ListViewElement;

    /// @brief Scrolling list of items with a fixed extent, built on demand: only items in or near the viewport
    /// exist as elements, so lists of any length cost the same. Items are keyed by index, and signals read by
    /// ItemBuilder rebuild the visible items when they change.
    struct ListView {
        using ElementType = ListViewElement;

        int ItemCount    = 0;
        float ItemExtent = 28.0f;
        std::function<Node(int Index)> ItemBuilder;
        ScrollController* Controller = nullptr;
        Axis Direction               = Axis::Vertical;
        bool ShowScrollbar           = true;
    };

    class XENUI_API ListViewElement final : public RenderElementBase {
    public:
        void Mount(Element* Parent, BuildOwner& Owner, const Node& Widget) override;
        void Update(const Node& NewWidget) override;

    protected:
        void PerformRebuild() override;

    private:
        [[nodiscard]] RenderVirtualList& GetList() const { return static_cast<RenderVirtualList&>(*_RenderObject); }
        void Configure();
        void InvalidateCache();
        void BuildRange(int First, int Count);

        std::unordered_map<int, Node> _Cache;
    };
}  // namespace XenUI
