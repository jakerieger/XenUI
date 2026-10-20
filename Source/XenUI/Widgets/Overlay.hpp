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
#include "XenUI/Reactive/Signal.hpp"
#include "XenUI/Render/RenderOverlay.hpp"
#include "XenUI/Tree/Widget.hpp"
#include "XenUI/Widgets/Basic.hpp"
#include "XenUI/Widgets/Input.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace XenUI {
    class OverlayEntry;

    enum class PopupKind : uint8_t {
        /// @brief Takes keyboard input and closes when the user clicks elsewhere, scrolls, or switches away.
        Menu,
        /// @brief Passive: ignores the pointer and never takes keyboard input.
        Tooltip,
    };

    struct PopupRequest {
        /// @brief Rectangle the popup attaches to, in the requesting window's coordinates.
        Rect Anchor;
        PopupPlacement Placement = PopupPlacement::Below;
        PopupKind Kind           = PopupKind::Menu;
        /// @brief Builds the popup's content. Signals it reads rebuild the popup.
        std::function<Node()> Builder;
        /// @brief Called when the popup is dismissed from outside its content. Should reset the entry.
        std::function<void()> OnDismissed;
        std::optional<Color> Background;
    };

    /// @brief Shows popups as separate native windows, so they can extend past the edges of their owner.
    /// Implemented by windows; hosts without one fall back to drawing popups inside the overlay.
    class XENUI_API PopupPresenter {
    public:
        virtual ~PopupPresenter();

        virtual uint64_t OpenPopup(PopupRequest Request) = 0;
        virtual void ClosePopup(uint64_t Id)             = 0;
    };

    /// @brief Floating content above a window's widgets. Every WidgetHost provides one; reach it from a Build
    /// method with OverlayController::Of.
    class XENUI_API OverlayController {
    public:
        struct Entry {
            uint64_t Id = 0;
            std::function<Node()> Builder;
        };

        /// @brief The overlay of the window containing Context, or nullptr outside a WidgetHost.
        [[nodiscard]] static OverlayController* Of(BuildContext& Context);

        /// @brief Adds a layer covering the window, built by Builder. Removed when the handle is reset.
        [[nodiscard]] OverlayEntry Show(std::function<Node()> Builder);

        /// @brief Opens a popup next to an anchor: a native window when the host supports it, otherwise a layer
        /// in this overlay with the same behavior.
        [[nodiscard]] OverlayEntry ShowPopup(PopupRequest Request);

        /// @brief Entries in stacking order. Tracked.
        [[nodiscard]] const std::vector<Entry>& GetEntries() const { return _Entries.Get(); }

        /// @internal
        void SetPresenter(PopupPresenter* Presenter) { _Presenter = Presenter; }

    private:
        friend class OverlayEntry;

        void Remove(uint64_t Id);

        Signal<std::vector<Entry>> _Entries;
        std::unordered_map<uint64_t, uint64_t> _NativePopups;
        PopupPresenter* _Presenter = nullptr;
        uint64_t _NextId           = 1;
    };

    class XENUI_API OverlayEntry {
    public:
        OverlayEntry() = default;
        ~OverlayEntry();

        OverlayEntry(const OverlayEntry&)            = delete;
        OverlayEntry& operator=(const OverlayEntry&) = delete;
        OverlayEntry(OverlayEntry&& Other) noexcept;
        OverlayEntry& operator=(OverlayEntry&& Other) noexcept;

        void Reset();
        [[nodiscard]] bool IsShown() const { return _Owner != nullptr; }

    private:
        friend class OverlayController;

        OverlayEntry(OverlayController& Owner, uint64_t Id) : _Owner(&Owner), _Id(Id) {}

        OverlayController* _Owner = nullptr;
        uint64_t _Id              = 0;
    };

    /// @brief Root wrapper installed by WidgetHost: stacks overlay entries above the content.
    struct OverlayHost {
        Node Child;
        /// @brief Fills the viewport. Off for hosts sized to their content.
        bool Expand               = true;
        PopupPresenter* Presenter = nullptr;

        class State : public ComponentState<OverlayHost> {
        protected:
            Node Build(BuildContext& Context) override;

        private:
            OverlayController _Controller;
        };
    };

    struct AnchoredPopup {
        Rect Anchor;
        PopupPlacement Placement = PopupPlacement::Below;
        Node Child;

        [[nodiscard]] std::unique_ptr<RenderAnchoredPopup> CreateRenderObject() const {
            auto Result = std::make_unique<RenderAnchoredPopup>();
            UpdateRenderObject(*Result);
            return Result;
        }

        void UpdateRenderObject(RenderAnchoredPopup& Target) const { Target.SetAnchor(Anchor, Placement); }
    };

    struct IgnorePointer {
        bool Ignoring = true;
        Node Child;

        [[nodiscard]] std::unique_ptr<RenderIgnorePointer> CreateRenderObject() const {
            auto Result = std::make_unique<RenderIgnorePointer>();
            UpdateRenderObject(*Result);
            return Result;
        }

        void UpdateRenderObject(RenderIgnorePointer& Target) const { Target.SetIgnoring(Ignoring); }
    };

    /// @brief Full-size layer that blocks the content beneath it and reports presses on it.
    struct XENUI_API ModalBarrier {
        std::optional<Color> Tint;
        std::function<void()> OnDismiss;

        [[nodiscard]] Node Build(BuildContext& Context) const;
    };

    /// @brief Window-space bounds of the render box an element produced, as of the last layout.
    [[nodiscard]] XENUI_API Rect GetGlobalBounds(const Element& Source);
}  // namespace XenUI
