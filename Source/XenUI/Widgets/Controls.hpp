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
#include "XenUI/Core/Timer.hpp"
#include "XenUI/Input/Focus.hpp"
#include "XenUI/Reactive/Signal.hpp"
#include "XenUI/Render/RenderControls.hpp"
#include "XenUI/Tree/Widget.hpp"
#include "XenUI/Widgets/Basic.hpp"
#include "XenUI/Widgets/Input.hpp"
#include "XenUI/Widgets/Overlay.hpp"
#include "XenUI/Widgets/Semantics.hpp"
#include "XenUI/Widgets/Theme.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace XenUI {
    /// @brief Inherited styling captured where a popup is requested. Popups are built in their own tree (often in
    /// their own window), so they re-provide the requester's theme and text style.
    struct XENUI_API PopupStyle {
        Theme Palette;
        TextStyle Typography;

        [[nodiscard]] static PopupStyle Capture(BuildContext& Context);
        [[nodiscard]] Node Wrap(const Node& Content) const;
    };

    enum class ButtonVariant : uint8_t { Primary, Secondary, Ghost };

    /// @brief Push button. Activates on click, or on Enter or Space while focused.
    struct Button {
        std::string Label;
        std::function<void()> OnPressed;
        ButtonVariant Variant = ButtonVariant::Secondary;
        bool Enabled          = true;
        std::optional<float> Width;
        /// @brief The name screen readers announce, when Label is a symbol; Label when empty.
        std::string AccessibleLabel;

        class XENUI_API State : public ComponentState<Button> {
        protected:
            Node Build(BuildContext& Context) override;

        private:
            void Activate() const;

            Signal<bool> _Hovered = false;
            Signal<bool> _Pressed = false;
            FocusNode _Focus;
            Animated<Color> _Fill;
            bool _Shown = false;
        };
    };

    struct Checkbox {
        bool Value = false;
        std::function<void(bool)> OnChanged;
        std::string Label;
        bool Enabled = true;

        class XENUI_API State : public ComponentState<Checkbox> {
        protected:
            Node Build(BuildContext& Context) override;

        private:
            Animated<float> _Checked;
            bool _Shown = false;
        };
    };

    /// @brief On/off toggle drawn as a sliding switch.
    struct Switch {
        bool Value = false;
        std::function<void(bool)> OnChanged;
        std::string Label;
        bool Enabled = true;

        class XENUI_API State : public ComponentState<Switch> {
        protected:
            Node Build(BuildContext& Context) override;

        private:
            Animated<float> _Position;
            bool _Shown = false;
        };
    };

    struct RawSlider {
        RenderSlider::Configuration Configuration;

        [[nodiscard]] std::unique_ptr<RenderSlider> CreateRenderObject() const {
            auto Result = std::make_unique<RenderSlider>();
            UpdateRenderObject(*Result);
            return Result;
        }

        void UpdateRenderObject(RenderSlider& Target) const { Target.SetConfiguration(Configuration); }
    };

    /// @brief Linear value control. Click or drag to set, wheel or arrow keys to nudge (hold Shift for fine steps).
    /// With Bind set, the slider reads and writes the signal directly and repaints without rebuilding.
    struct XENUI_API Slider {
        float Value         = 0.0f;
        float Min           = 0.0f;
        float Max           = 1.0f;
        Signal<float>* Bind = nullptr;
        std::function<void(float)> OnChanged;
        Axis Direction = Axis::Horizontal;
        bool Enabled   = true;
        /// @brief The name screen readers announce.
        std::string Label;

        [[nodiscard]] Node Build(BuildContext& Context) const;
    };

    /// @brief Rotary control for DAW-style parameters. Drag vertically with the cursor hidden and unbounded
    /// movement, hold Shift for fine control, double-click to reset to Default, or use the wheel.
    struct Knob {
        float Value         = 0.0f;
        float Min           = 0.0f;
        float Max           = 1.0f;
        float Default       = 0.0f;
        Signal<float>* Bind = nullptr;
        std::function<void(float)> OnChanged;
        float Diameter = 32.0f;
        /// @brief Draws the value arc from the middle of the range, for pan-like parameters.
        bool Bipolar = false;
        /// @brief The name screen readers announce.
        std::string Label;

        class XENUI_API State : public ComponentState<Knob> {
        protected:
            Node Build(BuildContext& Context) override;

        private:
            [[nodiscard]] BoundValue GetSource() const;

            float _DragValue = 0.0f;
        };
    };

    /// @brief Shows Message near the child after the pointer rests on it.
    struct Tooltip {
        std::string Message;
        Node Child;
        double Delay = 0.6;

        class XENUI_API State : public ComponentState<Tooltip> {
        protected:
            Node Build(BuildContext& Context) override;

        private:
            void Show();
            void Hide();

            OverlayController* _Overlay = nullptr;
            PopupStyle _Style;
            Timer _Timer;
            OverlayEntry _Entry;
        };
    };

    struct MenuItem {
        std::string Label;
        std::string ShortcutText;
        std::function<void()> OnSelect;
        bool Enabled   = true;
        bool Checked   = false;
        bool Separator = false;

        [[nodiscard]] static MenuItem Divider() { return {.Separator = true}; }
    };

    /// @brief The panel of a popup menu. Navigate with the mouse or Up, Down, Enter and Escape.
    struct MenuPanel {
        std::vector<MenuItem> Items;
        std::function<void()> OnDismiss;
        float Width = 200.0f;

        class XENUI_API State : public ComponentState<MenuPanel> {
        protected:
            Node Build(BuildContext& Context) override;

        private:
            void Move(int Direction);
            void Select(int Index) const;

            Signal<int> _Highlighted = -1;
            FocusNode _Focus;
        };
    };

    /// @brief Opens a menu popup next to Anchor (window coordinates). OnClosed runs when the user dismisses it or
    /// picks an item, and should reset the returned entry.
    [[nodiscard]] XENUI_API OverlayEntry ShowMenu(OverlayController& Overlay,
                                                  const Rect& Anchor,
                                                  PopupPlacement Placement,
                                                  std::vector<MenuItem> Items,
                                                  std::function<void()> OnClosed,
                                                  const PopupStyle& Style,
                                                  float Width = 200.0f);

    /// @brief Opens a menu at the pointer on right-click.
    struct ContextMenu {
        std::vector<MenuItem> Items;
        Node Child;

        class XENUI_API State : public ComponentState<ContextMenu> {
        protected:
            Node Build(BuildContext& Context) override;

        private:
            void Open(const Vec2& LocalPosition);

            OverlayController* _Overlay = nullptr;
            PopupStyle _Style;
            OverlayEntry _Menu;
        };
    };

    /// @brief Drop-down choice between a few options.
    struct Select {
        std::vector<std::string> Options;
        int SelectedIndex = 0;
        std::function<void(int)> OnChanged;
        float Width = 160.0f;
        /// @brief The name screen readers announce.
        std::string Label;

        class XENUI_API State : public ComponentState<Select> {
        protected:
            Node Build(BuildContext& Context) override;

        private:
            void Open();
            void Close();

            OverlayController* _Overlay = nullptr;
            PopupStyle _Style;
            OverlayEntry _Menu;
            Signal<bool> _Hovered = false;
            FocusNode _Focus;
        };
    };

    struct RawSplit {
        RenderSplit::Configuration Configuration;
        NodeList Children;

        [[nodiscard]] std::unique_ptr<RenderSplit> CreateRenderObject() const {
            auto Result = std::make_unique<RenderSplit>();
            UpdateRenderObject(*Result);
            return Result;
        }

        void UpdateRenderObject(RenderSplit& Target) const { Target.SetConfiguration(Configuration); }
    };

    /// @brief Two resizable panes. Pass Ratio to persist or control the split; otherwise it starts at
    /// InitialRatio and is kept internally.
    struct SplitView {
        Axis Direction       = Axis::Horizontal;
        Signal<float>* Ratio = nullptr;
        float InitialRatio   = 0.5f;
        float MinFirst       = 80.0f;
        float MinSecond      = 80.0f;
        Node First;
        Node Second;

        class XENUI_API State : public ComponentState<SplitView> {
        protected:
            void Init() override;
            Node Build(BuildContext& Context) override;

        private:
            Signal<float> _Ratio = 0.5f;
        };
    };
}  // namespace XenUI
