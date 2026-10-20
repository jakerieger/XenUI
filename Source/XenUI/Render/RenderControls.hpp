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
#include "XenUI/Render/RenderInput.hpp"
#include "XenUI/Render/RenderLayout.hpp"

#include <functional>
#include <optional>
#include <string>

namespace XenUI {
    /// @brief A value read from a signal when one is bound, or from a plain value otherwise. Writes go to the
    /// signal first, then to the change callback.
    struct BoundValue {
        float Value         = 0.0f;
        Signal<float>* Bind = nullptr;
        std::function<void(float)> OnChanged;

        /// @brief Current value; tracked when reading a bound signal.
        [[nodiscard]] float Get() const { return Bind != nullptr ? Bind->Get() : Value; }
        [[nodiscard]] float Peek() const { return Bind != nullptr ? Bind->Peek() : Value; }

        void Commit(float NewValue) const {
            if (Bind != nullptr) { *Bind = NewValue; }
            if (OnChanged) { OnChanged(NewValue); }
        }
    };

    /// @brief Track-and-thumb slider that maps pointer position directly to a value.
    class XENUI_API RenderSlider final : public RenderInteractive,
                                         public Observer {
    public:
        struct Configuration {
            BoundValue Source;
            float Min      = 0.0f;
            float Max      = 1.0f;
            Axis Direction = Axis::Horizontal;
            bool Enabled   = true;
            Color TrackColor;
            Color FillColor;
            Color ThumbColor;
            std::string Label;
        };

        RenderSlider();

        void SetConfiguration(Configuration NewConfiguration);

        bool HandlePointer(const PointerEvent& Event) override;
        void Paint(PaintContext& Context) override;
        void OnDependencyChanged() override { MarkNeedsPaint(); }
        [[nodiscard]] bool TracksHover() const override { return true; }
        [[nodiscard]] std::optional<CursorShape> GetCursor() const override;
        [[nodiscard]] bool DescribeSemantics(SemanticsProperties& Properties) const override;

    protected:
        void PerformLayout() override;
        [[nodiscard]] bool HitTestSelf(const Vec2& Position) const override;

    private:
        [[nodiscard]] float GetFraction(float Value) const;
        void CommitFromPosition(const Vec2& Local);

        Configuration _Configuration;
        bool _Dragging = false;
    };

    /// @brief Two panes separated by a draggable divider. The split ratio is read from a signal, so dragging
    /// relays out only this widget.
    class XENUI_API RenderSplit final : public RenderInteractive,
                                        public Observer {
    public:
        static constexpr float kDividerThickness = 6.0f;

        struct Configuration {
            Signal<float>* Ratio = nullptr;
            Axis Direction       = Axis::Horizontal;
            float MinFirst       = 80.0f;
            float MinSecond      = 80.0f;
            Color DividerColor;
            Color ActiveColor;
        };

        void SetConfiguration(const Configuration& NewConfiguration);

        bool HitTest(HitTestResult& Result, const Vec2& Position) override;
        bool HandlePointer(const PointerEvent& Event) override;
        void HandleHoverEnter(const PointerEvent& Event) override;
        void HandleHoverExit(const PointerEvent& Event) override;
        [[nodiscard]] bool TracksHover() const override { return true; }
        [[nodiscard]] std::optional<CursorShape> GetCursor() const override;
        void Paint(PaintContext& Context) override;
        void OnDependencyChanged() override { MarkNeedsLayout(); }

    protected:
        void PerformLayout() override;

    private:
        [[nodiscard]] bool IsHorizontal() const { return _Configuration.Direction == Axis::Horizontal; }
        [[nodiscard]] Rect GetDividerRect() const;

        Configuration _Configuration;
        float _FirstExtent = 0.0f;
        float _Available   = 0.0f;
        bool _Hovered      = false;
        std::optional<float> _Grab;
    };
}  // namespace XenUI
