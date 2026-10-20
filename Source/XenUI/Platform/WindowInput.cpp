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
#include "XenUI/Platform/PopupManager.hpp"
#include "XenUI/Platform/Utf8.hpp"
#include "XenUI/Platform/WindowImpl.hpp"
#include "XenUI/Platform/SystemDragDrop.hpp"
#include "XenUI/Input/TextInput.hpp"
#include "XenUI/Widgets/WidgetHost.hpp"

#include <Windows.h>
#include <windowsx.h>
#include <imm.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace XenUI {
    namespace {
        constexpr USHORT kGenericDesktopPage = 0x01;
        constexpr USHORT kMouseUsage         = 0x02;

        KeyModifiers QueryModifiers() {
            KeyModifiers Result = KeyModifiers::None;
            if (GetKeyState(VK_SHIFT) < 0) { Result = Result | KeyModifiers::Shift; }
            if (GetKeyState(VK_CONTROL) < 0) { Result = Result | KeyModifiers::Control; }
            if (GetKeyState(VK_MENU) < 0) { Result = Result | KeyModifiers::Alt; }
            if (GetKeyState(VK_LWIN) < 0 || GetKeyState(VK_RWIN) < 0) { Result = Result | KeyModifiers::Super; }
            return Result;
        }

        MouseButton ButtonsFromWParam(WPARAM WParam) {
            const auto Flags   = GET_KEYSTATE_WPARAM(WParam);
            MouseButton Result = MouseButton::None;
            if ((Flags & MK_LBUTTON) != 0) { Result = Result | MouseButton::Primary; }
            if ((Flags & MK_RBUTTON) != 0) { Result = Result | MouseButton::Secondary; }
            if ((Flags & MK_MBUTTON) != 0) { Result = Result | MouseButton::Middle; }
            if ((Flags & MK_XBUTTON1) != 0) { Result = Result | MouseButton::Back; }
            if ((Flags & MK_XBUTTON2) != 0) { Result = Result | MouseButton::Forward; }
            return Result;
        }

        MouseButton WithoutButton(MouseButton Set, MouseButton Removed) {
            return static_cast<MouseButton>(static_cast<uint8_t>(Set) & ~static_cast<uint8_t>(Removed));
        }

        double CurrentTime() {
            const AppContext* Context = AppContext::TryGet();
            return Context != nullptr ? Context->GetTime() : 0.0;
        }

        LPCWSTR GetSystemCursorName(CursorShape Shape) {
            switch (Shape) {
                case CursorShape::Hand:
                    return IDC_HAND;
                case CursorShape::IBeam:
                    return IDC_IBEAM;
                case CursorShape::Crosshair:
                    return IDC_CROSS;
                case CursorShape::ResizeHorizontal:
                    return IDC_SIZEWE;
                case CursorShape::ResizeVertical:
                    return IDC_SIZENS;
                case CursorShape::ResizeAll:
                    return IDC_SIZEALL;
                case CursorShape::NotAllowed:
                    return IDC_NO;
                case CursorShape::Wait:
                    return IDC_WAIT;
                default:
                    return IDC_ARROW;
            }
        }
    }  // namespace

    void WindowImpl::SetCursor(CursorShape Shape) {
        _Cursor = Shape;
        if (_TrackingMouse || _RelativeMode) { ApplyCursor(); }
    }

    void WindowImpl::BeginRelativePointer() {
        if (_RelativeMode) { return; }

        const RAWINPUTDEVICE Device {kGenericDesktopPage, kMouseUsage, 0, _Hwnd};
        if (!RegisterRawInputDevices(&Device, 1, sizeof(Device))) { return; }

        GetCursorPos(&_RelativeAnchor);
        const RECT Pin {_RelativeAnchor.x, _RelativeAnchor.y, _RelativeAnchor.x + 1, _RelativeAnchor.y + 1};
        ClipCursor(&Pin);
        _LastAbsoluteRaw.reset();
        _RelativeMode = true;
        ApplyCursor();
    }

    void WindowImpl::EndRelativePointer() {
        if (!_RelativeMode) { return; }

        ClipCursor(nullptr);
        SetCursorPos(_RelativeAnchor.x, _RelativeAnchor.y);
        const RAWINPUTDEVICE Device {kGenericDesktopPage, kMouseUsage, RIDEV_REMOVE, nullptr};
        RegisterRawInputDevices(&Device, 1, sizeof(Device));
        _RelativeMode = false;
    }

    std::optional<LRESULT> WindowImpl::HandleInputMessage(UINT Message, WPARAM WParam, LPARAM LParam) {
        switch (Message) {
            case WM_MOUSEMOVE:
                if (!_TrackingMouse) {
                    TRACKMOUSEEVENT Tracking {.cbSize    = sizeof(TRACKMOUSEEVENT),
                                              .dwFlags   = TME_LEAVE,
                                              .hwndTrack = _Hwnd};
                    TrackMouseEvent(&Tracking);
                    _TrackingMouse = true;
                    ApplyCursor();
                }
                if (!_RelativeMode) {
                    DispatchMouse(PointerPhase::Move, MouseButton::None, WParam, ClientToDips(LParam));
                }
                return 0;
            case WM_MOUSELEAVE:
                _TrackingMouse = false;
                DispatchMouse(PointerPhase::Leave, MouseButton::None, 0, {});
                return 0;
            case WM_LBUTTONDOWN:
            case WM_RBUTTONDOWN:
            case WM_MBUTTONDOWN:
            case WM_XBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_RBUTTONUP:
            case WM_MBUTTONUP:
            case WM_XBUTTONUP: {
                MouseButton Button = MouseButton::Primary;
                if (Message == WM_RBUTTONDOWN || Message == WM_RBUTTONUP) { Button = MouseButton::Secondary; }
                if (Message == WM_MBUTTONDOWN || Message == WM_MBUTTONUP) { Button = MouseButton::Middle; }
                if (Message == WM_XBUTTONDOWN || Message == WM_XBUTTONUP) {
                    Button = GET_XBUTTON_WPARAM(WParam) == XBUTTON1 ? MouseButton::Back : MouseButton::Forward;
                }

                const bool Pressed = Message == WM_LBUTTONDOWN || Message == WM_RBUTTONDOWN ||
                                     Message == WM_MBUTTONDOWN || Message == WM_XBUTTONDOWN;
                if (Pressed && _Popups && _Popups->HasMenu()) {
                    _Popups->DismissMenus();
                    return 0;
                }
                if (Pressed) {
                    if (_HeldButtons == MouseButton::None) { SetCapture(_Hwnd); }
                    _HeldButtons = _HeldButtons | Button;
                    DispatchMouse(PointerPhase::Down, Button, WParam, ClientToDips(LParam));
                } else {
                    _HeldButtons = WithoutButton(_HeldButtons, Button);
                    DispatchMouse(PointerPhase::Up, Button, WParam, ClientToDips(LParam));
                    if (_HeldButtons == MouseButton::None) {
                        _ReleasingCapture = true;
                        ReleaseCapture();
                        _ReleasingCapture = false;
                    }
                }
                return (Message == WM_XBUTTONDOWN || Message == WM_XBUTTONUP) ? TRUE : 0;
            }
            case WM_MOUSEWHEEL:
            case WM_MOUSEHWHEEL:
                if (_Popups && _Popups->HasMenu()) { _Popups->DismissMenus(); }
                DispatchWheel(WParam, LParam, Message == WM_MOUSEHWHEEL);
                return 0;
            case WM_NCLBUTTONDOWN:
            case WM_NCRBUTTONDOWN:
            case WM_NCMBUTTONDOWN:
                if (_Popups) { _Popups->DismissMenus(); }
                return std::nullopt;
            case WM_SETCURSOR:
                if (LOWORD(LParam) == HTCLIENT) {
                    ApplyCursor();
                    return TRUE;
                }
                return std::nullopt;
            case WM_CAPTURECHANGED:
                if (!_ReleasingCapture && _HeldButtons != MouseButton::None && !IsSystemDragActive()) {
                    CancelPointers();
                }
                return 0;
            case WM_KILLFOCUS:
            case WM_CANCELMODE:
                if (IsSystemDragActive()) { return std::nullopt; }
                CancelPointers();
                if (_Popups) { _Popups->DismissAll(); }
                return std::nullopt;
            case WM_INPUT:
                HandleRawInput(LParam);
                return std::nullopt;
            case WM_KEYDOWN:
            case WM_KEYUP:
                return DispatchKey(WParam, LParam, Message == WM_KEYDOWN) ? std::optional<LRESULT>(0) : std::nullopt;
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
                _LastSysKeyHandled = DispatchKey(WParam, LParam, Message == WM_SYSKEYDOWN);
                return _LastSysKeyHandled ? std::optional<LRESULT>(0) : std::nullopt;
            case WM_SYSCHAR:
                return _LastSysKeyHandled ? std::optional<LRESULT>(0) : std::nullopt;
            case WM_CHAR:
                DispatchCharacter(WParam);
                return 0;
            case WM_IME_SETCONTEXT:
                if (GetTextInput() != nullptr) { LParam &= ~static_cast<LPARAM>(ISC_SHOWUICOMPOSITIONWINDOW); }
                return DefWindowProcW(_Hwnd, Message, WParam, LParam);
            case WM_IME_STARTCOMPOSITION:
                if (TextInputClient* Client = GetTextInput()) {
                    PlaceInputMethod(*Client);
                    return 0;
                }
                return std::nullopt;
            case WM_IME_COMPOSITION:
                if (TextInputClient* Client = GetTextInput()) {
                    HandleComposition(*Client, LParam);
                    return 0;
                }
                return std::nullopt;
            case WM_IME_ENDCOMPOSITION:
                if (TextInputClient* Client = GetTextInput()) {
                    Client->UpdateComposition({}, 0);
                    return 0;
                }
                return std::nullopt;
            default:
                return std::nullopt;
        }
    }

    void WindowImpl::DispatchMouse(PointerPhase Phase, MouseButton Button, WPARAM WParam, const Vec2& Position) {
        InputDispatcher* Input = GetInput();
        if (Input == nullptr) { return; }

        Input->HandlePointer({
          .Phase     = Phase,
          .Kind      = PointerKind::Mouse,
          .Position  = Position,
          .Button    = Button,
          .Buttons   = ButtonsFromWParam(WParam),
          .Modifiers = QueryModifiers(),
          .Time      = CurrentTime(),
        });
    }

    void WindowImpl::DispatchWheel(WPARAM WParam, LPARAM LParam, bool Horizontal) {
        InputDispatcher* Input = GetInput();
        if (Input == nullptr) { return; }

        POINT Screen {GET_X_LPARAM(LParam), GET_Y_LPARAM(LParam)};
        ScreenToClient(_Hwnd, &Screen);
        const float Notches = static_cast<float>(GET_WHEEL_DELTA_WPARAM(WParam)) / static_cast<float>(WHEEL_DELTA);
        const float Scale   = GetDpiScale();

        Input->HandlePointer({
          .Phase      = PointerPhase::Wheel,
          .Kind       = PointerKind::Mouse,
          .Position   = {static_cast<float>(Screen.x) / Scale, static_cast<float>(Screen.y) / Scale},
          .WheelDelta = Horizontal ? Vec2 {Notches, 0.0f} : Vec2 {0.0f, Notches},
          .Buttons    = ButtonsFromWParam(WParam),
          .Modifiers  = QueryModifiers(),
          .Time       = CurrentTime(),
        });
    }

    bool WindowImpl::DispatchKey(WPARAM WParam, LPARAM LParam, bool Pressed) {
        const KeyEvent Event {
          .Code      = static_cast<KeyCode>(WParam),
          .Modifiers = QueryModifiers(),
          .Pressed   = Pressed,
          .Repeat    = Pressed && (LParam & (1 << 30)) != 0,
          .ScanCode  = static_cast<uint32_t>((LParam >> 16) & 0xFF),
          .Time      = CurrentTime(),
        };

        if (_Popups && _Popups->HasMenu()) {
            if (_Popups->RouteKey(Event)) { return true; }
            if (Event.Pressed && Event.Code == KeyCode::Escape) {
                _Popups->DismissMenus();
                return true;
            }
            return false;
        }

        InputDispatcher* Input = GetInput();
        if (Input != nullptr && Input->HandleKey(Event)) { return true; }
        return _Shortcuts.HandleKey(Event);
    }

    void WindowImpl::DispatchCharacter(WPARAM WParam) {
        const auto Unit = static_cast<wchar_t>(WParam);
        if (IS_HIGH_SURROGATE(Unit)) {
            _HighSurrogate = Unit;
            return;
        }

        std::wstring Characters;
        if (IS_LOW_SURROGATE(Unit)) {
            if (_HighSurrogate == 0) { return; }
            Characters = {_HighSurrogate, Unit};
        } else {
            if (Unit < 0x20 || Unit == 0x7F) { return; }
            Characters = {Unit};
        }
        _HighSurrogate = 0;

        if (_Popups && _Popups->HasMenu()) {
            _Popups->RouteText({Narrow(Characters)});
            return;
        }
        if (InputDispatcher* Input = GetInput()) { Input->HandleText({Narrow(Characters)}); }
    }

    TextInputClient* WindowImpl::GetTextInput() const {
        if (_Popups && _Popups->HasMenu()) { return nullptr; }
        InputDispatcher* Input = GetInput();
        return Input != nullptr ? Input->GetFocusManager().GetTextInput() : nullptr;
    }

    void WindowImpl::HandleComposition(TextInputClient& Client, LPARAM Flags) {
        HIMC Context = ImmGetContext(_Hwnd);
        if (Context == nullptr) { return; }
        const auto Read = [Context](DWORD Kind) {
            const LONG Bytes = ImmGetCompositionStringW(Context, Kind, nullptr, 0);
            std::wstring Text(static_cast<size_t>(std::max<LONG>(Bytes, 0)) / sizeof(wchar_t), L'\0');
            if (Bytes > 0) { ImmGetCompositionStringW(Context, Kind, Text.data(), static_cast<DWORD>(Bytes)); }
            return Text;
        };

        if ((Flags & GCS_RESULTSTR) != 0) {
            const std::wstring Result = Read(GCS_RESULTSTR);
            Client.UpdateComposition({}, 0);
            if (!Result.empty()) {
                if (InputDispatcher* Input = GetInput()) { Input->HandleText({Narrow(Result)}); }
            }
        }
        if ((Flags & GCS_COMPSTR) != 0) {
            const std::wstring Composing = Read(GCS_COMPSTR);
            const LONG Cursor            = (Flags & GCS_CURSORPOS) != 0
                                             ? ImmGetCompositionStringW(Context, GCS_CURSORPOS, nullptr, 0)
                                             : static_cast<LONG>(Composing.size());
            const size_t Units           = std::min(static_cast<size_t>(std::max<LONG>(Cursor, 0)), Composing.size());
            Client.UpdateComposition(Narrow(Composing), Narrow(std::wstring_view(Composing).substr(0, Units)).size());
        }
        ImmReleaseContext(_Hwnd, Context);
        PlaceInputMethod(Client);
    }

    void WindowImpl::PlaceInputMethod(const TextInputClient& Client) const {
        HIMC Context = ImmGetContext(_Hwnd);
        if (Context == nullptr) { return; }
        const float Scale = GetDpiScale();
        const Rect Caret  = Client.GetCaretBounds();
        const RECT Pixels {static_cast<LONG>(std::floor(Caret.X * Scale)),
                           static_cast<LONG>(std::floor(Caret.Y * Scale)),
                           static_cast<LONG>(std::ceil((Caret.X + Caret.Width) * Scale)) + 1,
                           static_cast<LONG>(std::ceil((Caret.Y + Caret.Height) * Scale))};
        COMPOSITIONFORM Composition {.dwStyle = CFS_POINT, .ptCurrentPos = {Pixels.left, Pixels.top}};
        ImmSetCompositionWindow(Context, &Composition);
        CANDIDATEFORM Candidates {.dwIndex      = 0,
                                  .dwStyle      = CFS_EXCLUDE,
                                  .ptCurrentPos = {Pixels.left, Pixels.bottom},
                                  .rcArea       = Pixels};
        ImmSetCandidateWindow(Context, &Candidates);
        ImmReleaseContext(_Hwnd, Context);
    }

    void WindowImpl::HandleRawInput(LPARAM LParam) {
        if (!_RelativeMode) { return; }

        RAWINPUT Raw {};
        UINT Size = sizeof(Raw);
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(LParam), RID_INPUT, &Raw, &Size, sizeof(RAWINPUTHEADER)) ==
              static_cast<UINT>(-1) ||
            Raw.header.dwType != RIM_TYPEMOUSE) {
            return;
        }

        const RAWMOUSE& Mouse = Raw.data.mouse;
        Vec2 Delta;
        if ((Mouse.usFlags & MOUSE_MOVE_ABSOLUTE) != 0) {
            const bool VirtualDesktop = (Mouse.usFlags & MOUSE_VIRTUAL_DESKTOP) != 0;
            const int Width           = GetSystemMetrics(VirtualDesktop ? SM_CXVIRTUALSCREEN : SM_CXSCREEN);
            const int Height          = GetSystemMetrics(VirtualDesktop ? SM_CYVIRTUALSCREEN : SM_CYSCREEN);
            const POINT Absolute {std::lround(static_cast<double>(Mouse.lLastX) / 65535.0 * Width),
                                  std::lround(static_cast<double>(Mouse.lLastY) / 65535.0 * Height)};
            if (_LastAbsoluteRaw) {
                Delta = {static_cast<float>(Absolute.x - _LastAbsoluteRaw->x),
                         static_cast<float>(Absolute.y - _LastAbsoluteRaw->y)};
            }
            _LastAbsoluteRaw = Absolute;
        } else {
            Delta = {static_cast<float>(Mouse.lLastX), static_cast<float>(Mouse.lLastY)};
        }

        if (Delta == Vec2 {}) { return; }
        if (InputDispatcher* Input = GetInput()) {
            Input->HandleRelativeMotion(Delta / GetDpiScale(), QueryModifiers());
        }
    }

    void WindowImpl::CancelPointers() {
        _HeldButtons = MouseButton::None;
        if (InputDispatcher* Input = GetInput()) { Input->CancelAll(); }
        EndRelativePointer();
    }

    void WindowImpl::ApplyCursor() const {
        if (_Cursor == CursorShape::Hidden || _RelativeMode) {
            ::SetCursor(nullptr);
            return;
        }
        ::SetCursor(LoadCursorW(nullptr, GetSystemCursorName(_Cursor)));
    }

    std::optional<DropEffect> WindowImpl::PerformSystemDrag(const DragData& Data, DropEffect Allowed) {
        if (IsSystemDragActive()) { return std::nullopt; }
        const DropEffect Result = RunSystemDrag(Data, Allowed);
        _HeldButtons            = MouseButton::None;
        if (GetCapture() == _Hwnd) {
            _ReleasingCapture = true;
            ReleaseCapture();
            _ReleasingCapture = false;
        }
        return Result;
    }

    Vec2 WindowImpl::ScreenToDips(POINT Screen) const {
        ScreenToClient(_Hwnd, &Screen);
        const float Scale = GetDpiScale();
        return {static_cast<float>(Screen.x) / Scale, static_cast<float>(Screen.y) / Scale};
    }

    InputDispatcher* WindowImpl::GetInput() const {
        return _Content ? &_Content->GetInput() : nullptr;
    }

    Vec2 WindowImpl::ClientToDips(LPARAM LParam) const {
        const float Scale = GetDpiScale();
        return {static_cast<float>(GET_X_LPARAM(LParam)) / Scale, static_cast<float>(GET_Y_LPARAM(LParam)) / Scale};
    }
}  // namespace XenUI
