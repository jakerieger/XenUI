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
#include "XenUI/Reactive/Reactive.hpp"
#include "XenUI/Render/RenderObject.hpp"
#include "XenUI/Tree/Node.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <typeinfo>
#include <vector>

namespace XenUI {
    class BuildOwner;

    /// @brief Persistent instance of a widget at one position in the tree. Holds the current description, any state,
    /// and its children; reconciles them against new descriptions on every rebuild. Elements observe the signals
    /// read while building and schedule their own rebuild when those change.
    class XENUI_API Element : public Observer {
    public:
        Element();
        ~Element() override;

        Element(const Element&)            = delete;
        Element& operator=(const Element&) = delete;

        static void* operator new(size_t Size);
        static void operator delete(void* Pointer, size_t Size) noexcept;

        [[nodiscard]] const Node& GetWidget() const { return _Widget; }
        [[nodiscard]] Element* GetParent() const { return _Parent; }
        [[nodiscard]] BuildOwner* GetOwner() const { return _Owner; }
        [[nodiscard]] uint32_t GetDepth() const { return _Depth; }
        [[nodiscard]] bool IsMounted() const { return _Mounted; }
        [[nodiscard]] bool NeedsBuild() const { return _Dirty; }

        /// @brief Schedules this element to rebuild during the owner's next build pass.
        void MarkNeedsBuild();

        /// @brief The render object this element contributes to its nearest render ancestor, if any.
        [[nodiscard]] virtual RenderObject* FindRenderObject() const = 0;

        virtual void VisitChildren(const std::function<void(Element&)>& Visitor) const = 0;

        /// @internal
        virtual void Mount(Element* Parent, BuildOwner& Owner, const Node& Widget);
        /// @internal
        virtual void Update(const Node& NewWidget);
        /// @internal
        virtual void Unmount();
        /// @internal Called when a descendant's render object may have been replaced outside of a parent-driven
        /// update. Identity cannot be compared by address, since pooled allocations are reused immediately.
        virtual void RenderChildChanged();
        /// @internal
        [[nodiscard]] virtual const void* GetProvidedValue(const std::type_info& Type) const;
        /// @internal
        virtual void AddDependent(Element& Dependent);
        /// @internal
        virtual void RemoveDependent(Element& Dependent);
        /// @internal
        void RebuildIfDirty();

    protected:
        virtual void PerformRebuild() {}
        void OnDependencyChanged() override;
        void ClearDirty() { _Dirty = false; }
        void ClearProviderDependencies();

        /// @brief Updates, replaces or removes Child so that it matches NewWidget.
        [[nodiscard]] std::unique_ptr<Element> UpdateChild(std::unique_ptr<Element> Child, const Node& NewWidget);

        /// @brief Reconciles a child list: matches by position from both ends, then by key in the middle.
        /// Unkeyed children in the reordered middle section are recreated.
        [[nodiscard]] std::vector<std::unique_ptr<Element>>
        UpdateChildren(std::vector<std::unique_ptr<Element>> OldChildren, std::span<const Node> NewWidgets);

    private:
        friend class BuildContext;

        [[nodiscard]] std::unique_ptr<Element> InflateWidget(const Node& Widget);
        static void DiscardChild(std::unique_ptr<Element> Child);
        [[nodiscard]] static bool CanUpdate(const Element& Existing, const Node& NewWidget);

        Node _Widget;
        Element* _Parent   = nullptr;
        BuildOwner* _Owner = nullptr;
        std::vector<Element*> _Providers;
        uint32_t _Depth = 0;
        bool _Mounted   = false;
        bool _Dirty     = false;
    };

    /// @brief Handed to Build methods. Locates values provided by ancestors.
    class XENUI_API BuildContext {
    public:
        explicit BuildContext(Element& Owner) : _Element(Owner) {}

        [[nodiscard]] Element& GetElement() const { return _Element; }

        /// @brief Returns the value of the nearest ancestor Provide<T>, or nullptr. The calling component rebuilds
        /// when that value changes. The pointer is only valid during the current Build.
        template<class T>
        [[nodiscard]] const T* Find() const {
            return static_cast<const T*>(FindProvided(typeid(T)));
        }

    private:
        [[nodiscard]] const void* FindProvided(const std::type_info& Type) const;

        Element& _Element;
    };

    /// @brief Collects elements needing a rebuild and rebuilds them shallowest-first, so a parent's rebuild can
    /// update a dirty child before the child would rebuild on its own.
    class XENUI_API BuildOwner {
    public:
        void Schedule(Element& Dirty);
        void Unschedule(Element& Removed);
        void BuildDirtyElements();

        [[nodiscard]] bool HasDirtyElements() const { return !_DirtyElements.empty(); }
        [[nodiscard]] bool IsBuilding() const { return _Building; }

        /// @brief Invoked when the first element becomes dirty, to schedule a build pass.
        void SetOnBuildScheduled(std::function<void()> Callback) { _OnBuildScheduled = std::move(Callback); }

    private:
        std::vector<Element*> _DirtyElements;
        std::function<void()> _OnBuildScheduled;
        bool _NeedsSort = false;
        bool _Building  = false;
    };

    /// @brief Element with exactly one child produced by a Build method.
    class XENUI_API ComponentElement : public Element {
    public:
        [[nodiscard]] RenderObject* FindRenderObject() const override;
        void VisitChildren(const std::function<void(Element&)>& Visitor) const override;
        void Mount(Element* Parent, BuildOwner& Owner, const Node& Widget) override;
        void Update(const Node& NewWidget) override;
        void Unmount() override;
        void RenderChildChanged() override;

    protected:
        virtual Node Build(BuildContext& Context) = 0;
        virtual void OnMounted() {}
        virtual void OnWidgetUpdated(const Node& Previous) { (void)Previous; }
        virtual void OnUnmounting() {}
        void PerformRebuild() override;

    private:
        void Rebuild(bool NotifyAncestors);

        std::unique_ptr<Element> _Child;
    };

    /// @brief Element that owns a RenderObject and attaches its children's render objects to it.
    class XENUI_API RenderElementBase : public Element {
    public:
        [[nodiscard]] RenderObject* FindRenderObject() const override { return _RenderObject.get(); }
        void VisitChildren(const std::function<void(Element&)>& Visitor) const override;
        void Unmount() override;
        void RenderChildChanged() override;

    protected:
        void UpdateSingleChild(const Node& Child);
        void UpdateMultipleChildren(std::span<const Node> Children);
        void CollectRenderChildren();

        std::unique_ptr<RenderObject> _RenderObject;
        std::vector<std::unique_ptr<Element>> _Children;
    };
}  // namespace XenUI
