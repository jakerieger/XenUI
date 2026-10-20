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

#include "XenUI/Tree/Element.hpp"

namespace XenUI {
    template<RenderWidget TWidget>
    class RenderElement final : public RenderElementBase {
    public:
        using RenderType = RenderObjectTypeOf<TWidget>;

        void Mount(Element* Parent, BuildOwner& Owner, const Node& Widget) override {
            RenderElementBase::Mount(Parent, Owner, Widget);
            _RenderObject = GetDescription().CreateRenderObject();
            UpdateChildElements();
        }

        void Update(const Node& NewWidget) override {
            RenderElementBase::Update(NewWidget);
            GetDescription().UpdateRenderObject(static_cast<RenderType&>(*_RenderObject));
            UpdateChildElements();
        }

    private:
        [[nodiscard]] const TWidget& GetDescription() const { return GetWidget().template As<TWidget>(); }

        void UpdateChildElements() {
            if constexpr (HasChildren<TWidget>) {
                UpdateMultipleChildren(GetDescription().Children);
            } else if constexpr (HasSingleChild<TWidget>) {
                UpdateSingleChild(GetDescription().Child);
            }
            CollectRenderChildren();
        }
    };
}  // namespace XenUI
