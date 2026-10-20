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
#include "XenUI/Render/RenderObject.hpp"
#include "XenUI/Tree/Key.hpp"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

namespace XenUI {
    class ArenaBlock;
    class BuildContext;
    class Element;
    class Node;

    template<class TWidget>
    class ComponentState;

    template<class T>
    struct Provide;

    using NodeList = std::vector<Node>;

    template<class T>
    using RenderObjectTypeOf = typename decltype(std::declval<const T&>().CreateRenderObject())::element_type;

    /// @brief A widget whose `Node Build(BuildContext&) const` describes its subtree from its properties alone.
    template<class T>
    concept StatelessWidget = requires(const T& Description, BuildContext& Context) {
        { Description.Build(Context) } -> std::same_as<Node>;
    };

    /// @brief A widget with a nested `State` class (deriving ComponentState) that persists across rebuilds.
    template<class T>
    concept StatefulWidget = requires { typename T::State; } && std::derived_from<typename T::State, ComponentState<T>>;

    /// @brief A leaf of the widget layer that creates and configures a RenderObject. Children are taken from a
    /// `Node Child` or `NodeList Children` member when present.
    template<class T>
    concept RenderWidget =
      requires(const T& Description) { Description.CreateRenderObject(); } &&
      requires(const T& Description, RenderObjectTypeOf<T>& Target) { Description.UpdateRenderObject(Target); } &&
      std::derived_from<RenderObjectTypeOf<T>, RenderObject>;

    template<class T>
    inline constexpr bool kIsProvide = false;

    template<class T>
    inline constexpr bool kIsProvide<Provide<T>> = true;

    /// @brief A widget that names its own element class with `using ElementType = ...;`, for widgets whose
    /// reconciliation does not fit the stateless, stateful or render patterns (for example, lazily built lists).
    template<class T>
    concept CustomElementWidget =
      requires { typename T::ElementType; } && std::derived_from<typename T::ElementType, Element>;

    template<class T>
    concept Widget =
      std::move_constructible<T> && std::destructible<T> &&
      (CustomElementWidget<T> || StatelessWidget<T> || StatefulWidget<T> || RenderWidget<T> || kIsProvide<T>);

    template<class T>
    concept HasSingleChild = requires(const T& Description) {
        { Description.Child } -> std::same_as<const Node&>;
    };

    template<class T>
    concept HasChildren = requires(const T& Description) {
        { Description.Children } -> std::same_as<const NodeList&>;
    };

    struct WidgetType {
        const std::type_info* Info              = nullptr;
        size_t PayloadOffset                    = 0;
        void (*Destroy)(void* Payload) noexcept = nullptr;
        Element* (*CreateElement)()             = nullptr;
    };

    struct WidgetHeader {
        const WidgetType* Type = nullptr;
        ArenaBlock* Block      = nullptr;
        uint32_t RefCount      = 0;
    };

    template<class T>
    const WidgetType& GetWidgetType();

    XENUI_API void DestroyWidget(WidgetHeader* Header) noexcept;

    /// @brief Reference-counted handle to an immutable widget description stored in the WidgetArena. Any Widget
    /// converts to a Node implicitly. Copying is cheap; the description is destroyed with its last handle.
    class Node {
    public:
        Node() = default;

        template<class T>
            requires Widget<std::remove_cvref_t<T>>
        Node(T&& Description);

        Node(const Node& Other) noexcept : _Header(Other._Header), _Key(Other._Key) { Retain(); }
        Node(Node&& Other) noexcept : _Header(std::exchange(Other._Header, nullptr)), _Key(Other._Key) {}

        Node& operator=(const Node& Other) noexcept {
            Node Copy(Other);
            Swap(Copy);
            return *this;
        }

        Node& operator=(Node&& Other) noexcept {
            Node Moved(std::move(Other));
            Swap(Moved);
            return *this;
        }

        ~Node() { Release(); }

        [[nodiscard]] bool IsEmpty() const { return _Header == nullptr; }
        explicit operator bool() const { return _Header != nullptr; }

        [[nodiscard]] const WidgetType* GetType() const { return _Header != nullptr ? _Header->Type : nullptr; }
        [[nodiscard]] const Key& GetKey() const { return _Key; }

        [[nodiscard]] Node WithKey(const Key& NewKey) const {
            Node Copy(*this);
            Copy._Key = NewKey;
            return Copy;
        }

        [[nodiscard]] bool IsSameType(const Node& Other) const {
            if (_Header == nullptr || Other._Header == nullptr) { return false; }
            return _Header->Type == Other._Header->Type || *_Header->Type->Info == *Other._Header->Type->Info;
        }

        /// @brief True when both handles refer to the same description with the same key.
        [[nodiscard]] bool IsSameWidget(const Node& Other) const {
            return _Header == Other._Header && _Key == Other._Key;
        }

        template<class T>
        [[nodiscard]] bool Is() const {
            return _Header != nullptr && *_Header->Type->Info == typeid(T);
        }

        template<class T>
        [[nodiscard]] const T& As() const {
            return *reinterpret_cast<const T*>(reinterpret_cast<const std::byte*>(_Header) +
                                               _Header->Type->PayloadOffset);
        }

    private:
        void Retain() noexcept {
            if (_Header != nullptr) { ++_Header->RefCount; }
        }

        void Release() noexcept {
            if (_Header != nullptr && --_Header->RefCount == 0) { DestroyWidget(_Header); }
            _Header = nullptr;
        }

        void Swap(Node& Other) noexcept {
            std::swap(_Header, Other._Header);
            std::swap(_Key, Other._Key);
        }

        WidgetHeader* _Header = nullptr;
        Key _Key;
    };

    /// @brief Gives Description an identity among its siblings, preserving its element across reorders.
    inline Node Keyed(const Key& Identity, const Node& Description) {
        return Description.WithKey(Identity);
    }
}  // namespace XenUI
