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
#include "XenUI/Core/Animation.hpp"
#include "XenUI/Input/Focus.hpp"
#include "XenUI/Render/RenderLayout.hpp"
#include "XenUI/Tree/Widget.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace XenUI {
    /// @brief One option of a set where only one can be chosen. Usually created by RadioGroup, which also moves the
    /// selection with the arrow keys.
    struct RadioButton {
        bool Selected = false;
        std::function<void()> OnSelected;
        std::string Label;
        bool Enabled = true;
        /// @brief Optional externally owned focus node, so a group can move focus between its options.
        FocusNode* FocusTarget = nullptr;

        class XENUI_API State : public ComponentState<RadioButton> {
        protected:
            Node Build(BuildContext& Context) override;

        private:
            Animated<float> _Dot;
            FocusNode _Focus;
            bool _Shown = false;
        };
    };

    /// @brief A set of mutually exclusive options. Click an option, or use the arrow keys while one has focus to
    /// move the selection.
    struct RadioGroup {
        std::vector<std::string> Options;
        /// @brief The selected option, or -1 for none.
        int SelectedIndex = -1;
        std::function<void(int)> OnChanged;
        Axis Direction = Axis::Vertical;
        float Spacing  = 8.0f;
        bool Enabled   = true;
        /// @brief The name screen readers announce for the group.
        std::string Label;

        class XENUI_API State : public ComponentState<RadioGroup> {
        protected:
            Node Build(BuildContext& Context) override;

        private:
            void Move(int Delta);

            std::vector<std::unique_ptr<FocusNode>> _Focus;
        };
    };
}  // namespace XenUI
