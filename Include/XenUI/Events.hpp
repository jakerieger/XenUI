// Author: Jake Rieger
// Created: 12/3/24.
//

#pragma once

#include "EventSystem.hpp"

namespace Xen {
    class PaintEvent final : public IEvent {
    public:
        PaintEvent() = default;
    };

    class CloseEvent final : public IEvent {
    public:
        CloseEvent() = default;
    };

    class ResizeEvent final : public IEvent {
    public:
        ResizeEvent(i32 w, i32 h) : Width(w), Height(h) {}
        const i32 Width, Height;
    };

    class KeyPressEvent final : public IEvent {
    public:
        explicit KeyPressEvent(const u64 keyCode) : KeyCode(keyCode) {}
        const u64 KeyCode;
    };

    class CursorMoveEvent final : public IEvent {
    public:
        explicit CursorMoveEvent(i32 x, i32 y) : X(x), Y(y) {}
        const i32 X, Y;
    };
}  // namespace Xen