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

#include "XenUI/Render/Semantics.hpp"
#include "XenUI/Input/Focus.hpp"
#include "XenUI/Render/RenderInput.hpp"

#include <algorithm>
#include <limits>

namespace XenUI {
    namespace {
        constexpr float kHuge = std::numeric_limits<float>::max() / 4.0f;

        void AppendText(std::string& Text, const std::string& Piece) {
            if (Piece.empty()) { return; }
            if (!Text.empty()) { Text += ' '; }
            Text += Piece;
        }
    }  // namespace

    struct SemanticsTree::BuildState {
        std::unordered_map<uint64_t, SemanticsNode> Nodes;
        std::unordered_map<uint64_t, Source> Sources;
        std::unordered_set<RenderFocus*> Claimed;
        RenderFocus* FocusedBox = nullptr;
        uint64_t Focused        = kRootId;

        RenderFocus* Claim(RenderFocus* Candidate) {
            if (Candidate == nullptr || Claimed.contains(Candidate)) { return nullptr; }
            Claimed.insert(Candidate);
            return Candidate;
        }

        /// @brief The first unclaimed focus box inside a leaf node's subtree.
        RenderFocus* FindFocus(RenderObject& Object) {
            for (RenderObject* Child : Object.GetChildren()) {
                if (auto* Focusable = dynamic_cast<RenderFocus*>(Child);
                    Focusable != nullptr && Focusable->CanRequestFocus() && !Claimed.contains(Focusable)) {
                    return Focusable;
                }
                if (RenderFocus* Found = FindFocus(*Child)) { return Found; }
            }
            return nullptr;
        }

        /// @brief The text inside a leaf node, for its name.
        static void CollectText(RenderObject& Object, std::string& Text) {
            for (RenderObject* Child : Object.GetChildren()) {
                SemanticsProperties Inner;
                if (const auto* Box = dynamic_cast<const RenderBox*>(Child);
                    Box != nullptr && Box->DescribeSemantics(Inner)) {
                    if (Inner.Role == SemanticsRole::Excluded) { continue; }
                    if (!Inner.Label.empty()) {
                        AppendText(Text, Inner.Label);
                        continue;
                    }
                }
                CollectText(*Child, Text);
            }
        }
    };

    bool IsLeafRole(SemanticsRole Role) {
        switch (Role) {
            case SemanticsRole::Text:
            case SemanticsRole::Image:
            case SemanticsRole::Button:
            case SemanticsRole::CheckBox:
            case SemanticsRole::Switch:
            case SemanticsRole::Slider:
            case SemanticsRole::Edit:
            case SemanticsRole::ComboBox:
            case SemanticsRole::ListItem:
            case SemanticsRole::MenuItem:
                return true;
            default:
                return false;
        }
    }

    bool SemanticsProperties::HasSameData(const SemanticsProperties& Other) const {
        return Role == Other.Role && Label == Other.Label && Description == Other.Description &&
               Identifier == Other.Identifier && Value == Other.Value && Range == Other.Range &&
               Toggled == Other.Toggled && Expanded == Other.Expanded && Selected == Other.Selected &&
               Enabled == Other.Enabled && ReadOnly == Other.ReadOnly;
    }

    bool RenderSemantics::DescribeSemantics(SemanticsProperties& Properties) const {
        if (!_Hint.empty()) { return false; }
        Properties = _Properties;
        if (Properties.Range && Properties.ReadRange) { Properties.Range->Value = Properties.ReadRange(); }
        return true;
    }

    SemanticsTree::SemanticsTree() {
        _Nodes[kRootId] = SemanticsNode {.Id = kRootId, .Properties = {.Role = SemanticsRole::Pane}};
    }

    SemanticsTree::~SemanticsTree() = default;

    SemanticsUpdate SemanticsTree::Rebuild(RenderBox& Root, const FocusManager* Focus) {
        BuildState State;
        State.FocusedBox = Focus != nullptr ? Focus->GetFocusedBox() : nullptr;

        const Rect Window    = Rect::FromOriginSize(Root.GetGlobalOffset(), Root.GetSize());
        State.Nodes[kRootId] = SemanticsNode {
          .Id            = kRootId,
          .Properties    = {.Role = SemanticsRole::Pane},
          .Bounds        = Window,
          .VisibleBounds = Window,
        };
        Visit(Root, State, kRootId, {.Clip = Rect {-kHuge, -kHuge, kHuge * 2.0f, kHuge * 2.0f}});

        SemanticsUpdate Update;
        Update.FocusChanged = State.Focused != _Focused;
        for (const auto& [Id, Node] : State.Nodes) {
            const auto Previous = _Nodes.find(Id);
            if (Previous == _Nodes.end()) { continue; }
            const SemanticsNode& Old = Previous->second;
            if (Old.Children != Node.Children) { Update.StructureChanged.push_back(Id); }

            const SemanticsProperties& Before = Old.Properties;
            const SemanticsProperties& After  = Node.Properties;
            const auto Report                 = [&](bool Changed, SemanticsUpdate::Change What) {
                if (Changed) { Update.Properties.push_back({Id, What}); }
            };
            Report(Before.Label != After.Label, SemanticsUpdate::Change::Name);
            Report(Before.Value != After.Value, SemanticsUpdate::Change::Value);
            Report(Before.Range != After.Range, SemanticsUpdate::Change::Range);
            Report(Before.Toggled != After.Toggled, SemanticsUpdate::Change::Toggle);
            Report(Before.Expanded != After.Expanded, SemanticsUpdate::Change::Expanded);
            Report(Before.Selected != After.Selected, SemanticsUpdate::Change::Selected);
            Report(Before.Enabled != After.Enabled, SemanticsUpdate::Change::Enabled);
        }

        _Nodes   = std::move(State.Nodes);
        _Sources = std::move(State.Sources);
        _Focused = State.Focused;
        return Update;
    }

    void SemanticsTree::Visit(RenderObject& Object, BuildState& State, uint64_t Parent, Inherited Context) {
        auto* Box = dynamic_cast<RenderBox*>(&Object);
        if (Box == nullptr) {
            for (RenderObject* Child : Object.GetChildren()) {
                Visit(*Child, State, Parent, Context);
            }
            return;
        }

        if (auto* Focusable = dynamic_cast<RenderFocus*>(Box); Focusable != nullptr && Focusable->CanRequestFocus()) {
            Context.Focus = Focusable;
        }
        if (const auto* Hinted = dynamic_cast<const RenderSemantics*>(Box);
            Hinted != nullptr && !Hinted->GetHint().empty()) {
            Context.Hint = &Hinted->GetHint();
        }

        const Rect Bounds = Rect::FromOriginSize(Box->GetGlobalOffset(), Box->GetSize());
        SemanticsProperties Properties;
        if (!Box->DescribeSemantics(Properties)) {
            if (Box->ClipsChildren()) { Context.Clip = Context.Clip.Intersection(Bounds); }
            for (RenderObject* Child : Object.GetChildren()) {
                Visit(*Child, State, Parent, Context);
            }
            return;
        }
        if (Properties.Role == SemanticsRole::Excluded) { return; }

        const uint64_t Id = Box->GetSerial();
        const bool Leaf   = IsLeafRole(Properties.Role);
        const bool IsText = Properties.Role == SemanticsRole::Text;
        const Rect Clip   = Context.Clip;

        RenderFocus* FocusBox = !IsText ? State.Claim(Context.Focus) : nullptr;
        if (FocusBox == nullptr && Leaf && !IsText) { FocusBox = State.Claim(State.FindFocus(*Box)); }
        if (Leaf && Properties.Label.empty()) { BuildState::CollectText(*Box, Properties.Label); }
        if (Context.Hint != nullptr && !IsText) {
            if (Properties.Label.empty()) {
                Properties.Label = *Context.Hint;
            } else if (Properties.Description.empty() && Properties.Label != *Context.Hint) {
                Properties.Description = *Context.Hint;
            }
        }

        SemanticsNode Node {
          .Id            = Id,
          .Parent        = Parent,
          .Properties    = std::move(Properties),
          .Bounds        = Bounds,
          .VisibleBounds = Clip.Intersection(Bounds),
          .Focusable     = FocusBox != nullptr,
        };
        Source Origin {.Box = Box, .Lifetime = Box->GetLifetimeToken()};
        if (FocusBox != nullptr) {
            Origin.FocusBox      = FocusBox;
            Origin.FocusLifetime = FocusBox->GetLifetimeToken();
            if (FocusBox == State.FocusedBox) { State.Focused = Id; }
        }
        State.Nodes[Parent].Children.push_back(Id);
        State.Nodes[Id]   = std::move(Node);
        State.Sources[Id] = std::move(Origin);

        if (Leaf) { return; }
        const Inherited Inner {.Clip = Box->ClipsChildren() ? Clip.Intersection(Bounds) : Clip};
        for (RenderObject* Child : Object.GetChildren()) {
            Visit(*Child, State, Id, Inner);
        }
    }

    const SemanticsNode* SemanticsTree::Find(uint64_t Id) const {
        const auto Found = _Nodes.find(Id);
        return Found != _Nodes.end() ? &Found->second : nullptr;
    }

    uint64_t SemanticsTree::HitTest(const Vec2& Position) const {
        uint64_t Current = kRootId;
        for (;;) {
            const SemanticsNode& Node = *Find(Current);
            uint64_t Deeper           = kRootId;
            for (auto Child = Node.Children.rbegin(); Child != Node.Children.rend(); ++Child) {
                if (Find(*Child)->VisibleBounds.Contains(Position)) {
                    Deeper = *Child;
                    break;
                }
            }
            if (Deeper == kRootId) { return Current; }
            Current = Deeper;
        }
    }

    const SemanticsNode* SemanticsTree::FindByLabel(std::string_view Label) const {
        std::vector<uint64_t> Pending {kRootId};
        while (!Pending.empty()) {
            const SemanticsNode& Node = *Find(Pending.front());
            Pending.erase(Pending.begin());
            if (Node.Id != kRootId && (Node.Properties.Label == Label || Node.Properties.Identifier == Label)) {
                return &Node;
            }
            Pending.insert(Pending.end(), Node.Children.begin(), Node.Children.end());
        }
        return nullptr;
    }

    std::optional<SemanticsProperties> SemanticsTree::Describe(uint64_t Id) const {
        const auto Found = _Sources.find(Id);
        if (Found == _Sources.end() || Found->second.Lifetime.expired()) { return std::nullopt; }

        SemanticsProperties Properties;
        if (!Found->second.Box->DescribeSemantics(Properties) || !Properties.Enabled) { return std::nullopt; }
        return Properties;
    }

    bool SemanticsTree::Invoke(uint64_t Id) const {
        const std::optional<SemanticsProperties> Properties = Describe(Id);
        if (!Properties || !Properties->OnInvoke) { return false; }
        Properties->OnInvoke();
        return true;
    }

    bool SemanticsTree::Toggle(uint64_t Id) const {
        const std::optional<SemanticsProperties> Properties = Describe(Id);
        if (!Properties || !Properties->OnToggle) { return false; }
        Properties->OnToggle();
        return true;
    }

    bool SemanticsTree::SetRangeValue(uint64_t Id, float Value) const {
        const std::optional<SemanticsProperties> Properties = Describe(Id);
        if (!Properties || !Properties->OnSetRange || !Properties->Range) { return false; }
        const SemanticsRange& Range = *Properties->Range;
        Properties->OnSetRange(std::clamp(Value, std::min(Range.Min, Range.Max), std::max(Range.Min, Range.Max)));
        return true;
    }

    bool SemanticsTree::SetValue(uint64_t Id, const std::string& Value) const {
        const std::optional<SemanticsProperties> Properties = Describe(Id);
        if (!Properties || !Properties->OnSetValue || Properties->ReadOnly) { return false; }
        Properties->OnSetValue(Value);
        return true;
    }

    bool SemanticsTree::SetExpanded(uint64_t Id, bool Expanded) const {
        const std::optional<SemanticsProperties> Properties = Describe(Id);
        if (!Properties || !Properties->OnExpand) { return false; }
        Properties->OnExpand(Expanded);
        return true;
    }

    bool SemanticsTree::Select(uint64_t Id) const {
        const std::optional<SemanticsProperties> Properties = Describe(Id);
        if (!Properties || !Properties->OnSelect) { return false; }
        Properties->OnSelect();
        return true;
    }

    bool SemanticsTree::Focus(uint64_t Id) const {
        const auto Found = _Sources.find(Id);
        if (Found == _Sources.end() || Found->second.FocusBox == nullptr || Found->second.FocusLifetime.expired()) {
            return false;
        }
        Found->second.FocusBox->GetNode().RequestFocus();
        return true;
    }
}  // namespace XenUI
