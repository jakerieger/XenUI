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

#include "XenUI/Render/Semantics.hpp"
#include "XenUI/Tree/Widget.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace XenUI {
    /// @brief Describes its child to assistive technology such as screen readers. The built-in controls describe
    /// themselves; use this for custom widgets, or to label groups. Actions are offered when their callback is set.
    struct Semantics {
        SemanticsRole Role = SemanticsRole::Group;
        std::string Label;
        std::string Description;
        std::string Identifier;
        std::optional<std::string> Value;
        std::optional<SemanticsRange> Range;
        std::optional<bool> Toggled;
        std::optional<bool> Expanded;
        std::optional<bool> Selected;
        bool Enabled  = true;
        bool ReadOnly = false;
        std::function<float()> ReadRange;
        std::function<void()> OnInvoke;
        std::function<void()> OnToggle;
        std::function<void(float)> OnSetRange;
        std::function<void(const std::string&)> OnSetValue;
        std::function<void(bool)> OnExpand;
        std::function<void()> OnSelect;
        Node Child;

        [[nodiscard]] std::unique_ptr<RenderSemantics> CreateRenderObject() const {
            auto Result = std::make_unique<RenderSemantics>();
            UpdateRenderObject(*Result);
            return Result;
        }

        void UpdateRenderObject(RenderSemantics& Target) const {
            Target.SetProperties({
              .Role        = Role,
              .Label       = Label,
              .Description = Description,
              .Identifier  = Identifier,
              .Value       = Value,
              .Range       = Range,
              .Toggled     = Toggled,
              .Expanded    = Expanded,
              .Selected    = Selected,
              .Enabled     = Enabled,
              .ReadOnly    = ReadOnly,
              .ReadRange   = ReadRange,
              .OnInvoke    = OnInvoke,
              .OnToggle    = OnToggle,
              .OnSetRange  = OnSetRange,
              .OnSetValue  = OnSetValue,
              .OnExpand    = OnExpand,
              .OnSelect    = OnSelect,
            });
        }
    };

    /// @brief Gives Hint to the first accessible node inside its child: as its description, or as its name when it
    /// has none. Tooltips use it, so their message reaches screen readers.
    struct SemanticsHint {
        std::string Hint;
        Node Child;

        [[nodiscard]] std::unique_ptr<RenderSemantics> CreateRenderObject() const {
            auto Result = std::make_unique<RenderSemantics>();
            UpdateRenderObject(*Result);
            return Result;
        }

        void UpdateRenderObject(RenderSemantics& Target) const {
            Target.SetProperties({});
            Target.SetHint(Hint);
        }
    };

    /// @brief Hides its child from assistive technology, for decoration that would only add noise.
    struct ExcludeSemantics {
        Node Child;

        [[nodiscard]] std::unique_ptr<RenderSemantics> CreateRenderObject() const {
            auto Result = std::make_unique<RenderSemantics>();
            UpdateRenderObject(*Result);
            return Result;
        }

        void UpdateRenderObject(RenderSemantics& Target) const {
            Target.SetProperties({.Role = SemanticsRole::Excluded});
        }
    };
}  // namespace XenUI
