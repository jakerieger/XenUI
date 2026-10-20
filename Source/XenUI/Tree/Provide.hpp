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

#include "XenUI/Tree/Element.hpp"

#include <algorithm>
#include <concepts>
#include <typeinfo>
#include <vector>

namespace XenUI {
    /// @brief Makes Value available to every descendant through BuildContext::Find<T>(). Descendants that looked it
    /// up rebuild when a new Provide supplies a different value (compared with == when T supports it).
    template<class T>
    struct Provide {
        using ValueType = T;

        T Value;
        Node Child;
    };

    template<class T>
    class ProvideElement final : public ComponentElement {
    public:
        [[nodiscard]] const void* GetProvidedValue(const std::type_info& Type) const override {
            return Type == typeid(T) ? &GetDescription().Value : nullptr;
        }

        void AddDependent(Element& Dependent) override {
            if (std::find(_Dependents.begin(), _Dependents.end(), &Dependent) == _Dependents.end()) {
                _Dependents.push_back(&Dependent);
            }
        }

        void RemoveDependent(Element& Dependent) override { std::erase(_Dependents, &Dependent); }

    protected:
        Node Build(BuildContext& Context) override {
            (void)Context;
            return GetDescription().Child;
        }

        void OnWidgetUpdated(const Node& Previous) override {
            if constexpr (std::equality_comparable<T>) {
                if (Previous.template As<Provide<T>>().Value == GetDescription().Value) { return; }
            }
            for (Element* Dependent : std::vector<Element*>(_Dependents)) {
                Dependent->MarkNeedsBuild();
            }
        }

    private:
        [[nodiscard]] const Provide<T>& GetDescription() const { return GetWidget().template As<Provide<T>>(); }

        std::vector<Element*> _Dependents;
    };
}  // namespace XenUI
