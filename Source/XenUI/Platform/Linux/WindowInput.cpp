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

#include "XenUI/AppContext.hpp"
#include "XenUI/Input/TextInput.hpp"
#include "XenUI/Platform/Linux/PopupManager.hpp"
#include "XenUI/Platform/Linux/WindowImpl.hpp"
#include "XenUI/Widgets/WidgetHost.hpp"
#include "XenUI/Widgets/WindowChrome.hpp"

#include <array>
#include <cmath>
#include <string>
#include <utility>

namespace XenUI {
    namespace {
        /// @brief How far inside a borderless window's edge the pointer resizes it, in DIPs.
        constexpr float kResizeBorder = 5.0f;
        /// @brief Two caption presses this close in time and space make a double-click, as on Windows by default.
        constexpr double kDoubleClickSeconds = 0.5;
        constexpr float kDoubleClickDistance = 6.0f;
        /// @brief Focus loss and pointer leave arriving this close together come from the same caption press.
        constexpr double kSameGrabSeconds = 0.15;

        double CurrentTime() {
            const AppContext* Context = AppContext::TryGet();
            return Context != nullptr ? Context->GetTime() : 0.0;
        }

        KeyModifiers ToModifiers(SDL_Keymod Mod) {
            KeyModifiers Result = KeyModifiers::None;
            if ((Mod & SDL_KMOD_SHIFT) != 0) { Result = Result | KeyModifiers::Shift; }
            if ((Mod & SDL_KMOD_CTRL) != 0) { Result = Result | KeyModifiers::Control; }
            if ((Mod & SDL_KMOD_ALT) != 0) { Result = Result | KeyModifiers::Alt; }
            if ((Mod & SDL_KMOD_GUI) != 0) { Result = Result | KeyModifiers::Super; }
            return Result;
        }

        KeyModifiers QueryModifiers() {
            return ToModifiers(SDL_GetModState());
        }

        MouseButton ToButton(uint8_t Button) {
            switch (Button) {
                case SDL_BUTTON_LEFT:
                    return MouseButton::Primary;
                case SDL_BUTTON_RIGHT:
                    return MouseButton::Secondary;
                case SDL_BUTTON_MIDDLE:
                    return MouseButton::Middle;
                case SDL_BUTTON_X1:
                    return MouseButton::Back;
                case SDL_BUTTON_X2:
                    return MouseButton::Forward;
                default:
                    return MouseButton::None;
            }
        }

        MouseButton ToButtons(SDL_MouseButtonFlags Flags) {
            MouseButton Result = MouseButton::None;
            if ((Flags & SDL_BUTTON_LMASK) != 0) { Result = Result | MouseButton::Primary; }
            if ((Flags & SDL_BUTTON_RMASK) != 0) { Result = Result | MouseButton::Secondary; }
            if ((Flags & SDL_BUTTON_MMASK) != 0) { Result = Result | MouseButton::Middle; }
            if ((Flags & SDL_BUTTON_X1MASK) != 0) { Result = Result | MouseButton::Back; }
            if ((Flags & SDL_BUTTON_X2MASK) != 0) { Result = Result | MouseButton::Forward; }
            return Result;
        }

        MouseButton WithoutButton(MouseButton Set, MouseButton Removed) {
            return static_cast<MouseButton>(static_cast<uint8_t>(Set) & ~static_cast<uint8_t>(Removed));
        }

        /// @brief The Windows virtual-key code XenUI's KeyCode uses for an SDL key. The layout-mapped key decides,
        /// like VK codes; letters and digits fall back to their physical position on layouts without Latin keys.
        KeyCode ToKeyCode(SDL_Keycode Key, SDL_Scancode Scan) {
            if (Key >= SDLK_A && Key <= SDLK_Z) {
                return static_cast<KeyCode>(static_cast<uint16_t>(KeyCode::A) + (Key - SDLK_A));
            }
            if (Key >= SDLK_0 && Key <= SDLK_9) {
                return static_cast<KeyCode>(static_cast<uint16_t>(KeyCode::D0) + (Key - SDLK_0));
            }
            if (Key >= SDLK_F1 && Key <= SDLK_F12) {
                return static_cast<KeyCode>(static_cast<uint16_t>(KeyCode::F1) + (Key - SDLK_F1));
            }
            if (Key >= SDLK_F13 && Key <= SDLK_F24) {
                return static_cast<KeyCode>(static_cast<uint16_t>(KeyCode::F13) + (Key - SDLK_F13));
            }
            if (Key >= SDLK_KP_1 && Key <= SDLK_KP_9) {
                return static_cast<KeyCode>(static_cast<uint16_t>(KeyCode::Numpad1) + (Key - SDLK_KP_1));
            }

            switch (Key) {
                case SDLK_BACKSPACE:
                    return KeyCode::Backspace;
                case SDLK_TAB:
                    return KeyCode::Tab;
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                    return KeyCode::Enter;
                case SDLK_LSHIFT:
                case SDLK_RSHIFT:
                    return KeyCode::Shift;
                case SDLK_LCTRL:
                case SDLK_RCTRL:
                    return KeyCode::Control;
                case SDLK_LALT:
                case SDLK_RALT:
                    return KeyCode::Alt;
                case SDLK_PAUSE:
                    return KeyCode::Pause;
                case SDLK_CAPSLOCK:
                    return KeyCode::CapsLock;
                case SDLK_ESCAPE:
                    return KeyCode::Escape;
                case SDLK_SPACE:
                    return KeyCode::Space;
                case SDLK_PAGEUP:
                    return KeyCode::PageUp;
                case SDLK_PAGEDOWN:
                    return KeyCode::PageDown;
                case SDLK_END:
                    return KeyCode::End;
                case SDLK_HOME:
                    return KeyCode::Home;
                case SDLK_LEFT:
                    return KeyCode::Left;
                case SDLK_UP:
                    return KeyCode::Up;
                case SDLK_RIGHT:
                    return KeyCode::Right;
                case SDLK_DOWN:
                    return KeyCode::Down;
                case SDLK_PRINTSCREEN:
                    return KeyCode::PrintScreen;
                case SDLK_INSERT:
                    return KeyCode::Insert;
                case SDLK_DELETE:
                    return KeyCode::Delete;
                case SDLK_LGUI:
                    return KeyCode::LeftSuper;
                case SDLK_RGUI:
                    return KeyCode::RightSuper;
                case SDLK_APPLICATION:
                case SDLK_MENU:
                    return KeyCode::Menu;
                case SDLK_KP_0:
                    return KeyCode::Numpad0;
                case SDLK_KP_MULTIPLY:
                    return KeyCode::Multiply;
                case SDLK_KP_PLUS:
                    return KeyCode::Add;
                case SDLK_KP_MINUS:
                    return KeyCode::Subtract;
                case SDLK_KP_PERIOD:
                case SDLK_KP_DECIMAL:
                    return KeyCode::Decimal;
                case SDLK_KP_DIVIDE:
                    return KeyCode::Divide;
                case SDLK_NUMLOCKCLEAR:
                    return KeyCode::NumLock;
                case SDLK_SCROLLLOCK:
                    return KeyCode::ScrollLock;
                case SDLK_SEMICOLON:
                    return KeyCode::Semicolon;
                case SDLK_EQUALS:
                case SDLK_PLUS:
                    return KeyCode::Equals;
                case SDLK_COMMA:
                    return KeyCode::Comma;
                case SDLK_MINUS:
                    return KeyCode::Minus;
                case SDLK_PERIOD:
                    return KeyCode::Period;
                case SDLK_SLASH:
                    return KeyCode::Slash;
                case SDLK_GRAVE:
                    return KeyCode::Backquote;
                case SDLK_LEFTBRACKET:
                    return KeyCode::LeftBracket;
                case SDLK_BACKSLASH:
                    return KeyCode::Backslash;
                case SDLK_RIGHTBRACKET:
                    return KeyCode::RightBracket;
                case SDLK_APOSTROPHE:
                    return KeyCode::Quote;
                default:
                    break;
            }

            if (Scan >= SDL_SCANCODE_A && Scan <= SDL_SCANCODE_Z) {
                return static_cast<KeyCode>(static_cast<uint16_t>(KeyCode::A) + (Scan - SDL_SCANCODE_A));
            }
            if (Scan >= SDL_SCANCODE_1 && Scan <= SDL_SCANCODE_9) {
                return static_cast<KeyCode>(static_cast<uint16_t>(KeyCode::D1) + (Scan - SDL_SCANCODE_1));
            }
            if (Scan == SDL_SCANCODE_0) { return KeyCode::D0; }
            return KeyCode::None;
        }

        SDL_SystemCursor ToSystemCursor(CursorShape Shape) {
            switch (Shape) {
                case CursorShape::Hand:
                    return SDL_SYSTEM_CURSOR_POINTER;
                case CursorShape::IBeam:
                    return SDL_SYSTEM_CURSOR_TEXT;
                case CursorShape::Crosshair:
                    return SDL_SYSTEM_CURSOR_CROSSHAIR;
                case CursorShape::ResizeHorizontal:
                    return SDL_SYSTEM_CURSOR_EW_RESIZE;
                case CursorShape::ResizeVertical:
                    return SDL_SYSTEM_CURSOR_NS_RESIZE;
                case CursorShape::ResizeAll:
                    return SDL_SYSTEM_CURSOR_MOVE;
                case CursorShape::NotAllowed:
                    return SDL_SYSTEM_CURSOR_NOT_ALLOWED;
                case CursorShape::Wait:
                    return SDL_SYSTEM_CURSOR_WAIT;
                default:
                    return SDL_SYSTEM_CURSOR_DEFAULT;
            }
        }

        SDL_Cursor* GetSystemCursor(CursorShape Shape) {
            // Created once and kept for the process: SDL frees cursors when video shuts down.
            static std::array<SDL_Cursor*, SDL_SYSTEM_CURSOR_COUNT> Cursors {};
            const SDL_SystemCursor Id = ToSystemCursor(Shape);
            if (Cursors[Id] == nullptr) { Cursors[Id] = SDL_CreateSystemCursor(Id); }
            return Cursors[Id];
        }

        /// @brief Byte offset of the Characters-th code point of Text.
        size_t Utf8Offset(std::string_view Text, int32_t Characters) {
            size_t Offset = 0;
            for (int32_t Index = 0; Index < Characters && Offset < Text.size(); ++Index) {
                ++Offset;
                while (Offset < Text.size() && (static_cast<uint8_t>(Text[Offset]) & 0xC0) == 0x80) {
                    ++Offset;
                }
            }
            return Offset;
        }
    }  // namespace

    void WindowImpl::SetCursor(CursorShape Shape) {
        _Cursor = Shape;
        if (_MouseInside || _RelativeMode) { ApplyCursor(); }
    }

    void WindowImpl::ApplyCursor() const {
        // On Wayland every SDL cursor call re-attaches the cursor surface, so only call when something changes.
        if (_Cursor == CursorShape::Hidden || _RelativeMode) {
            if (SDL_CursorVisible()) { SDL_HideCursor(); }
            return;
        }
        SDL_Cursor* Wanted = GetSystemCursor(_Cursor);
        if (SDL_GetCursor() != Wanted) { SDL_SetCursor(Wanted); }
        if (!SDL_CursorVisible()) { SDL_ShowCursor(); }
    }

    void WindowImpl::BeginRelativePointer() {
        if (_RelativeMode || _Window == nullptr) { return; }
        SDL_GetMouseState(&_RelativeAnchor.X, &_RelativeAnchor.Y);
        if (!SDL_SetWindowRelativeMouseMode(_Window, true)) { return; }
        _RelativeMode = true;
        ApplyCursor();
    }

    void WindowImpl::EndRelativePointer() {
        if (!_RelativeMode) { return; }
        _RelativeMode = false;
        if (_Window != nullptr) {
            SDL_SetWindowRelativeMouseMode(_Window, false);
            SDL_WarpMouseInWindow(_Window, _RelativeAnchor.X, _RelativeAnchor.Y);
        }
        ApplyCursor();
    }

    void WindowImpl::DispatchPointer(PointerPhase Phase, MouseButton Button, const Vec2& Position) {
        InputDispatcher* Input = GetInput();
        if (Input == nullptr) { return; }
        Input->HandlePointer({
          .Phase     = Phase,
          .Kind      = PointerKind::Mouse,
          .Position  = Position,
          .Button    = Button,
          .Buttons   = _HeldButtons,
          .Modifiers = QueryModifiers(),
          .Time      = CurrentTime(),
        });
    }

    void WindowImpl::HandleMouseMotion(const SDL_MouseMotionEvent& Event) {
        if (_RelativeMode) {
            if (InputDispatcher* Input = GetInput(); Input != nullptr && (Event.xrel != 0.0f || Event.yrel != 0.0f)) {
                Input->HandleRelativeMotion(ToDips(Event.xrel, Event.yrel), QueryModifiers());
            }
            return;
        }
        if (!_MouseInside) {
            _MouseInside = true;
            ApplyCursor();
        }
        const Vec2 Position = ToDips(Event.x, Event.y);
        _HeldButtons        = ToButtons(Event.state);
        // Moving without having left means focus went elsewhere while the pointer sat on the caption.
        EndCaptionGrab(true);
        _LastPointer = Position;
        if (HandleCaptionButton(Position, false, false)) { return; }
        DispatchPointer(PointerPhase::Move, MouseButton::None, Position);
    }

    void WindowImpl::HandleMouseButton(const SDL_MouseButtonEvent& Event) {
        const MouseButton Button = ToButton(Event.button);
        if (Button == MouseButton::None) { return; }
        const Vec2 Position = ToDips(Event.x, Event.y);

        if (Event.down) {
            if (_Popups && _Popups->HasMenu()) {
                _Popups->DismissMenus();
                return;
            }
            if (Button == MouseButton::Primary && HandleCaptionButton(Position, true, false)) { return; }
            if (_Chrome) {
                if (InputDispatcher* Input = GetInput();
                    Input != nullptr && Input->HitTestWindowRegion(Position) == WindowRegion::Caption) {
                    // SDL's hit test moves the window on a left press; double-clicks and the window menu are ours.
                    if (Button == MouseButton::Primary && Event.clicks == 2 && _Chrome->ToggleMaximize) {
                        _Chrome->ToggleMaximize();
                        return;
                    }
                    if (Button == MouseButton::Secondary) {
                        const Vec2 Point {Event.x, Event.y};
                        SDL_ShowWindowSystemMenu(_Window,
                                                 static_cast<int>(std::lround(Point.X)),
                                                 static_cast<int>(std::lround(Point.Y)));
                        return;
                    }
                }
            }
            _HeldButtons = _HeldButtons | Button;
            DispatchPointer(PointerPhase::Down, Button, Position);
            return;
        }

        if (Button == MouseButton::Primary && HandleCaptionButton(Position, false, true)) { return; }
        _HeldButtons = WithoutButton(_HeldButtons, Button);
        DispatchPointer(PointerPhase::Up, Button, Position);
    }

    void WindowImpl::HandleWheel(const SDL_MouseWheelEvent& Event) {
        if (_Popups && _Popups->HasMenu()) { _Popups->DismissMenus(); }
        InputDispatcher* Input = GetInput();
        if (Input == nullptr) { return; }
        const float Direction = Event.direction == SDL_MOUSEWHEEL_FLIPPED ? -1.0f : 1.0f;
        Input->HandlePointer({
          .Phase      = PointerPhase::Wheel,
          .Kind       = PointerKind::Mouse,
          .Position   = ToDips(Event.mouse_x, Event.mouse_y),
          .WheelDelta = Vec2 {Event.x, Event.y} * Direction,
          .Buttons    = _HeldButtons,
          .Modifiers  = QueryModifiers(),
          .Time       = CurrentTime(),
        });
    }

    void WindowImpl::HandleKey(const SDL_KeyboardEvent& Event) {
        const KeyEvent Key {
          .Code      = ToKeyCode(Event.key, Event.scancode),
          .Modifiers = ToModifiers(Event.mod),
          .Pressed   = Event.down,
          .Repeat    = Event.repeat,
          .ScanCode  = static_cast<uint32_t>(Event.scancode),
          .Time      = CurrentTime(),
        };
        if (Key.Code == KeyCode::None) { return; }

        if (_Popups && _Popups->HasMenu()) {
            if (_Popups->RouteKey(Key)) { return; }
            if (Key.Pressed && Key.Code == KeyCode::Escape) { _Popups->DismissMenus(); }
            return;
        }

        InputDispatcher* Input = GetInput();
        if (Input != nullptr && Input->HandleKey(Key)) { return; }
        _Shortcuts.HandleKey(Key);
    }

    void WindowImpl::HandleText(const SDL_TextInputEvent& Event) {
        if (Event.text == nullptr || Event.text[0] == '\0') { return; }
        const TextEvent Text {Event.text};
        if (_Popups && _Popups->HasMenu()) {
            _Popups->RouteText(Text);
            return;
        }
        if (TextInputClient* Client = GetTextInput()) { Client->UpdateComposition({}, 0); }
        if (InputDispatcher* Input = GetInput()) { Input->HandleText(Text); }
    }

    void WindowImpl::HandleComposition(const SDL_TextEditingEvent& Event) {
        TextInputClient* Client = GetTextInput();
        if (Client == nullptr) { return; }
        const std::string_view Composing = Event.text != nullptr ? Event.text : "";
        const size_t Cursor = Event.start < 0 ? Composing.size() : Utf8Offset(Composing, Event.start);
        Client->UpdateComposition(std::string(Composing), Cursor);
        PlaceInputMethod();
    }

    TextInputClient* WindowImpl::GetTextInput() const {
        if (_Popups && _Popups->HasMenu()) { return nullptr; }
        InputDispatcher* Input = GetInput();
        return Input != nullptr ? Input->GetFocusManager().GetTextInput() : nullptr;
    }

    void WindowImpl::PlaceInputMethod() {
        if (_Window == nullptr) { return; }
        const TextInputClient* Client = GetTextInput();
        if (Client == nullptr) {
            if (_InputMethodArea) {
                _InputMethodArea.reset();
                SDL_SetTextInputArea(_Window, nullptr, 0);
            }
            return;
        }

        const float ToCoordinates = 1.0f / GetCoordinateScale();
        const Rect Caret          = Client->GetCaretBounds();
        const Rect Area {std::floor(Caret.X * ToCoordinates),
                         std::floor(Caret.Y * ToCoordinates),
                         std::ceil(std::max(Caret.Width, 1.0f) * ToCoordinates),
                         std::ceil(Caret.Height * ToCoordinates)};
        if (_InputMethodArea == Area) { return; }
        _InputMethodArea = Area;
        const SDL_Rect Native {static_cast<int>(Area.X),
                               static_cast<int>(Area.Y),
                               static_cast<int>(Area.Width),
                               static_cast<int>(Area.Height)};
        SDL_SetTextInputArea(_Window, &Native, 0);
    }

    void WindowImpl::HandleDrop(const SDL_Event& Event) {
        InputDispatcher* Input = GetInput();
        // SDL reports files only when they are dropped, so a drag in progress offers its data as external but
        // unknown; drop targets see the files on release.
        switch (Event.type) {
            case SDL_EVENT_DROP_BEGIN:
                _Drop        = {};
                _Drop.Active = true;
                _Drop.Data.External = true;
                break;
            case SDL_EVENT_DROP_POSITION:
                _Drop.Active   = true;
                _Drop.Position = ToDips(Event.drop.x, Event.drop.y);
                _Drop.Data.External = true;
                if (Input != nullptr) {
                    Input->DragOverExternal(_Drop.Position, _Drop.Data, QueryModifiers(), DropEffect::Copy);
                }
                break;
            case SDL_EVENT_DROP_FILE:
                if (Event.drop.data != nullptr) { _Drop.Data.Files.emplace_back(Event.drop.data); }
                _Drop.Position = ToDips(Event.drop.x, Event.drop.y);
                break;
            case SDL_EVENT_DROP_TEXT:
                if (Event.drop.data != nullptr) { _Drop.Data.Text = std::string(Event.drop.data); }
                _Drop.Position = ToDips(Event.drop.x, Event.drop.y);
                break;
            case SDL_EVENT_DROP_COMPLETE: {
                _Drop.Data.External = true;
                const bool HasData  = !_Drop.Data.Files.empty() || _Drop.Data.Text.has_value();
                if (Input != nullptr) {
                    if (HasData) {
                        Input->DropExternal(_Drop.Position, _Drop.Data, QueryModifiers(), DropEffect::Copy);
                    } else {
                        Input->DragLeaveExternal();
                    }
                }
                _Drop = {};
                break;
            }
            default:
                break;
        }
    }

    void WindowImpl::SetCaptionHover(WindowRegion Region) {
        if (_Chrome && _Chrome->Hovered.Peek() != Region) { _Chrome->Hovered = Region; }
    }

    bool WindowImpl::HandleCaptionButton(const Vec2& Position, bool Pressed, bool Released) {
        if (!_Chrome) { return false; }
        InputDispatcher* Input = GetInput();
        WindowRegion Region    = Input != nullptr ? Input->HitTestWindowRegion(Position) : WindowRegion::Client;
        if (!Pressed && !Released) { _PointerOverCaption = Region == WindowRegion::Caption; }
        if (Region == WindowRegion::Caption || (Region == WindowRegion::Maximize && !_Resizable)) {
            Region = WindowRegion::Client;
        }
        SetCaptionHover(Region);

        const WindowRegion Held = _Chrome->Pressed.Peek();
        if (Pressed) {
            if (Region == WindowRegion::Client) { return false; }
            if (_Popups) { _Popups->DismissMenus(); }
            _Chrome->Pressed = Region;
            return true;
        }
        if (Released) {
            if (Held == WindowRegion::Client) { return false; }
            _Chrome->Pressed = WindowRegion::Client;
            if (Held == Region) {
                const std::function<void()> Action = Region == WindowRegion::Minimize   ? _Chrome->Minimize
                                                     : Region == WindowRegion::Maximize ? _Chrome->ToggleMaximize
                                                                                        : _Chrome->Close;
                if (Action) { Action(); }
            }
            return true;
        }
        // While a caption button is held, the pointer belongs to it.
        return Held != WindowRegion::Client;
    }

    SDL_HitTestResult WindowImpl::HitTest(SDL_Window* Window, const SDL_Point* Point, void* Data) {
        auto* Self = static_cast<WindowImpl*>(Data);
        if (Self == nullptr || Self->_Window != Window) { return SDL_HITTEST_NORMAL; }

        const Vec2 Position = Self->ToDips(static_cast<float>(Point->x), static_cast<float>(Point->y));
        const Size Client   = Self->GetClientSize();
        const bool Maximized = (SDL_GetWindowFlags(Window) & SDL_WINDOW_MAXIMIZED) != 0;
        if (Self->_Resizable && !Maximized) {
            const bool Left   = Position.X < kResizeBorder;
            const bool Right  = Position.X >= Client.Width - kResizeBorder;
            const bool Top    = Position.Y < kResizeBorder;
            const bool Bottom = Position.Y >= Client.Height - kResizeBorder;
            if (Top && Left) { return SDL_HITTEST_RESIZE_TOPLEFT; }
            if (Top && Right) { return SDL_HITTEST_RESIZE_TOPRIGHT; }
            if (Bottom && Left) { return SDL_HITTEST_RESIZE_BOTTOMLEFT; }
            if (Bottom && Right) { return SDL_HITTEST_RESIZE_BOTTOMRIGHT; }
            if (Top) { return SDL_HITTEST_RESIZE_TOP; }
            if (Bottom) { return SDL_HITTEST_RESIZE_BOTTOM; }
            if (Left) { return SDL_HITTEST_RESIZE_LEFT; }
            if (Right) { return SDL_HITTEST_RESIZE_RIGHT; }
        }

        InputDispatcher* Input = Self->GetInput();
        const bool Caption     = Input != nullptr && Input->HitTestWindowRegion(Position) == WindowRegion::Caption;
        if (!Self->_InCaptionGrab) {
            Self->_PointerOverCaption = Caption;
            Self->_LastPointer        = Position;
        }
        return Caption ? SDL_HITTEST_DRAGGABLE : SDL_HITTEST_NORMAL;
    }

    bool WindowImpl::IsCaptionGrabLikely() const {
        return _Chrome != nullptr && _PointerOverCaption && _HeldButtons == MouseButton::None;
    }

    void WindowImpl::BeginCaptionGrab() {
        const double Now = CurrentTime();
        if (_InCaptionGrab && Now - _CaptionGrabStarted < kSameGrabSeconds) { return; }
        _InCaptionGrab      = true;
        _CaptionGrabStarted = Now;
        SetCaptionHover(WindowRegion::Client);

        const Vec2 Offset = _LastPointer - _CaptionClickPosition;
        const bool Double = _CaptionClickTime >= 0.0 && Now - _CaptionClickTime <= kDoubleClickSeconds &&
                            std::abs(Offset.X) <= kDoubleClickDistance && std::abs(Offset.Y) <= kDoubleClickDistance;
        if (Double) {
            // The compositor owns the pointer until release; maximizing in the middle of its move can be ignored.
            _ToggleAfterGrab  = true;
            _CaptionClickTime = -1.0;
        } else {
            _CaptionClickTime     = Now;
            _CaptionClickPosition = _LastPointer;
        }
    }

    void WindowImpl::EndCaptionGrab(bool CheckFocus) {
        if (!_InCaptionGrab) { return; }
        _InCaptionGrab = false;
        if (CheckFocus && _Chrome && SDL_GetKeyboardFocus() != _Window) { _Chrome->Active = false; }
        if (std::exchange(_ToggleAfterGrab, false) && _Chrome && _Chrome->ToggleMaximize) { _Chrome->ToggleMaximize(); }
    }

    void WindowImpl::CancelPointers() {
        _HeldButtons = MouseButton::None;
        if (_Chrome) { _Chrome->Pressed = WindowRegion::Client; }
        if (InputDispatcher* Input = GetInput()) { Input->CancelAll(); }
        EndRelativePointer();
    }
}  // namespace XenUI
