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
#include "XenUI/Core/Error.hpp"
#include "XenUI/Core/Animation.hpp"
#include "XenUI/Core/Timer.hpp"
#include "XenUI/Platform/HResult.hpp"
#include "XenUI/Platform/WindowImpl.hpp"

#include <Windows.h>
#include <objbase.h>
#include <ole2.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace XenUI {
    namespace {
        constexpr double kMaxWaitMilliseconds      = 2147483647.0;
        constexpr DWORD kAnimationWaitMilliseconds = 16;

        AppContext* CurrentContext = nullptr;

        int64_t QueryTicks() {
            LARGE_INTEGER Ticks {};
            QueryPerformanceCounter(&Ticks);
            return Ticks.QuadPart;
        }
    }  // namespace

    ComApartment::ComApartment() {
        const HRESULT Result = OleInitialize(nullptr);
        if (Result == RPC_E_CHANGED_MODE) { return; }
        ThrowIfFailed(Result, "OleInitialize");
        _Initialized = true;
    }

    ComApartment::~ComApartment() {
        if (_Initialized) { OleUninitialize(); }
    }

    AppContext::AppContext() {
        if (CurrentContext != nullptr) { throw Error("Only one XenUI::App may exist at a time"); }
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

        LARGE_INTEGER Frequency {};
        QueryPerformanceFrequency(&Frequency);
        _ClockPeriod = 1.0 / static_cast<double>(Frequency.QuadPart);
        _ClockStart  = QueryTicks();

        CurrentContext = this;
    }

    AppContext::~AppContext() {
        CurrentContext = nullptr;
    }

    AppContext& AppContext::Get() {
        if (CurrentContext == nullptr) { throw Error("XenUI::App must be created before any other XenUI object"); }
        return *CurrentContext;
    }

    AppContext* AppContext::TryGet() {
        return CurrentContext;
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
        const std::vector<WindowImpl*> Windows = _Windows;
        for (WindowImpl* Entry : Windows) {
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

    void AppContext::OnWindowOpened(WindowImpl& Opened) {
        _Windows.push_back(&Opened);
    }

    void AppContext::OnWindowClosed(WindowImpl& Closed) {
        std::erase(_Windows, &Closed);
        if (_Windows.empty()) { PostQuitMessage(0); }
    }

    void AppContext::HandleDeviceLost() {
        for (WindowImpl* Entry : _Windows) {
            Entry->ReleaseSwapChain();
        }
        _GraphicsDevice.Recreate();
        for (WindowImpl* Entry : _Windows) {
            if (Entry->IsOpen()) { Entry->RecreateSwapChain(); }
        }
    }

    void AppContext::ReportException(std::exception_ptr Exception) {
        if (!_PendingException) { _PendingException = std::move(Exception); }
    }

    double AppContext::GetTime() const {
        return static_cast<double>(QueryTicks() - _ClockStart) * _ClockPeriod;
    }

    void AppContext::RethrowPendingException() {
        if (_PendingException) { std::rethrow_exception(std::exchange(_PendingException, nullptr)); }
    }

    void AppContext::RenderNextReadyWindow() {
        std::vector<WindowImpl*> Waiting;
        std::vector<HANDLE> Handles;
        for (WindowImpl* Entry : _Windows) {
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
