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
#include "XenUI/Tree/Element.hpp"

#include <memory>

namespace XenUI {
    template<StatefulWidget TWidget>
    class StatefulElement;

    class XENUI_API StateBase {
    public:
        StateBase();
        virtual ~StateBase();

        StateBase(const StateBase&)            = delete;
        StateBase& operator=(const StateBase&) = delete;

        /// @brief Rebuilds the component on the next build pass. Rarely needed: reading a Signal in Build
        /// subscribes automatically.
        void MarkNeedsBuild();

        [[nodiscard]] Element& GetElement() const { return *_Element; }

    protected:
        /// @brief Called once, after the element is mounted and before the first Build.
        virtual void Init() {}

        /// @brief Called once, when the element leaves the tree for good.
        virtual void Dispose() {}

    private:
        template<StatefulWidget>
        friend class StatefulElement;

        Element* _Element = nullptr;
    };

    /// @brief Persistent state of a stateful widget. Declare it as `struct MyWidget { ...; class State; };`
    /// where `class MyWidget::State : public ComponentState<MyWidget>`.
    template<class TWidget>
    class ComponentState : public StateBase {
    public:
        /// @brief The current description. Replaced, not mutated, when the parent rebuilds.
        [[nodiscard]] const TWidget& GetWidget() const { return GetElement().GetWidget().template As<TWidget>(); }

    protected:
        virtual Node Build(BuildContext& Context) = 0;

        /// @brief Called after the parent supplied a new description of the same type and key.
        virtual void WidgetUpdated(const TWidget& Previous) { (void)Previous; }

    private:
        template<StatefulWidget>
        friend class StatefulElement;
    };

    template<StatelessWidget TWidget>
    class StatelessElement final : public ComponentElement {
    protected:
        Node Build(BuildContext& Context) override { return GetWidget().template As<TWidget>().Build(Context); }
    };

    template<StatefulWidget TWidget>
    class StatefulElement final : public ComponentElement {
    public:
        using StateType = typename TWidget::State;

        [[nodiscard]] StateType& GetState() const { return *_State; }

    protected:
        void OnMounted() override {
            _State             = std::make_unique<StateType>();
            GetBase()._Element = this;
            GetBase().Init();
        }

        void OnWidgetUpdated(const Node& Previous) override {
            GetComponentState().WidgetUpdated(Previous.template As<TWidget>());
        }

        void OnUnmounting() override { GetBase().Dispose(); }

        Node Build(BuildContext& Context) override { return GetComponentState().Build(Context); }

    private:
        StateBase& GetBase() const { return *_State; }
        ComponentState<TWidget>& GetComponentState() const { return *_State; }

        std::unique_ptr<StateType> _State;
    };
}  // namespace XenUI
