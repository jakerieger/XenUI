//
// Created by conta on 3/17/2024.
//

#pragma once

#include "Rect.h"
#include "XenAPI.h"

#include <algorithm>
#include <vector>

namespace Xen {
    class XEN_API Element {
    public:
        explicit Element(const i64 zIndex) : ZIndex(zIndex) {}
        virtual ~Element()                        = default;
        virtual void Draw()                       = 0;
        virtual void UpdateSize(const Rect& size) = 0;

        [[nodiscard]] i64 GetZIndex() const { return ZIndex; }
        [[nodiscard]] Element* GetChild() const { return Children.at(0); }
        [[nodiscard]] u64 GetNumChildren() const { return Children.size(); }
        std::vector<Element*>& GetChildren() { return Children; }

        template<typename T>
        T* As() {
            return dynamic_cast<T*>(this);
        }

        static void SortByZIndex(std::vector<Element*>& elements) {
            std::ranges::sort(elements, [](const Element* a, const Element* b) {
                return a->GetZIndex() < b->GetZIndex();
            });
        }

    protected:
        std::vector<Element*> Children;

    private:
        i64 ZIndex;
    };
}  // namespace Xen