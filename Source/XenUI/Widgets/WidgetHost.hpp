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
#include "XenUI/Graphics/Layer.hpp"
#include "XenUI/Input/InputDispatcher.hpp"
#include "XenUI/Render/RenderBox.hpp"
#include "XenUI/Tree/ElementTree.hpp"

#include <functional>
#include <memory>

namespace XenUI {
    class WindowChrome;
    class PopupPresenter;

    /// @brief Runs the build → layout → paint pipeline for a widget tree, producing a layer tree. Windows host one
    /// per content root; it can also be driven headlessly, for example in tests.
    class XENUI_API WidgetHost {
    public:
        WidgetHost();
        ~WidgetHost();

        WidgetHost(const WidgetHost&)            = delete;
        WidgetHost& operator=(const WidgetHost&) = delete;

        /// @brief Shows Root beneath the overlay layer that popups, menus and tooltips use.
        void SetRoot(const Node& Root);

        /// @brief Viewport size in device-independent pixels. The root widget is laid out to fill it exactly.
        void SetViewportSize(const Size& Viewport);

        /// @brief Lays the root widget out at its natural size, up to Maximum. Call before SetRoot.
        void SetSizeToContent(const Size& Maximum);

        /// @brief Shows popups through Presenter instead of inside the overlay. Call before SetRoot.
        void SetPopupPresenter(PopupPresenter* Presenter) { _Presenter = Presenter; }
        /// @brief The platform window (an HWND) that native child windows of this tree are created in.
        void SetNativeParent(void* Parent) { _Pipeline.SetNativeParent(Parent); }
        /// @brief The title bar state that WindowControls and WindowChrome::Of find, for custom title bars.
        void SetWindowChrome(WindowChrome* Chrome) { _Chrome = Chrome; }

        /// @brief Size of the laid-out root, which is the content size when sized to content.
        [[nodiscard]] Size GetLaidOutSize() const { return _View->GetSize(); }

        /// @brief Rebuilds dirty elements, then flushes layout and painting.
        void Update();

        [[nodiscard]] bool NeedsUpdate() const;

        /// @brief Invoked when an update becomes necessary, so the host can schedule a frame.
        void SetOnUpdateRequested(std::function<void()> Callback);

        [[nodiscard]] const std::shared_ptr<Layer>& GetRootLayer() const;

        /// @brief The root of the render tree, above the overlay and the root widget.
        [[nodiscard]] RenderView& GetView() const { return *_View; }

        /// @brief The render box of the root widget, or nullptr.
        [[nodiscard]] RenderBox* GetRootBox() const;

        [[nodiscard]] ElementTree& GetTree() const { return *_Tree; }

        /// @brief Feeds window input into the tree. Positions are in the same space as the viewport.
        [[nodiscard]] InputDispatcher& GetInput() const { return *_Input; }

    private:
        void RequestUpdate();

        PipelineOwner _Pipeline;
        std::unique_ptr<RenderView> _View;
        std::unique_ptr<InputDispatcher> _Input;
        std::unique_ptr<ElementTree> _Tree;
        std::function<void()> _OnUpdateRequested;
        PopupPresenter* _Presenter = nullptr;
        WindowChrome* _Chrome      = nullptr;
    };
}  // namespace XenUI
