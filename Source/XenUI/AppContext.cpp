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
#include "XenUI/Platform/WindowCore.hpp"

#include <algorithm>
#include <mutex>
#include <utility>

namespace XenUI {
    namespace {
        AppContext* CurrentContext = nullptr;

        /// @brief Work posted from any thread. Outlives the App, so a late Post finds no target instead of a dangling
        /// queue.
        struct PostedWork {
            std::mutex Mutex;
            std::vector<std::function<void()>> Pending;
            /// @brief What the platform wakes to run the queue (the dispatcher window on Windows), or null when no
            /// App is running.
            void* Target = nullptr;
        };

        PostedWork& GetPostedWork() {
            static PostedWork Instance;
            return Instance;
        }
    }  // namespace

    AppContext::AppContext() : _Platform(CreatePlatformState()), _ClockStart(std::chrono::steady_clock::now()) {
        if (CurrentContext != nullptr) { throw Error("Only one XenUI::App may exist at a time"); }

        CurrentContext   = this;
        void* const Wake = CreateDispatcher();

        PostedWork& Work = GetPostedWork();
        std::lock_guard Lock(Work.Mutex);
        Work.Target = Wake;
    }

    AppContext::~AppContext() {
        {
            PostedWork& Work = GetPostedWork();
            std::lock_guard Lock(Work.Mutex);
            Work.Target = nullptr;
            Work.Pending.clear();
        }
        DestroyDispatcher();
        CurrentContext = nullptr;
    }

    bool AppContext::Post(std::function<void()> Work) {
        PostedWork& Queue = GetPostedWork();
        std::lock_guard Lock(Queue.Mutex);
        if (Queue.Target == nullptr || !Work) { return false; }
        // One wake-up covers everything queued until RunPosted takes the batch.
        const bool Wake = Queue.Pending.empty();
        Queue.Pending.push_back(std::move(Work));
        if (Wake) { WakeDispatcher(Queue.Target); }
        return true;
    }

    void AppContext::RunPosted() {
        std::vector<std::function<void()>> Batch;
        {
            PostedWork& Queue = GetPostedWork();
            std::lock_guard Lock(Queue.Mutex);
            Batch.swap(Queue.Pending);
        }
        for (const std::function<void()>& Work : Batch) {
            try {
                Work();
            } catch (...) { ReportException(std::current_exception()); }
        }
    }

    AppContext& AppContext::Get() {
        if (CurrentContext == nullptr) { throw Error("XenUI::App must be created before any other XenUI object"); }
        return *CurrentContext;
    }

    AppContext* AppContext::TryGet() {
        return CurrentContext;
    }

    void AppContext::OnWindowOpened(WindowCore& Opened) {
        _Windows.push_back(&Opened);
    }

    void AppContext::OnWindowClosed(WindowCore& Closed) {
        std::erase(_Windows, &Closed);
        if (_Windows.empty()) { Quit(0); }
    }

    void AppContext::HandleDeviceLost() {
        for (WindowCore* Entry : _Windows) {
            Entry->ReleaseSwapChain();
        }
        _GraphicsDevice.Recreate();
        for (WindowCore* Entry : _Windows) {
            if (Entry->IsOpen()) { Entry->RecreateSwapChain(); }
        }
    }

    void AppContext::ReportException(std::exception_ptr Exception) {
        if (!_PendingException) { _PendingException = std::move(Exception); }
    }

    double AppContext::GetTime() const {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - _ClockStart).count();
    }

    void AppContext::RethrowPendingException() {
        if (_PendingException) { std::rethrow_exception(std::exchange(_PendingException, nullptr)); }
    }
}  // namespace XenUI
