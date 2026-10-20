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

#include "XenUI/Tree/Component.hpp"
#include "XenUI/Tree/Element.hpp"
#include "XenUI/Tree/Node.hpp"
#include "XenUI/Tree/Provide.hpp"
#include "XenUI/Tree/RenderElement.hpp"
#include "XenUI/Tree/WidgetArena.hpp"

#include <algorithm>
#include <new>
#include <utility>

namespace XenUI {
    template<class T>
    Element* CreateElementFor() {
        if constexpr (CustomElementWidget<T>) {
            return new typename T::ElementType();
        } else if constexpr (kIsProvide<T>) {
            return new ProvideElement<typename T::ValueType>();
        } else if constexpr (StatefulWidget<T>) {
            return new StatefulElement<T>();
        } else if constexpr (StatelessWidget<T>) {
            return new StatelessElement<T>();
        } else {
            return new RenderElement<T>();
        }
    }

    template<class T>
    const WidgetType& GetWidgetType() {
        static const WidgetType Type {
          .Info          = &typeid(T),
          .PayloadOffset = (sizeof(WidgetHeader) + alignof(T) - 1) / alignof(T) * alignof(T),
          .Destroy       = [](void* Payload) noexcept { static_cast<T*>(Payload)->~T(); },
          .CreateElement = &CreateElementFor<T>,
        };
        return Type;
    }

    template<class T>
        requires Widget<std::remove_cvref_t<T>>
    Node::Node(T&& Description) {
        using TWidget = std::remove_cvref_t<T>;

        const WidgetType& Type = GetWidgetType<TWidget>();
        ArenaBlock* Block      = nullptr;
        void* Memory           = WidgetArena::Allocate(Type.PayloadOffset + sizeof(TWidget),
                                             std::max(alignof(WidgetHeader), alignof(TWidget)),
                                             Block);

        try {
            new (static_cast<std::byte*>(Memory) + Type.PayloadOffset) TWidget(std::forward<T>(Description));
        } catch (...) {
            WidgetArena::Release(*Block);
            throw;
        }
        _Header = new (Memory) WidgetHeader {&Type, Block, 1};
    }
}  // namespace XenUI
