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

#include "XenUI/Core/Timer.hpp"

#include <algorithm>
#include <chrono>
#include <utility>

namespace XenUI {
    TimerQueue& TimerQueue::Get() {
        thread_local TimerQueue Queue;
        return Queue;
    }

    uint64_t TimerQueue::Schedule(double Delay, double Interval, std::function<void()> Callback) {
        const uint64_t Id = _NextId++;
        _Entries.push_back({Id, Now() + std::max(Delay, 0.0), std::max(Interval, 0.0), std::move(Callback)});
        return Id;
    }

    void TimerQueue::Cancel(uint64_t Id) {
        std::erase_if(_Entries, [Id](const Entry& Candidate) { return Candidate.Id == Id; });
    }

    bool TimerQueue::IsScheduled(uint64_t Id) const {
        return std::any_of(_Entries.begin(), _Entries.end(), [Id](const Entry& Candidate) {
            return Candidate.Id == Id;
        });
    }

    void TimerQueue::RunDue() {
        const double Current = Now();
        std::vector<uint64_t> Due;
        for (const Entry& Candidate : _Entries) {
            if (Candidate.Due <= Current) { Due.push_back(Candidate.Id); }
        }

        for (const uint64_t Id : Due) {
            const auto Found = std::find_if(_Entries.begin(), _Entries.end(), [Id](const Entry& Candidate) {
                return Candidate.Id == Id;
            });
            if (Found == _Entries.end()) { continue; }

            const std::function<void()> Callback = Found->Callback;
            if (Found->Interval > 0.0) {
                Found->Due = std::max(Found->Due + Found->Interval, Current);
            } else {
                _Entries.erase(Found);
            }
            if (Callback) { Callback(); }
        }
    }

    std::optional<double> TimerQueue::GetNextDue() const {
        if (_Entries.empty()) { return std::nullopt; }
        return std::min_element(_Entries.begin(),
                                _Entries.end(),
                                [](const Entry& Left, const Entry& Right) { return Left.Due < Right.Due; })
          ->Due;
    }

    double TimerQueue::Now() const {
        if (_TimeSource) { return _TimeSource(); }
        using Clock = std::chrono::steady_clock;
        return std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
    }

    Timer::~Timer() {
        Cancel();
    }

    Timer::Timer(Timer&& Other) noexcept : _Id(std::exchange(Other._Id, 0)) {}

    Timer& Timer::operator=(Timer&& Other) noexcept {
        if (this != &Other) {
            Cancel();
            _Id = std::exchange(Other._Id, 0);
        }
        return *this;
    }

    Timer Timer::After(double Seconds, std::function<void()> Callback) {
        return Timer(TimerQueue::Get().Schedule(Seconds, 0.0, std::move(Callback)));
    }

    Timer Timer::Every(double Seconds, std::function<void()> Callback) {
        return Timer(TimerQueue::Get().Schedule(Seconds, Seconds, std::move(Callback)));
    }

    void Timer::Cancel() {
        if (_Id == 0) { return; }
        TimerQueue::Get().Cancel(_Id);
        _Id = 0;
    }

    bool Timer::IsActive() const {
        return _Id != 0 && TimerQueue::Get().IsScheduled(_Id);
    }
}  // namespace XenUI
