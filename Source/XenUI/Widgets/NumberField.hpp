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
#include "XenUI/Input/Focus.hpp"
#include "XenUI/Input/TextEditingController.hpp"
#include "XenUI/Reactive/Reactive.hpp"
#include "XenUI/Render/RenderControls.hpp"
#include "XenUI/Tree/Widget.hpp"

#include <functional>
#include <limits>
#include <optional>
#include <string>

namespace XenUI {
    /// @brief An editable number with step buttons. Type a value and press Enter or move focus away to apply it;
    /// invalid text reverts. Up and Down (or the wheel while focused, or the step buttons) change it by Step, ten
    /// steps with Shift. Values are clamped to [Min, Max]. Bind a signal to read and write it directly.
    struct NumberField {
        float Value         = 0.0f;
        Signal<float>* Bind = nullptr;
        std::function<void(float)> OnChanged;
        float Min  = std::numeric_limits<float>::lowest();
        float Max  = std::numeric_limits<float>::max();
        float Step = 1.0f;
        /// @brief Digits shown after the decimal point.
        int Decimals = 0;
        /// @brief Shown after the number, such as " dB" or " BPM". Typing it is optional.
        std::string Suffix;
        std::optional<float> Width = 120.0f;
        bool Enabled               = true;
        /// @brief The name screen readers announce.
        std::string Label;

        class XENUI_API State : public ComponentState<NumberField> {
        protected:
            void Init() override;
            void WidgetUpdated(const NumberField& Previous) override;
            void Dispose() override { _Sync.reset(); }
            Node Build(BuildContext& Context) override;

        private:
            [[nodiscard]] BoundValue GetSource() const;
            [[nodiscard]] float Clamp(float Candidate) const;
            [[nodiscard]] std::string Format(float Number) const;
            [[nodiscard]] std::optional<float> Parse(const std::string& Text) const;
            void ShowValue(float Number);
            void Apply();
            void Step(int Direction, bool Coarse);

            TextEditingController _Editor;
            FocusNode _Focus;
            std::optional<Effect> _Sync;
        };
    };
}  // namespace XenUI
