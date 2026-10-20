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

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace XenUI {
    /// @brief Callbacks scheduled on the UI thread's run loop. One queue per thread.
    class XENUI_API TimerQueue {
    public:
        [[nodiscard]] static TimerQueue& Get();

        /// @param Interval Seconds between repeats, or 0 for a one-shot timer.
        uint64_t Schedule(double Delay, double Interval, std::function<void()> Callback);
        void Cancel(uint64_t Id);
        [[nodiscard]] bool IsScheduled(uint64_t Id) const;

        /// @brief Runs every callback that is due. Callbacks may schedule or cancel timers.
        void RunDue();

        [[nodiscard]] std::optional<double> GetNextDue() const;

        /// @brief Seconds on the queue's clock: a steady clock unless a time source was installed.
        [[nodiscard]] double Now() const;

        /// @brief Replaces the clock, for deterministic tests. Pass an empty function to restore the default.
        void SetTimeSource(std::function<double()> Source) { _TimeSource = std::move(Source); }

    private:
        struct Entry {
            uint64_t Id;
            double Due;
            double Interval;
            std::function<void()> Callback;
        };

        std::vector<Entry> _Entries;
        std::function<double()> _TimeSource;
        uint64_t _NextId = 1;
    };

    /// @brief Owning handle to a scheduled callback. Cancels it when destroyed or reassigned.
    class XENUI_API Timer {
    public:
        Timer() = default;
        ~Timer();

        Timer(const Timer&)            = delete;
        Timer& operator=(const Timer&) = delete;
        Timer(Timer&& Other) noexcept;
        Timer& operator=(Timer&& Other) noexcept;

        [[nodiscard]] static Timer After(double Seconds, std::function<void()> Callback);
        [[nodiscard]] static Timer Every(double Seconds, std::function<void()> Callback);

        void Cancel();
        [[nodiscard]] bool IsActive() const;

    private:
        explicit Timer(uint64_t Id) : _Id(Id) {}

        uint64_t _Id = 0;
    };
}  // namespace XenUI
