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

#include <cstdint>
#include <span>
#include <vector>

namespace XenUI {
    class PipelineOwner;

    /// @brief Node of the render tree, which handles layout, painting and hit testing. Owned by its element;
    /// parent and child links are non-owning and maintained by the element tree.
    class XENUI_API RenderObject {
    public:
        RenderObject();
        virtual ~RenderObject();

        RenderObject(const RenderObject&)            = delete;
        RenderObject& operator=(const RenderObject&) = delete;

        static void* operator new(size_t Size);
        static void operator delete(void* Pointer, size_t Size) noexcept;

        [[nodiscard]] RenderObject* GetParent() const { return _Parent; }
        [[nodiscard]] const std::vector<RenderObject*>& GetChildren() const { return _Children; }
        [[nodiscard]] PipelineOwner* GetOwner() const { return _Owner; }
        [[nodiscard]] uint32_t GetDepth() const;
        /// @brief Unique for the life of the process; never reused, unlike the object's address.
        [[nodiscard]] uint64_t GetSerial() const { return _Serial; }

        /// @internal Replaces the child list, reparenting, attaching and detaching as needed.
        void SetChildren(std::span<RenderObject* const> Children);

        /// @internal Associates this subtree with a pipeline that schedules its layout and painting.
        void Attach(PipelineOwner& Owner);
        /// @internal
        void Detach();

    protected:
        virtual void ChildrenChanged() {}
        virtual void OnAttached() {}
        virtual void OnDetached() {}

    private:
        void DetachChild(RenderObject& Child);

        RenderObject* _Parent = nullptr;
        PipelineOwner* _Owner = nullptr;
        std::vector<RenderObject*> _Children;
        uint64_t _Serial = 0;
    };
}  // namespace XenUI
