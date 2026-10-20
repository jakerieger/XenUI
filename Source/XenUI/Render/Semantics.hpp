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
#include "XenUI/Core/Math.hpp"
#include "XenUI/Render/RenderBox.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace XenUI {
    class FocusManager;
    class RenderFocus;

    /// @brief What a node of the accessibility tree is, which decides how screen readers announce it.
    enum class SemanticsRole : uint8_t {
        /// @brief Hides the subtree from assistive technology, for decoration.
        Excluded,
        Group,
        Pane,
        Text,
        Image,
        Button,
        CheckBox,
        Switch,
        Slider,
        Edit,
        ComboBox,
        List,
        ListItem,
        Menu,
        MenuItem,
        ToolBar,
    };

    /// @brief Whether nodes with Role are announced as a single control, with their name taken from the text
    /// inside them, rather than as a container whose children are visited.
    [[nodiscard]] XENUI_API bool IsLeafRole(SemanticsRole Role);

    struct SemanticsRange {
        float Value = 0.0f;
        float Min   = 0.0f;
        float Max   = 1.0f;
        /// @brief The amount one arrow key press changes the value by.
        float Step = 0.05f;

        bool operator==(const SemanticsRange&) const = default;
    };

    /// @brief How a render box describes itself to assistive technology: what it is, what it says and what can be
    /// done with it. Each action is offered only when its callback is set.
    struct SemanticsProperties {
        SemanticsRole Role = SemanticsRole::Group;
        /// @brief The name that is announced. Leaf nodes without one are named after the text inside them.
        std::string Label;
        /// @brief Extra help, such as a tooltip.
        std::string Description;
        /// @brief A stable name for test automation; not announced.
        std::string Identifier;
        /// @brief Text value, for edits and combo boxes.
        std::optional<std::string> Value;
        std::optional<SemanticsRange> Range;
        std::optional<bool> Toggled;
        std::optional<bool> Expanded;
        std::optional<bool> Selected;
        bool Enabled  = true;
        bool ReadOnly = false;

        /// @brief Reads the current range value, for values that change without a rebuild, such as a bound
        /// signal. Untracked.
        std::function<float()> ReadRange;
        std::function<void()> OnInvoke;
        std::function<void()> OnToggle;
        std::function<void(float)> OnSetRange;
        std::function<void(const std::string&)> OnSetValue;
        /// @brief Called with true to expand and false to collapse.
        std::function<void(bool)> OnExpand;
        std::function<void()> OnSelect;

        /// @brief Whether everything except the callbacks is the same.
        [[nodiscard]] XENUI_API bool HasSameData(const SemanticsProperties& Other) const;
    };

    /// @brief Describes its child with properties given by a Semantics widget.
    class XENUI_API RenderSemantics final : public RenderBox {
    public:
        void SetProperties(SemanticsProperties Properties) {
            _Properties = std::move(Properties);
            _Hint.clear();
        }
        /// @brief Describes nothing itself, and gives Hint as the description of the first node inside it, or as
        /// its name when it has none.
        void SetHint(std::string Hint) { _Hint = std::move(Hint); }
        [[nodiscard]] const std::string& GetHint() const { return _Hint; }
        [[nodiscard]] bool DescribeSemantics(SemanticsProperties& Properties) const override;

    private:
        SemanticsProperties _Properties;
        std::string _Hint;
    };

    /// @brief One node of a SemanticsTree, as of the last rebuild.
    struct SemanticsNode {
        uint64_t Id     = 0;
        uint64_t Parent = 0;
        std::vector<uint64_t> Children;
        SemanticsProperties Properties;
        /// @brief Window-space bounds in device-independent pixels.
        Rect Bounds;
        /// @brief The part of Bounds not clipped away by scroll views and clips.
        Rect VisibleBounds;
        /// @brief Whether the node can take keyboard focus.
        bool Focusable = false;

        [[nodiscard]] bool IsOffscreen() const { return VisibleBounds.IsEmpty(); }
    };

    /// @brief What changed in a SemanticsTree rebuild, for raising accessibility events.
    struct SemanticsUpdate {
        enum class Change : uint8_t { Name, Value, Range, Toggle, Expanded, Selected, Enabled };

        struct PropertyChange {
            uint64_t Id = 0;
            Change What = Change::Name;
        };

        bool FocusChanged = false;
        std::vector<PropertyChange> Properties;
        /// @brief Nodes whose children were added, removed or reordered.
        std::vector<uint64_t> StructureChanged;
    };

    /// @brief The accessibility tree of one widget tree: the render boxes that describe themselves, nested as
    /// they are in the render tree. Rebuilt from the render tree on demand; node ids stay the same for as long as
    /// their render box lives. Actions run against the live render box, so they always see current callbacks.
    class XENUI_API SemanticsTree {
    public:
        static constexpr uint64_t kRootId = 0;

        SemanticsTree();
        ~SemanticsTree();

        SemanticsTree(const SemanticsTree&)            = delete;
        SemanticsTree& operator=(const SemanticsTree&) = delete;

        /// @brief Rebuilds from Root, the render view of a widget host, and reports what changed.
        SemanticsUpdate Rebuild(RenderBox& Root, const FocusManager* Focus);

        /// @brief The node with Id, or nullptr. The root (kRootId) always exists and stands for the whole window.
        [[nodiscard]] const SemanticsNode* Find(uint64_t Id) const;
        [[nodiscard]] const SemanticsNode& GetRoot() const { return *Find(kRootId); }

        /// @brief The node holding keyboard focus, or kRootId when none does.
        [[nodiscard]] uint64_t GetFocused() const { return _Focused; }

        /// @brief The deepest visible node at Position (window space), or kRootId.
        [[nodiscard]] uint64_t HitTest(const Vec2& Position) const;

        /// @brief Finds the first node, in tree order, with the given label or identifier. For tests.
        [[nodiscard]] const SemanticsNode* FindByLabel(std::string_view Label) const;

        [[nodiscard]] size_t GetNodeCount() const { return _Nodes.size(); }

        bool Invoke(uint64_t Id) const;
        bool Toggle(uint64_t Id) const;
        bool SetRangeValue(uint64_t Id, float Value) const;
        bool SetValue(uint64_t Id, const std::string& Value) const;
        bool SetExpanded(uint64_t Id, bool Expanded) const;
        bool Select(uint64_t Id) const;
        bool Focus(uint64_t Id) const;

    private:
        struct Source {
            RenderBox* Box = nullptr;
            std::weak_ptr<void> Lifetime;
            RenderFocus* FocusBox = nullptr;
            std::weak_ptr<void> FocusLifetime;
        };

        struct BuildState;

        struct Inherited {
            RenderFocus* Focus      = nullptr;
            const std::string* Hint = nullptr;
            Rect Clip;
        };

        void Visit(RenderObject& Object, BuildState& State, uint64_t Parent, Inherited Context);
        [[nodiscard]] std::optional<SemanticsProperties> Describe(uint64_t Id) const;

        std::unordered_map<uint64_t, SemanticsNode> _Nodes;
        std::unordered_map<uint64_t, Source> _Sources;
        uint64_t _Focused = kRootId;
    };
}  // namespace XenUI
