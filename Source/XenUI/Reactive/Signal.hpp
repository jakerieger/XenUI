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

#include "XenUI/Reactive/Reactive.hpp"

#include <concepts>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace XenUI {
    /// @brief Observable value. Reading it inside a tracked scope (a component's Build, a Computed, an Effect)
    /// subscribes that scope; writing a different value notifies every subscriber. Movable, not copyable.
    template<class T>
    class Signal {
    public:
        Signal()
            requires std::default_initializable<T>
            : Signal(T {}) {}

        Signal(T Initial) : _State(std::make_unique<State>(std::move(Initial))) {}

        Signal(const Signal&)                = delete;
        Signal& operator=(const Signal&)     = delete;
        Signal(Signal&&) noexcept            = default;
        Signal& operator=(Signal&&) noexcept = default;

        Signal& operator=(T NewValue) {
            Set(std::move(NewValue));
            return *this;
        }

        [[nodiscard]] const T& Get() const {
            _State->Source.Track();
            return _State->Value;
        }

        [[nodiscard]] const T& operator()() const { return Get(); }

        /// @brief Reads without subscribing.
        [[nodiscard]] const T& Peek() const { return _State->Value; }

        /// @brief Stores NewValue and notifies subscribers. Equal values are ignored when T supports ==.
        void Set(T NewValue) {
            if constexpr (std::equality_comparable<T>) {
                if (_State->Value == NewValue) { return; }
            }
            _State->Value = std::move(NewValue);
            _State->Source.Notify();
        }

        /// @brief Mutates the value in place, then notifies subscribers unconditionally.
        template<std::invocable<T&> TMutator>
        void Modify(TMutator&& Mutator) {
            std::invoke(std::forward<TMutator>(Mutator), _State->Value);
            _State->Source.Notify();
        }

    private:
        struct State {
            explicit State(T Initial) : Value(std::move(Initial)) {}

            T Value;
            SignalSource Source;
        };

        std::unique_ptr<State> _State;
    };

    /// @brief Lazily evaluated value derived from other signals. Recomputed on the first read after a dependency
    /// changes, and itself observable like a Signal.
    template<class T>
    class Computed {
    public:
        explicit Computed(std::function<T()> Compute) : _State(std::make_unique<State>(std::move(Compute))) {}

        Computed(const Computed&)                = delete;
        Computed& operator=(const Computed&)     = delete;
        Computed(Computed&&) noexcept            = default;
        Computed& operator=(Computed&&) noexcept = default;

        [[nodiscard]] const T& Get() const {
            _State->Source.Track();
            return _State->Refresh();
        }

        [[nodiscard]] const T& operator()() const { return Get(); }

        [[nodiscard]] const T& Peek() const { return _State->Refresh(); }

    private:
        class State final : public Observer {
        public:
            explicit State(std::function<T()> Compute) : _Compute(std::move(Compute)) {}

            void OnDependencyChanged() override {
                if (_Dirty) { return; }
                _Dirty = true;
                Source.Notify();
            }

            const T& Refresh() {
                if (_Dirty) {
                    TrackingScope Scope(*this);
                    _Value = _Compute();
                    _Dirty = false;
                }
                return *_Value;
            }

            SignalSource Source;

        private:
            std::function<T()> _Compute;
            std::optional<T> _Value;
            bool _Dirty = true;
        };

        std::unique_ptr<State> _State;
    };
}  // namespace XenUI
