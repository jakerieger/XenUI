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

#include "XenUI/Tree/Element.hpp"
#include "XenUI/Core/FrameStats.hpp"
#include "XenUI/Core/SlabPool.hpp"

#include <algorithm>
#include <unordered_map>

namespace XenUI {
    Element::Element() = default;

    Element::~Element() = default;

    void* Element::operator new(size_t Size) {
        return SlabPool::Allocate(Size);
    }

    void Element::operator delete(void* Pointer, size_t Size) noexcept {
        SlabPool::Free(Pointer, Size);
    }

    void Element::MarkNeedsBuild() {
        if (!_Mounted || _Dirty || _Owner == nullptr) { return; }
        _Dirty = true;
        _Owner->Schedule(*this);
    }

    void Element::Mount(Element* Parent, BuildOwner& Owner, const Node& Widget) {
        _Parent  = Parent;
        _Owner   = &Owner;
        _Depth   = Parent != nullptr ? Parent->_Depth + 1 : 0;
        _Widget  = Widget;
        _Mounted = true;
    }

    void Element::Update(const Node& NewWidget) {
        _Widget = NewWidget;
    }

    void Element::Unmount() {
        ClearProviderDependencies();
        ClearDependencies();
        if (_Dirty && _Owner != nullptr) { _Owner->Unschedule(*this); }
        _Dirty   = false;
        _Mounted = false;
    }

    void Element::RenderChildChanged() {}

    const void* Element::GetProvidedValue(const std::type_info& Type) const {
        (void)Type;
        return nullptr;
    }

    void Element::AddDependent(Element& Dependent) {
        (void)Dependent;
    }

    void Element::RemoveDependent(Element& Dependent) {
        (void)Dependent;
    }

    void Element::RebuildIfDirty() {
        if (!_Dirty || !_Mounted) { return; }
        _Dirty = false;
        PerformRebuild();
    }

    void Element::OnDependencyChanged() {
        MarkNeedsBuild();
    }

    void Element::ClearProviderDependencies() {
        for (Element* Provider : _Providers) {
            Provider->RemoveDependent(*this);
        }
        _Providers.clear();
    }

    std::unique_ptr<Element> Element::UpdateChild(std::unique_ptr<Element> Child, const Node& NewWidget) {
        if (NewWidget.IsEmpty()) {
            if (Child) { DiscardChild(std::move(Child)); }
            return nullptr;
        }

        if (Child) {
            if (Child->GetWidget().IsSameWidget(NewWidget)) { return Child; }
            if (CanUpdate(*Child, NewWidget)) {
                Child->Update(NewWidget);
                return Child;
            }
            DiscardChild(std::move(Child));
        }
        return InflateWidget(NewWidget);
    }

    std::vector<std::unique_ptr<Element>> Element::UpdateChildren(std::vector<std::unique_ptr<Element>> OldChildren,
                                                                  std::span<const Node> NewWidgets) {
        std::vector<const Node*> Incoming;
        Incoming.reserve(NewWidgets.size());
        for (const Node& Widget : NewWidgets) {
            if (!Widget.IsEmpty()) { Incoming.push_back(&Widget); }
        }

        std::vector<std::unique_ptr<Element>> Result(Incoming.size());
        size_t NewTop    = 0;
        size_t OldTop    = 0;
        size_t NewBottom = Incoming.size();
        size_t OldBottom = OldChildren.size();

        while (NewTop < NewBottom && OldTop < OldBottom && CanUpdate(*OldChildren[OldTop], *Incoming[NewTop])) {
            Result[NewTop] = UpdateChild(std::move(OldChildren[OldTop]), *Incoming[NewTop]);
            ++NewTop;
            ++OldTop;
        }

        while (NewTop < NewBottom && OldTop < OldBottom &&
               CanUpdate(*OldChildren[OldBottom - 1], *Incoming[NewBottom - 1])) {
            --NewBottom;
            --OldBottom;
        }

        std::unordered_map<Key, size_t, KeyHash> KeyedOld;
        for (size_t i = OldTop; i < OldBottom; ++i) {
            const Key& Identity = OldChildren[i]->GetWidget().GetKey();
            if (Identity.IsValid()) {
                KeyedOld.emplace(Identity, i);
            } else {
                DiscardChild(std::move(OldChildren[i]));
            }
        }

        for (; NewTop < NewBottom; ++NewTop) {
            const Node& Widget = *Incoming[NewTop];
            std::unique_ptr<Element> Match;
            if (Widget.GetKey().IsValid()) {
                if (const auto It = KeyedOld.find(Widget.GetKey()); It != KeyedOld.end()) {
                    std::unique_ptr<Element>& Candidate = OldChildren[It->second];
                    if (CanUpdate(*Candidate, Widget)) { Match = std::move(Candidate); }
                    KeyedOld.erase(It);
                }
            }
            Result[NewTop] = UpdateChild(std::move(Match), Widget);
        }

        for (size_t i = OldTop; i < OldBottom; ++i) {
            if (OldChildren[i]) { DiscardChild(std::move(OldChildren[i])); }
        }

        for (size_t NewIndex = NewBottom, OldIndex = OldBottom; NewIndex < Incoming.size(); ++NewIndex, ++OldIndex) {
            Result[NewIndex] = UpdateChild(std::move(OldChildren[OldIndex]), *Incoming[NewIndex]);
        }

        return Result;
    }

    std::unique_ptr<Element> Element::InflateWidget(const Node& Widget) {
        std::unique_ptr<Element> Child(Widget.GetType()->CreateElement());
        Child->Mount(this, *_Owner, Widget);
        return Child;
    }

    void Element::DiscardChild(std::unique_ptr<Element> Child) {
        Child->Unmount();
    }

    bool Element::CanUpdate(const Element& Existing, const Node& NewWidget) {
        const Node& Current = Existing.GetWidget();
        return Current.IsSameType(NewWidget) && Current.GetKey() == NewWidget.GetKey();
    }

    const void* BuildContext::FindProvided(const std::type_info& Type) const {
        for (Element* Ancestor = _Element.GetParent(); Ancestor != nullptr; Ancestor = Ancestor->GetParent()) {
            const void* Value = Ancestor->GetProvidedValue(Type);
            if (Value == nullptr) { continue; }

            std::vector<Element*>& Providers = _Element._Providers;
            if (std::find(Providers.begin(), Providers.end(), Ancestor) == Providers.end()) {
                Providers.push_back(Ancestor);
                Ancestor->AddDependent(_Element);
            }
            return Value;
        }
        return nullptr;
    }

    void BuildOwner::Schedule(Element& Dirty) {
        _DirtyElements.push_back(&Dirty);
        _NeedsSort = true;
        if (!_Building && _DirtyElements.size() == 1 && _OnBuildScheduled) { _OnBuildScheduled(); }
    }

    void BuildOwner::Unschedule(Element& Removed) {
        std::erase(_DirtyElements, &Removed);
    }

    void BuildOwner::BuildDirtyElements() {
        struct BuildingScope {
            explicit BuildingScope(bool& Flag) : Building(Flag) { Building = true; }
            ~BuildingScope() { Building = false; }
            bool& Building;
        } Scope(_Building);

        while (!_DirtyElements.empty()) {
            if (_NeedsSort) {
                std::sort(_DirtyElements.begin(), _DirtyElements.end(), [](const Element* Left, const Element* Right) {
                    return Left->GetDepth() > Right->GetDepth();
                });
                _NeedsSort = false;
            }

            Element* Next = _DirtyElements.back();
            _DirtyElements.pop_back();
            Next->RebuildIfDirty();
        }
    }

    RenderObject* ComponentElement::FindRenderObject() const {
        return _Child ? _Child->FindRenderObject() : nullptr;
    }

    void ComponentElement::VisitChildren(const std::function<void(Element&)>& Visitor) const {
        if (_Child) { Visitor(*_Child); }
    }

    void ComponentElement::Mount(Element* Parent, BuildOwner& Owner, const Node& Widget) {
        Element::Mount(Parent, Owner, Widget);
        OnMounted();
        Rebuild(false);
    }

    void ComponentElement::Update(const Node& NewWidget) {
        const Node Previous = GetWidget();
        Element::Update(NewWidget);
        OnWidgetUpdated(Previous);
        Rebuild(false);
    }

    void ComponentElement::Unmount() {
        if (_Child) {
            _Child->Unmount();
            _Child.reset();
        }
        OnUnmounting();
        Element::Unmount();
    }

    void ComponentElement::RenderChildChanged() {
        if (GetParent() != nullptr) { GetParent()->RenderChildChanged(); }
    }

    void ComponentElement::PerformRebuild() {
        ++FrameCounters::Current().Rebuilt;
        Rebuild(true);
    }

    void ComponentElement::Rebuild(bool NotifyAncestors) {
        ClearDirty();
        ClearProviderDependencies();
        Node Built;
        {
            TrackingScope Scope(*this);
            BuildContext Context(*this);
            Built = Build(Context);
        }

        _Child = UpdateChild(std::move(_Child), Built);
        if (NotifyAncestors) { RenderChildChanged(); }
    }

    void RenderElementBase::VisitChildren(const std::function<void(Element&)>& Visitor) const {
        for (const std::unique_ptr<Element>& Child : _Children) {
            Visitor(*Child);
        }
    }

    void RenderElementBase::Unmount() {
        for (std::unique_ptr<Element>& Child : _Children) {
            Child->Unmount();
        }
        _Children.clear();
        Element::Unmount();
        _RenderObject.reset();
    }

    void RenderElementBase::RenderChildChanged() {
        CollectRenderChildren();
    }

    void RenderElementBase::UpdateSingleChild(const Node& Child) {
        std::unique_ptr<Element> Existing;
        if (!_Children.empty()) { Existing = std::move(_Children.front()); }
        _Children.clear();

        if (std::unique_ptr<Element> Updated = UpdateChild(std::move(Existing), Child)) {
            _Children.push_back(std::move(Updated));
        }
    }

    void RenderElementBase::UpdateMultipleChildren(std::span<const Node> Children) {
        _Children = UpdateChildren(std::move(_Children), Children);
    }

    void RenderElementBase::CollectRenderChildren() {
        if (!_RenderObject) { return; }

        std::vector<RenderObject*> RenderChildren;
        RenderChildren.reserve(_Children.size());
        for (const std::unique_ptr<Element>& Child : _Children) {
            if (RenderObject* Rendered = Child->FindRenderObject()) { RenderChildren.push_back(Rendered); }
        }
        _RenderObject->SetChildren(RenderChildren);
    }
}  // namespace XenUI
