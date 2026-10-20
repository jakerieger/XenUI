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
#include "XenUI/Input/Events.hpp"
#include "XenUI/Reactive/Signal.hpp"

#include <vector>

namespace XenUI {
    class FocusManager;
    class RenderBox;
    class RenderFocus;
    class TextInputClient;

    /// @brief Identity of something that can receive keyboard focus. Own one in a component's state and pass it
    /// to a Focus widget to read or control focus; HasFocus is a tracked signal.
    class XENUI_API FocusNode {
    public:
        FocusNode();
        ~FocusNode();

        FocusNode(const FocusNode&)            = delete;
        FocusNode& operator=(const FocusNode&) = delete;

        [[nodiscard]] bool HasFocus() const { return _Focused.Get(); }

        /// @brief Focuses this node now, or as soon as its Focus widget is attached to a window.
        void RequestFocus();
        void Unfocus();

    private:
        friend class FocusManager;
        friend class RenderFocus;

        Signal<bool> _Focused  = false;
        RenderFocus* _Attached = nullptr;
        FocusManager* _Manager = nullptr;
        bool _FocusRequested   = false;
    };

    /// @brief Tracks the focused node of one widget tree and routes keyboard input through it. Key events travel
    /// from the focused Focus widget up through its ancestors, offering each Focus and Shortcuts widget a chance
    /// to handle them.
    class XENUI_API FocusManager {
    public:
        explicit FocusManager(RenderBox& Root);
        ~FocusManager();

        FocusManager(const FocusManager&)            = delete;
        FocusManager& operator=(const FocusManager&) = delete;

        void SetFocus(FocusNode* Node);
        [[nodiscard]] FocusNode* GetFocused() const { return _Focused; }
        /// @brief The render box of the Focus widget holding focus, or nullptr.
        [[nodiscard]] RenderFocus* GetFocusedBox() const { return _Focused != nullptr ? _Focused->_Attached : nullptr; }

        /// @internal Called when Node leaves the tree. If it had focus, focus returns to the most recently
        /// focused node that is still attached, so closing a popup restores focus to what opened it.
        void Forget(FocusNode& Node);

        /// @brief Focuses the next or previous focusable widget in paint order, wrapping around.
        void MoveFocus(bool Forward);

        /// @brief The text input client of the focused Focus widget, if it has one.
        [[nodiscard]] TextInputClient* GetTextInput() const;

        bool DispatchKey(const KeyEvent& Event);
        bool DispatchText(const TextEvent& Event);

    private:
        static constexpr size_t kHistoryLimit = 16;

        void Assign(FocusNode* Node, bool Remember);

        RenderBox& _Root;
        FocusNode* _Focused = nullptr;
        std::vector<FocusNode*> _History;
    };
}  // namespace XenUI
