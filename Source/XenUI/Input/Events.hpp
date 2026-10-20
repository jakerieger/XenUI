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

#include "XenUI/Core/Math.hpp"

#include <cstdint>
#include <string>
#include <type_traits>

namespace XenUI {
    enum class KeyModifiers : uint8_t {
        None    = 0,
        Shift   = 1 << 0,
        Control = 1 << 1,
        Alt     = 1 << 2,
        Super   = 1 << 3,
    };

    enum class MouseButton : uint8_t {
        None      = 0,
        Primary   = 1 << 0,
        Secondary = 1 << 1,
        Middle    = 1 << 2,
        Back      = 1 << 3,
        Forward   = 1 << 4,
    };

    template<class T>
        requires std::is_same_v<T, KeyModifiers> || std::is_same_v<T, MouseButton>
    constexpr T operator|(T Left, T Right) {
        return static_cast<T>(static_cast<uint8_t>(Left) | static_cast<uint8_t>(Right));
    }

    template<class T>
        requires std::is_same_v<T, KeyModifiers> || std::is_same_v<T, MouseButton>
    constexpr T operator&(T Left, T Right) {
        return static_cast<T>(static_cast<uint8_t>(Left) & static_cast<uint8_t>(Right));
    }

    /// @brief True when every flag in Required is present in Set.
    template<class T>
        requires std::is_same_v<T, KeyModifiers> || std::is_same_v<T, MouseButton>
    constexpr bool HasFlags(T Set, T Required) {
        return Required != T::None && (Set & Required) == Required;
    }

    enum class PointerPhase : uint8_t { Down, Move, Up, Cancel, Wheel, Leave };
    enum class PointerKind : uint8_t { Mouse, Pen, Touch };

    struct PointerEvent {
        PointerPhase Phase = PointerPhase::Move;
        PointerKind Kind   = PointerKind::Mouse;
        uint32_t PointerId = 0;
        /// @brief Window-space position in device-independent pixels.
        Vec2 Position;
        /// @brief Position in the receiving box's coordinate space. Filled in during dispatch.
        Vec2 LocalPosition;
        /// @brief Wheel movement in notches. Positive Y scrolls up (away from the user), positive X right.
        Vec2 WheelDelta;
        /// @brief The button that changed, for Down and Up.
        MouseButton Button     = MouseButton::None;
        MouseButton Buttons    = MouseButton::None;
        KeyModifiers Modifiers = KeyModifiers::None;
        double Time            = 0.0;
    };

    struct TapEvent {
        Vec2 LocalPosition;
        MouseButton Button     = MouseButton::Primary;
        KeyModifiers Modifiers = KeyModifiers::None;
        /// @brief 1 for a single tap, 2 for the second tap of a double tap, and so on.
        uint32_t Count = 1;
    };

    struct DragEvent {
        /// @brief Pointer position relative to the dragged box. Fixed at the start point during relative drags.
        Vec2 LocalPosition;
        /// @brief Movement since the previous update, in device-independent pixels.
        Vec2 Delta;
        /// @brief Movement since the drag started.
        Vec2 TotalDelta;
        MouseButton Button     = MouseButton::Primary;
        KeyModifiers Modifiers = KeyModifiers::None;
        bool Relative          = false;
        bool Cancelled         = false;
    };

    /// @brief Virtual key codes, matching Windows VK_ values. Punctuation keys are named for their US layout
    /// positions.
    enum class KeyCode : uint16_t {
        None         = 0x00,
        Backspace    = 0x08,
        Tab          = 0x09,
        Enter        = 0x0D,
        Shift        = 0x10,
        Control      = 0x11,
        Alt          = 0x12,
        Pause        = 0x13,
        CapsLock     = 0x14,
        Escape       = 0x1B,
        Space        = 0x20,
        PageUp       = 0x21,
        PageDown     = 0x22,
        End          = 0x23,
        Home         = 0x24,
        Left         = 0x25,
        Up           = 0x26,
        Right        = 0x27,
        Down         = 0x28,
        PrintScreen  = 0x2C,
        Insert       = 0x2D,
        Delete       = 0x2E,
        D0           = 0x30,
        D1           = 0x31,
        D2           = 0x32,
        D3           = 0x33,
        D4           = 0x34,
        D5           = 0x35,
        D6           = 0x36,
        D7           = 0x37,
        D8           = 0x38,
        D9           = 0x39,
        A            = 0x41,
        B            = 0x42,
        C            = 0x43,
        D            = 0x44,
        E            = 0x45,
        F            = 0x46,
        G            = 0x47,
        H            = 0x48,
        I            = 0x49,
        J            = 0x4A,
        K            = 0x4B,
        L            = 0x4C,
        M            = 0x4D,
        N            = 0x4E,
        O            = 0x4F,
        P            = 0x50,
        Q            = 0x51,
        R            = 0x52,
        S            = 0x53,
        T            = 0x54,
        U            = 0x55,
        V            = 0x56,
        W            = 0x57,
        X            = 0x58,
        Y            = 0x59,
        Z            = 0x5A,
        LeftSuper    = 0x5B,
        RightSuper   = 0x5C,
        Menu         = 0x5D,
        Numpad0      = 0x60,
        Numpad1      = 0x61,
        Numpad2      = 0x62,
        Numpad3      = 0x63,
        Numpad4      = 0x64,
        Numpad5      = 0x65,
        Numpad6      = 0x66,
        Numpad7      = 0x67,
        Numpad8      = 0x68,
        Numpad9      = 0x69,
        Multiply     = 0x6A,
        Add          = 0x6B,
        Separator    = 0x6C,
        Subtract     = 0x6D,
        Decimal      = 0x6E,
        Divide       = 0x6F,
        F1           = 0x70,
        F2           = 0x71,
        F3           = 0x72,
        F4           = 0x73,
        F5           = 0x74,
        F6           = 0x75,
        F7           = 0x76,
        F8           = 0x77,
        F9           = 0x78,
        F10          = 0x79,
        F11          = 0x7A,
        F12          = 0x7B,
        F13          = 0x7C,
        F14          = 0x7D,
        F15          = 0x7E,
        F16          = 0x7F,
        F17          = 0x80,
        F18          = 0x81,
        F19          = 0x82,
        F20          = 0x83,
        F21          = 0x84,
        F22          = 0x85,
        F23          = 0x86,
        F24          = 0x87,
        NumLock      = 0x90,
        ScrollLock   = 0x91,
        Semicolon    = 0xBA,
        Equals       = 0xBB,
        Comma        = 0xBC,
        Minus        = 0xBD,
        Period       = 0xBE,
        Slash        = 0xBF,
        Backquote    = 0xC0,
        LeftBracket  = 0xDB,
        Backslash    = 0xDC,
        RightBracket = 0xDD,
        Quote        = 0xDE,
    };

    [[nodiscard]] constexpr bool IsModifierKey(KeyCode Code) {
        return Code == KeyCode::Shift || Code == KeyCode::Control || Code == KeyCode::Alt ||
               Code == KeyCode::LeftSuper || Code == KeyCode::RightSuper;
    }

    struct KeyEvent {
        KeyCode Code           = KeyCode::None;
        KeyModifiers Modifiers = KeyModifiers::None;
        bool Pressed           = true;
        bool Repeat            = false;
        uint32_t ScanCode      = 0;
        double Time            = 0.0;
    };

    struct TextEvent {
        /// @brief UTF-8 encoded characters typed, excluding control characters.
        std::string Text;
    };

    /// @brief A key pressed with an exact set of modifiers. Write as `KeyModifiers::Control + KeyCode::S`.
    struct KeyCombination {
        KeyCode Code           = KeyCode::None;
        KeyModifiers Modifiers = KeyModifiers::None;

        [[nodiscard]] constexpr bool Matches(const KeyEvent& Event) const {
            return Event.Pressed && Event.Code == Code && Event.Modifiers == Modifiers;
        }

        constexpr bool operator==(const KeyCombination&) const = default;
    };

    constexpr KeyCombination operator+(KeyModifiers Modifiers, KeyCode Code) {
        return {Code, Modifiers};
    }

    constexpr KeyCombination operator|(KeyModifiers Modifiers, KeyCombination Combination) {
        return {Combination.Code, Combination.Modifiers | Modifiers};
    }

    enum class CursorShape : uint8_t {
        Arrow,
        Hand,
        IBeam,
        Crosshair,
        ResizeHorizontal,
        ResizeVertical,
        ResizeAll,
        NotAllowed,
        Wait,
        Hidden,
    };

    /// @brief How an interactive box participates in hit testing.
    enum class HitTestBehavior : uint8_t {
        /// @brief Hit only where a child is hit.
        DeferToChild,
        /// @brief Hit anywhere inside its bounds, and hide what is behind it.
        Opaque,
        /// @brief Hit anywhere inside its bounds, and let what is behind it be hit too.
        Translucent,
    };
}  // namespace XenUI
