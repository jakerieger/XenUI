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
#include "XenUI/Core/FrameStats.hpp"
#include "XenUI/Core/Math.hpp"
#include "XenUI/Core/Timer.hpp"
#include "XenUI/Layout/BoxConstraints.hpp"
#include "XenUI/Reactive/Reactive.hpp"
#include "XenUI/Reactive/Signal.hpp"
#include "XenUI/Tree/Widget.hpp"
#include "XenUI/Widgets/Controls.hpp"
#include "XenUI/Widgets/Overlay.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace XenUI {
    class RenderBox;
    class WidgetHost;
    class Window;

    /// @brief One element of an inspected widget tree, as of the last refresh.
    struct InspectorNode {
        /// @brief The path of child indices from the root, such as "0/2/1". Stays the same across refreshes while
        /// the tree keeps its shape.
        std::string Id;
        /// @brief The widget type, such as "Button" or "Provide<Theme>".
        std::string Name;
        /// @brief "Component" for stateless and stateful widgets and Provide, "Render" for render widgets.
        std::string Kind;
        /// @brief The widget's key, or empty without one. Keys are hashed, so this is the hash.
        std::string Key;
        /// @brief The render object this element owns, for render widgets, such as "RenderFlex".
        std::string RenderName;
        int Parent = -1;
        std::vector<int> Children;
        uint32_t Depth = 0;
        /// @brief Window-space bounds of the element's render object, when it has one.
        std::optional<Rect> Bounds;
        std::optional<BoxConstraints> Constraints;
        bool RepaintBoundary = false;
        bool NeedsLayout     = false;
        bool NeedsPaint      = false;
        /// @brief How the render object describes itself to screen readers, if it does.
        std::string Semantics;
    };

    /// @brief A snapshot of a widget host's element tree, for the inspector and for tests. Refresh walks the tree
    /// again; the snapshot copies everything it reports, so it stays valid while the tree changes.
    class XENUI_API WidgetInspector {
    public:
        explicit WidgetInspector(WidgetHost& Target) : _Target(&Target) {}

        void Refresh();

        [[nodiscard]] const std::vector<InspectorNode>& GetNodes() const { return _Nodes; }
        [[nodiscard]] const InspectorNode* Find(std::string_view Id) const;
        [[nodiscard]] int IndexOf(std::string_view Id) const;
        /// @brief The deepest render widget under Position (window coordinates), as of the last refresh.
        [[nodiscard]] std::string HitTest(const Vec2& Position) const;
        /// @brief Increases on every Refresh. Tracked, so views of the snapshot rebuild after a refresh.
        [[nodiscard]] uint64_t GetRevision() const { return _Revision.Get(); }

        /// @brief Cleans a C++ type name for display: drops "struct ", "class " and the XenUI namespace.
        [[nodiscard]] static std::string CleanTypeName(std::string_view Raw);

    private:
        WidgetHost* _Target = nullptr;
        std::vector<InspectorNode> _Nodes;
        std::unordered_map<const RenderBox*, int> _ByBox;
        Signal<uint64_t> _Revision = 0;
    };

    /// @internal What DeveloperTools and its inspector window share.
    struct InspectorModel {
        explicit InspectorModel(WidgetHost& Target) : Inspector(Target) {}

        /// @brief Expands every ancestor of Id so it is visible in the tree.
        void Reveal(const std::string& Id);

        WidgetInspector Inspector;
        Signal<std::string> Selected;
        Signal<std::set<std::string>> Expanded;
        Signal<bool> Picking = false;
        Signal<std::optional<Rect>> Highlight;
        Signal<std::string> HighlightLabel;
    };

    /// @brief A compact panel showing frame rate, frame times by stage, work counts and damage for a window, with a
    /// graph of recent frames. Refreshes four times a second.
    struct FrameStatsPanel {
        FrameStatistics* Source = nullptr;

        class XENUI_API State : public ComponentState<FrameStatsPanel> {
        protected:
            void Init() override;
            Node Build(BuildContext& Context) override;

        private:
            Signal<uint64_t> _Tick = 0;
            Timer _Refresh;
        };
    };

    /// @brief Developer tools for the window it is placed in. Wrap the root widget in it:
    ///
    /// - F12 opens the widget inspector in a separate window: the element tree, the selected element's widget,
    ///   render object, bounds, constraints and accessibility description, with the selection outlined in this
    ///   window and a pick mode to select an element by clicking it.
    /// - Shift+F12 shows the frame stats overlay in the top-right corner.
    ///
    /// The shortcuts work wherever focus is. Leave DeveloperTools out of release builds, or set Shortcuts to false
    /// and drive it with ShowInspector and ShowFrameStats.
    struct DeveloperTools {
        Node Child;
        bool Shortcuts = true;
        /// @brief Optional external switch for the inspector window.
        Signal<bool>* ShowInspector = nullptr;
        /// @brief Optional external switch for the frame stats overlay.
        Signal<bool>* ShowFrameStats = nullptr;

        class XENUI_API State : public ComponentState<DeveloperTools> {
        public:
            State();
            ~State() override;

        protected:
            void Init() override;
            void Dispose() override;
            Node Build(BuildContext& Context) override;

        private:
            [[nodiscard]] Signal<bool>& GetInspectorSwitch();
            [[nodiscard]] Signal<bool>& GetStatsSwitch();
            void Sync();
            void OpenInspector();
            void CloseInspector();
            void UpdateHighlight();

            WidgetHost* _Host           = nullptr;
            OverlayController* _Overlay = nullptr;
            PopupStyle _Style;
            uint64_t _Interceptor     = 0;
            Signal<bool> _InspectorOn = false;
            Signal<bool> _StatsOn     = false;
            std::unique_ptr<InspectorModel> _Model;
            std::unique_ptr<Window> _Window;
            OverlayEntry _StatsEntry;
            OverlayEntry _HighlightEntry;
            OverlayEntry _PickEntry;
            std::optional<Effect> _Watch;
            std::optional<Effect> _Follow;
            Timer _SyncLater;
            Timer _Refresh;
        };
    };
}  // namespace XenUI
