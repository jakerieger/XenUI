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

#include "XenUI/Input/Focus.hpp"
#include "XenUI/Input/TextInput.hpp"
#include "XenUI/Render/RenderInput.hpp"

#include <algorithm>
#include <vector>

namespace XenUI {
    namespace {
        /// @brief Whether a key would type a character into a text field.
        bool TypesText(const KeyEvent& Event) {
            if (HasFlags(Event.Modifiers, KeyModifiers::Control) || HasFlags(Event.Modifiers, KeyModifiers::Alt)) {
                return false;
            }
            const auto Code = static_cast<uint16_t>(Event.Code);
            return Event.Code == KeyCode::Space || (Code >= 0x30 && Code <= 0x5A) || (Code >= 0x60 && Code <= 0x6F) ||
                   (Code >= 0xBA && Code <= 0xDE);
        }

        void CollectFocusable(RenderObject& Node, std::vector<RenderFocus*>& Result) {
            if (auto* Focusable = dynamic_cast<RenderFocus*>(&Node);
                Focusable != nullptr && Focusable->CanRequestFocus()) {
                Result.push_back(Focusable);
            }
            for (RenderObject* Child : Node.GetChildren()) {
                CollectFocusable(*Child, Result);
            }
        }
    }  // namespace

    TextInputClient::~TextInputClient() = default;

    FocusNode::FocusNode() = default;

    FocusNode::~FocusNode() {
        Unfocus();
        if (_Attached != nullptr) { _Attached->ReleaseNode(*this); }
    }

    void FocusNode::RequestFocus() {
        if (_Manager != nullptr && _Attached != nullptr) {
            _Manager->SetFocus(this);
            return;
        }
        _FocusRequested = true;
    }

    void FocusNode::Unfocus() {
        _FocusRequested = false;
        if (_Manager != nullptr && _Manager->GetFocused() == this) { _Manager->SetFocus(nullptr); }
    }

    FocusManager::FocusManager(RenderBox& Root) : _Root(Root) {}

    FocusManager::~FocusManager() {
        _History.clear();
        Assign(nullptr, false);
    }

    void FocusManager::SetFocus(FocusNode* Node) {
        Assign(Node, true);
    }

    void FocusManager::Forget(FocusNode& Node) {
        std::erase(_History, &Node);
        if (_Focused != &Node) { return; }

        while (!_History.empty()) {
            FocusNode* const Candidate = _History.back();
            _History.pop_back();
            if (Candidate->_Attached != nullptr && Candidate->_Manager == this) {
                Assign(Candidate, false);
                return;
            }
        }
        Assign(nullptr, false);
    }

    void FocusManager::Assign(FocusNode* Node, bool Remember) {
        if (Node == _Focused) { return; }
        FocusNode* const Previous = _Focused;
        if (Remember && Previous != nullptr) {
            std::erase(_History, Previous);
            _History.push_back(Previous);
            if (_History.size() > kHistoryLimit) { _History.erase(_History.begin()); }
        }
        if (Node != nullptr) { std::erase(_History, Node); }

        _Focused = Node;
        if (Previous != nullptr) { Previous->_Focused = false; }
        if (Node != nullptr) { Node->_Focused = true; }
    }

    void FocusManager::MoveFocus(bool Forward) {
        std::vector<RenderFocus*> Focusable;
        CollectFocusable(_Root, Focusable);
        if (Focusable.empty()) { return; }

        const auto Current = std::find_if(Focusable.begin(), Focusable.end(), [this](const RenderFocus* Candidate) {
            return &Candidate->GetNode() == _Focused;
        });

        size_t Next = 0;
        if (Current == Focusable.end()) {
            Next = Forward ? 0 : Focusable.size() - 1;
        } else {
            const size_t Index = static_cast<size_t>(Current - Focusable.begin());
            Next = Forward ? (Index + 1) % Focusable.size() : (Index + Focusable.size() - 1) % Focusable.size();
        }
        Focusable[Next]->GetNode().RequestFocus();
    }

    TextInputClient* FocusManager::GetTextInput() const {
        return _Focused != nullptr && _Focused->_Attached != nullptr ? _Focused->_Attached->GetTextInput() : nullptr;
    }

    bool FocusManager::DispatchKey(const KeyEvent& Event) {
        if (_Focused == nullptr || _Focused->_Attached == nullptr) { return false; }

        const bool Typing = _Focused->_Attached->GetTextInput() != nullptr && TypesText(Event);
        if (!Typing) {
            for (RenderObject* Node = _Focused->_Attached; Node != nullptr; Node = Node->GetParent()) {
                if (auto* Shortcuts = dynamic_cast<RenderShortcuts*>(Node);
                    Shortcuts != nullptr && Shortcuts->HandlePreemptingKey(Event)) {
                    return true;
                }
            }
        }

        for (RenderObject* Node = _Focused->_Attached; Node != nullptr; Node = Node->GetParent()) {
            if (auto* Focusable = dynamic_cast<RenderFocus*>(Node)) {
                if (Focusable->HandleKey(Event)) { return true; }
            } else if (auto* Shortcuts = dynamic_cast<RenderShortcuts*>(Node)) {
                if (Shortcuts->HandleKey(Event)) { return true; }
            }
        }
        return false;
    }

    bool FocusManager::DispatchText(const TextEvent& Event) {
        if (_Focused == nullptr || _Focused->_Attached == nullptr) { return false; }

        for (RenderObject* Node = _Focused->_Attached; Node != nullptr; Node = Node->GetParent()) {
            if (auto* Focusable = dynamic_cast<RenderFocus*>(Node);
                Focusable != nullptr && Focusable->HandleText(Event)) {
                return true;
            }
        }
        return false;
    }
}  // namespace XenUI
