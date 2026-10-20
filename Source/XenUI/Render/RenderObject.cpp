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

#include "XenUI/Render/RenderObject.hpp"
#include "XenUI/Core/SlabPool.hpp"

#include <algorithm>
#include <atomic>

namespace XenUI {
    RenderObject::RenderObject() {
        static std::atomic<uint64_t> NextSerial = 1;
        _Serial                                 = NextSerial.fetch_add(1, std::memory_order_relaxed);
    }

    RenderObject::~RenderObject() {
        if (_Parent != nullptr) { _Parent->DetachChild(*this); }
        for (RenderObject* Child : _Children) {
            Child->_Parent = nullptr;
        }
    }

    void* RenderObject::operator new(size_t Size) {
        return SlabPool::Allocate(Size);
    }

    void RenderObject::operator delete(void* Pointer, size_t Size) noexcept {
        SlabPool::Free(Pointer, Size);
    }

    uint32_t RenderObject::GetDepth() const {
        uint32_t Depth = 0;
        for (const RenderObject* Ancestor = _Parent; Ancestor != nullptr; Ancestor = Ancestor->_Parent) {
            ++Depth;
        }
        return Depth;
    }

    void RenderObject::SetChildren(std::span<RenderObject* const> Children) {
        if (std::equal(Children.begin(), Children.end(), _Children.begin(), _Children.end())) { return; }

        for (RenderObject* Previous : _Children) {
            if (std::find(Children.begin(), Children.end(), Previous) != Children.end()) { continue; }
            Previous->_Parent = nullptr;
            if (Previous->_Owner != nullptr) { Previous->Detach(); }
        }

        for (RenderObject* Child : Children) {
            if (Child->_Parent != nullptr && Child->_Parent != this) { Child->_Parent->DetachChild(*Child); }
            Child->_Parent = this;
            if (_Owner != nullptr && Child->_Owner != _Owner) { Child->Attach(*_Owner); }
        }

        _Children.assign(Children.begin(), Children.end());
        ChildrenChanged();
    }

    void RenderObject::Attach(PipelineOwner& Owner) {
        _Owner = &Owner;
        OnAttached();
        for (RenderObject* Child : _Children) {
            Child->Attach(Owner);
        }
    }

    void RenderObject::Detach() {
        for (RenderObject* Child : _Children) {
            Child->Detach();
        }
        OnDetached();
        _Owner = nullptr;
    }

    void RenderObject::DetachChild(RenderObject& Child) {
        std::erase(_Children, &Child);
        Child._Parent = nullptr;
        ChildrenChanged();
    }
}  // namespace XenUI
