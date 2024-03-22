// Copyright (C) 2024 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

// Author: Jake Rieger
// Created: 3/21/2024.
//

#pragma once

#include "Rect.h"
#include "XenAPI.h"

#include <algorithm>
#include <vector>

namespace Xen {
    class XEN_API Element {
    public:
        explicit Element(const i64 zIndex, const Rect& size) : Size(size), ZIndex(zIndex) {}
        virtual ~Element()  = default;
        virtual void Draw() = 0;

        void UpdateSize(const Rect& size) { Size = size; }
        Rect& GetSize() { return Size; }

        [[nodiscard]] i64 GetZIndex() const { return ZIndex; }
        [[nodiscard]] Element* GetChild() const { return Children.at(0); }
        [[nodiscard]] u64 GetNumChildren() const { return Children.size(); }
        std::vector<Element*>& GetChildren() { return Children; }

        template<typename T>
        T* As() {
            return dynamic_cast<T*>(this);
        }

    protected:
        std::vector<Element*> Children;
        Rect Size;

    private:
        i64 ZIndex;
    };
}  // namespace Xen