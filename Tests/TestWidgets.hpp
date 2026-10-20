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

#include <XenUI/XenUI.hpp>

#include <memory>
#include <string>

namespace XenUI::Testing {
    /// @brief One App for every test that needs text or images. Only one App may exist at a time.
    inline App& SharedApp() {
        static App Instance;
        return Instance;
    }

    class RenderNamed : public RenderObject {
    public:
        std::string Name;
    };

    /// @brief Render widget with any number of children.
    struct Box {
        std::string Name;
        NodeList Children;

        [[nodiscard]] std::unique_ptr<RenderNamed> CreateRenderObject() const {
            auto Result  = std::make_unique<RenderNamed>();
            Result->Name = Name;
            return Result;
        }

        void UpdateRenderObject(RenderNamed& Target) const { Target.Name = Name; }
    };

    /// @brief Render widget with at most one child.
    struct Frame {
        std::string Name;
        Node Child;

        [[nodiscard]] std::unique_ptr<RenderNamed> CreateRenderObject() const {
            auto Result  = std::make_unique<RenderNamed>();
            Result->Name = Name;
            return Result;
        }

        void UpdateRenderObject(RenderNamed& Target) const { Target.Name = Name; }
    };

    /// @brief Leaf render widget.
    struct Label {
        std::string Text;

        [[nodiscard]] std::unique_ptr<RenderNamed> CreateRenderObject() const {
            auto Result  = std::make_unique<RenderNamed>();
            Result->Name = Text;
            return Result;
        }

        void UpdateRenderObject(RenderNamed& Target) const { Target.Name = Text; }
    };

    inline const std::string& NameOf(const RenderObject* Target) {
        static const std::string kNone = "<null>";
        return Target != nullptr ? static_cast<const RenderNamed*>(Target)->Name : kNone;
    }

    inline std::string DescribeChildren(const RenderObject* Target) {
        std::string Result;
        for (const RenderObject* Child : Target->GetChildren()) {
            if (!Result.empty()) { Result += ","; }
            Result += NameOf(Child);
        }
        return Result;
    }
}  // namespace XenUI::Testing
