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

#include "XenUI/Tree/ElementTree.hpp"

namespace XenUI {
    class RootElement final : public Element {
    public:
        explicit RootElement(ElementTree& Tree) : _Tree(Tree) {}

        void SetChild(const Node& Widget) {
            _Child = UpdateChild(std::move(_Child), Widget);
            _Tree.NotifyRootRenderObjectChanged();
        }

        [[nodiscard]] Element* GetChild() const { return _Child.get(); }

        [[nodiscard]] RenderObject* FindRenderObject() const override {
            return _Child ? _Child->FindRenderObject() : nullptr;
        }

        void VisitChildren(const std::function<void(Element&)>& Visitor) const override {
            if (_Child) { Visitor(*_Child); }
        }

        void Unmount() override {
            if (_Child) {
                _Child->Unmount();
                _Child.reset();
            }
            Element::Unmount();
        }

        void RenderChildChanged() override { _Tree.NotifyRootRenderObjectChanged(); }

    private:
        ElementTree& _Tree;
        std::unique_ptr<Element> _Child;
    };

    ElementTree::ElementTree(const Node& Root)
        : _Owner(std::make_unique<BuildOwner>()), _Root(std::make_unique<RootElement>(*this)) {
        _Root->Mount(nullptr, *_Owner, {});
        if (!Root.IsEmpty()) { SetRoot(Root); }
    }

    ElementTree::~ElementTree() {
        _Root->Unmount();
    }

    void ElementTree::SetRoot(const Node& Root) {
        _Root->SetChild(Root);
    }

    void ElementTree::Build() {
        _Owner->BuildDirtyElements();
    }

    bool ElementTree::NeedsBuild() const {
        return _Owner->HasDirtyElements();
    }

    void ElementTree::SetOnBuildScheduled(std::function<void()> Callback) {
        _Owner->SetOnBuildScheduled(std::move(Callback));
    }

    void ElementTree::SetOnRootRenderObjectChanged(std::function<void(RenderObject*)> Callback) {
        _OnRootRenderObjectChanged = std::move(Callback);
    }

    RenderObject* ElementTree::GetRootRenderObject() const {
        return _Root->FindRenderObject();
    }

    Element* ElementTree::GetRootElement() const {
        return _Root->GetChild();
    }

    void ElementTree::NotifyRootRenderObjectChanged() {
        if (_OnRootRenderObjectChanged) { _OnRootRenderObjectChanged(_Root->FindRenderObject()); }
    }
}  // namespace XenUI
