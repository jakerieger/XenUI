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
#include "XenUI/Core/Animation.hpp"
#include "XenUI/Core/Timer.hpp"
#include "XenUI/Platform/Win32/HResult.hpp"
#include "XenUI/Platform/Win32/WindowImpl.hpp"

#include <Windows.h>
#include <objbase.h>
#include <ole2.h>

#include <algorithm>
#include <cmath>
#include <mutex>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace XenUI {
    namespace {
        constexpr double kMaxWaitMilliseconds      = 2147483647.0;
        constexpr DWORD kAnimationWaitMilliseconds = 16;

        constexpr wchar_t kDispatcherClassName[] = L"XenUI.Dispatcher";
        constexpr UINT kRunPostedMessage         = WM_APP;

        LRESULT CALLBACK DispatcherProc(HWND Hwnd, UINT Message, WPARAM WParam, LPARAM LParam) {
            if (Message == kRunPostedMessage) {
                if (AppContext* Context = AppContext::TryGet()) { Context->RunPosted(); }
                return 0;
            }
            return DefWindowProcW(Hwnd, Message, WParam, LParam);
        }

        /// @brief Initializes OLE (and COM) on the UI thread for drag and drop, the clipboard and UI Automation.
        class ComApartment {
        public:
            ComApartment() {
                const HRESULT Result = OleInitialize(nullptr);
                if (Result == RPC_E_CHANGED_MODE) { return; }
                ThrowIfFailed(Result, "OleInitialize");
                _Initialized = true;
            }

            ~ComApartment() {
                if (_Initialized) { OleUninitialize(); }
            }

            ComApartment(const ComApartment&)            = delete;
            ComApartment& operator=(const ComApartment&) = delete;

        private:
            bool _Initialized = false;
        };
    }  // namespace

    struct AppContext::PlatformState {
        ComApartment Apartment;
        HWND Dispatcher = nullptr;
    };

    void AppContext::PlatformStateDeleter::operator()(PlatformState* State) const {
        delete State;
    }

    AppContext::PlatformState* AppContext::CreatePlatformState() {
        auto* State = new PlatformState();
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        return State;
    }

    void* AppContext::CreateDispatcher() {
        static std::once_flag Registered;
        std::call_once(Registered, [] {
            const WNDCLASSEXW WindowClass {
              .cbSize        = sizeof(WNDCLASSEXW),
              .lpfnWndProc   = &DispatcherProc,
              .hInstance     = reinterpret_cast<HINSTANCE>(&__ImageBase),
              .lpszClassName = kDispatcherClassName,
            };
            if (RegisterClassExW(&WindowClass) == 0) { ThrowLastError("RegisterClassExW"); }
        });
        _Platform->Dispatcher = CreateWindowExW(0,
                                                kDispatcherClassName,
                                                L"",
                                                0,
                                                0,
                                                0,
                                                0,
                                                0,
                                                HWND_MESSAGE,
                                                nullptr,
                                                reinterpret_cast<HINSTANCE>(&__ImageBase),
                                                nullptr);
        if (_Platform->Dispatcher == nullptr) { ThrowLastError("CreateWindowExW"); }
        return _Platform->Dispatcher;
    }

    void AppContext::DestroyDispatcher() {
        if (_Platform->Dispatcher != nullptr) { DestroyWindow(std::exchange(_Platform->Dispatcher, nullptr)); }
    }

    void AppContext::WakeDispatcher(void* Target) {
        PostMessageW(static_cast<HWND>(Target), kRunPostedMessage, 0, 0);
    }

    int AppContext::Run() {
        if (_Windows.empty()) { return 0; }

        while (true) {
            MSG Message {};
            while (PeekMessageW(&Message, nullptr, 0, 0, PM_REMOVE)) {
                if (Message.message == WM_QUIT) { return static_cast<int>(Message.wParam); }
                TranslateMessage(&Message);
                DispatchMessageW(&Message);
                RethrowPendingException();
            }

            AnimationTicker::Get().Tick();
            RenderNextReadyWindow();
            RethrowPendingException();
            TimerQueue::Get().RunDue();
            RethrowPendingException();
        }
    }

    void AppContext::RenderReadyWindows() {
        AnimationTicker::Get().Tick();
        // Every window on this platform is a WindowImpl.
        const std::vector<WindowCore*> Windows = _Windows;
        for (WindowCore* Core : Windows) {
            auto* Entry = static_cast<WindowImpl*>(Core);
            if (!Entry->HasPendingFrame()) { continue; }
            if (WaitForSingleObject(Entry->GetFrameWaitable(), 0) != WAIT_OBJECT_0) { continue; }
            Entry->MarkFrameReady();
            Entry->RenderFrame();
        }
        TimerQueue::Get().RunDue();
    }

    void AppContext::Quit(int ExitCode) {
        PostQuitMessage(ExitCode);
    }

    void AppContext::RenderNextReadyWindow() {
        std::vector<WindowImpl*> Waiting;
        std::vector<HANDLE> Handles;
        for (WindowCore* Core : _Windows) {
            auto* Entry = static_cast<WindowImpl*>(Core);
            if (!Entry->HasPendingFrame() || Handles.size() >= MAXIMUM_WAIT_OBJECTS - 1) { continue; }
            Waiting.push_back(Entry);
            Handles.push_back(Entry->GetFrameWaitable());
        }

        DWORD Timeout = AnimationTicker::Get().HasActive() ? kAnimationWaitMilliseconds : INFINITE;
        if (const std::optional<double> NextDue = TimerQueue::Get().GetNextDue()) {
            const double Remaining = std::max(0.0, *NextDue - TimerQueue::Get().Now());
            Timeout =
              std::min(Timeout, static_cast<DWORD>(std::min(std::ceil(Remaining * 1000.0), kMaxWaitMilliseconds)));
        }

        const auto Count     = static_cast<DWORD>(Handles.size());
        const DWORD Signaled = MsgWaitForMultipleObjectsEx(Count,
                                                           Count > 0 ? Handles.data() : nullptr,
                                                           Timeout,
                                                           QS_ALLINPUT,
                                                           MWMO_INPUTAVAILABLE);
        if (Signaled >= WAIT_OBJECT_0 + Count) { return; }

        WindowImpl& Ready = *Waiting[Signaled - WAIT_OBJECT_0];
        Ready.MarkFrameReady();
        Ready.RenderFrame();
    }
}  // namespace XenUI
