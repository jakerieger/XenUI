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

#include "XenUI/Api.hpp"

#include <functional>
#include <vector>

namespace XenUI {
    class SignalSource;

    /// @brief Something that depends on signals. Dependencies are recorded while the observer is the current
    /// TrackingScope and are re-recorded from scratch on every tracked run. UI thread only.
    class XENUI_API Observer {
    public:
        Observer() = default;
        virtual ~Observer();

        Observer(const Observer&)            = delete;
        Observer& operator=(const Observer&) = delete;

        /// @brief Called when any recorded dependency changes. Must not run user code synchronously; schedule work.
        virtual void OnDependencyChanged() = 0;

        void ClearDependencies();
        [[nodiscard]] size_t GetDependencyCount() const { return _Dependencies.size(); }

    private:
        friend class SignalSource;

        std::vector<SignalSource*> _Dependencies;
    };

    /// @brief The observable half of a signal: records readers and notifies them on change.
    class XENUI_API SignalSource {
    public:
        SignalSource() = default;
        ~SignalSource();

        SignalSource(const SignalSource&)            = delete;
        SignalSource& operator=(const SignalSource&) = delete;

        /// @brief Registers the current TrackingScope's observer, if any, as a dependent.
        void Track() const;

        /// @brief Notifies dependents, then runs scheduled effects unless inside a Batch.
        void Notify();

        [[nodiscard]] size_t GetObserverCount() const { return _Observers.size(); }

    private:
        friend class Observer;

        mutable std::vector<Observer*> _Observers;
    };

    /// @brief Makes Target the observer that signal reads are attributed to. By default its old dependencies are
    /// cleared first; pass ClearExisting = false to add to them instead.
    class XENUI_API TrackingScope {
    public:
        explicit TrackingScope(Observer& Target, bool ClearExisting = true);
        ~TrackingScope();

        TrackingScope(const TrackingScope&)            = delete;
        TrackingScope& operator=(const TrackingScope&) = delete;

        [[nodiscard]] static Observer* GetCurrent();

    private:
        Observer* _Previous;
    };

    /// @brief Runs Callback immediately, then again whenever a signal it read changes. Re-runs are deferred until
    /// the outermost notification or Batch completes, so they always observe a consistent state.
    class XENUI_API Effect final : public Observer {
    public:
        explicit Effect(std::function<void()> Callback);
        ~Effect() override;

        void OnDependencyChanged() override;

        /// @internal
        void Run();

    private:
        std::function<void()> _Callback;
        bool _Scheduled = false;
    };

    /// @brief Runs Callback with signal reads not attributed to the current observer.
    XENUI_API void Untracked(const std::function<void()>& Callback);

    /// @brief Defers effects until Callback returns, so several signal writes trigger each effect at most once.
    XENUI_API void Batch(const std::function<void()>& Callback);
}  // namespace XenUI
