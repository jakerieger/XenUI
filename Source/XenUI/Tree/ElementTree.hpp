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
#include "XenUI/Tree/Widget.hpp"

#include <functional>
#include <memory>

namespace XenUI {
    class RootElement;

    /// @brief Owns a mounted element tree and its build scheduling. Windows host one; it can also be driven
    /// directly, for example in tests.
    class XENUI_API ElementTree {
    public:
        explicit ElementTree(const Node& Root = {});
        ~ElementTree();

        ElementTree(const ElementTree&)            = delete;
        ElementTree& operator=(const ElementTree&) = delete;

        /// @brief Reconciles the tree against a new root description.
        void SetRoot(const Node& Root);

        /// @brief Rebuilds every element marked dirty since the last call, shallowest first.
        void Build();

        [[nodiscard]] bool NeedsBuild() const;

        /// @brief Invoked when an element first becomes dirty after a build pass.
        void SetOnBuildScheduled(std::function<void()> Callback);

        /// @brief Invoked whenever the topmost render object may have been replaced, including by SetRoot.
        void SetOnRootRenderObjectChanged(std::function<void(RenderObject*)> Callback);

        [[nodiscard]] RenderObject* GetRootRenderObject() const;

        /// @brief The element created for the root description, or nullptr.
        [[nodiscard]] Element* GetRootElement() const;

    private:
        friend class RootElement;

        void NotifyRootRenderObjectChanged();

        std::unique_ptr<BuildOwner> _Owner;
        std::unique_ptr<RootElement> _Root;
        std::function<void(RenderObject*)> _OnRootRenderObjectChanged;
    };
}  // namespace XenUI
