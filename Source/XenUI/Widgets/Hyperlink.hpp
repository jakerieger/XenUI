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
#include "XenUI/Graphics/Text.hpp"
#include "XenUI/Input/Focus.hpp"
#include "XenUI/Reactive/Signal.hpp"
#include "XenUI/Tree/Widget.hpp"

#include <functional>
#include <optional>
#include <string>

namespace XenUI {
    /// @brief Text in the accent color that opens Url when clicked, or with Enter or Space while focused. The
    /// cursor becomes a hand over it, and screen readers announce it as a link.
    struct Hyperlink {
        /// @brief The visible text; Url when empty.
        std::string Label;
        /// @brief Opened with the system's default handler. Ignored when OnPressed is set.
        std::string Url;
        /// @brief Runs instead of opening Url, for links that lead to another view.
        std::function<void()> OnPressed;
        std::optional<XenUI::Font> Font;
        bool Enabled = true;

        class XENUI_API State : public ComponentState<Hyperlink> {
        protected:
            Node Build(BuildContext& Context) override;

        private:
            void Activate() const;

            Signal<bool> _Hovered = false;
            FocusNode _Focus;
        };
    };
}  // namespace XenUI
