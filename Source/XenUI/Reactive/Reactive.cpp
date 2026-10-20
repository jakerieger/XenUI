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

#include "XenUI/Reactive/Reactive.hpp"

#include <algorithm>
#include <deque>

namespace XenUI {
    namespace {
        struct ReactiveContext {
            Observer* Current    = nullptr;
            int BatchDepth       = 0;
            int NotifyDepth      = 0;
            bool FlushingEffects = false;
            std::deque<Effect*> PendingEffects;
        };

        thread_local ReactiveContext Context;

        void FlushPendingEffects() {
            if (Context.FlushingEffects || Context.BatchDepth > 0 || Context.NotifyDepth > 0) { return; }

            Context.FlushingEffects = true;
            while (!Context.PendingEffects.empty()) {
                Effect* Next = Context.PendingEffects.front();
                Context.PendingEffects.pop_front();
                Next->Run();
            }
            Context.FlushingEffects = false;
        }
    }  // namespace

    Observer::~Observer() {
        ClearDependencies();
    }

    void Observer::ClearDependencies() {
        for (SignalSource* Source : _Dependencies) {
            std::erase(Source->_Observers, this);
        }
        _Dependencies.clear();
    }

    SignalSource::~SignalSource() {
        for (Observer* Dependent : _Observers) {
            std::erase(Dependent->_Dependencies, this);
        }
    }

    void SignalSource::Track() const {
        Observer* Current = Context.Current;
        if (Current == nullptr) { return; }

        auto& Dependencies = Current->_Dependencies;
        if (std::find(Dependencies.begin(), Dependencies.end(), this) != Dependencies.end()) { return; }

        Dependencies.push_back(const_cast<SignalSource*>(this));
        _Observers.push_back(Current);
    }

    void SignalSource::Notify() {
        ++Context.NotifyDepth;
        const std::vector<Observer*> Snapshot = _Observers;
        for (Observer* Dependent : Snapshot) {
            Dependent->OnDependencyChanged();
        }
        --Context.NotifyDepth;

        FlushPendingEffects();
    }

    TrackingScope::TrackingScope(Observer& Target, bool ClearExisting) : _Previous(Context.Current) {
        if (ClearExisting) { Target.ClearDependencies(); }
        Context.Current = &Target;
    }

    TrackingScope::~TrackingScope() {
        Context.Current = _Previous;
    }

    Observer* TrackingScope::GetCurrent() {
        return Context.Current;
    }

    Effect::Effect(std::function<void()> Callback) : _Callback(std::move(Callback)) {
        Run();
    }

    Effect::~Effect() {
        if (_Scheduled) { std::erase(Context.PendingEffects, this); }
    }

    void Effect::OnDependencyChanged() {
        if (_Scheduled) { return; }
        _Scheduled = true;
        Context.PendingEffects.push_back(this);
    }

    void Effect::Run() {
        _Scheduled = false;
        TrackingScope Scope(*this);
        _Callback();
    }

    void Untracked(const std::function<void()>& Callback) {
        Observer* const Previous = Context.Current;
        Context.Current          = nullptr;
        try {
            Callback();
        } catch (...) {
            Context.Current = Previous;
            throw;
        }
        Context.Current = Previous;
    }

    void Batch(const std::function<void()>& Callback) {
        ++Context.BatchDepth;
        try {
            Callback();
        } catch (...) {
            --Context.BatchDepth;
            throw;
        }
        --Context.BatchDepth;
        FlushPendingEffects();
    }
}  // namespace XenUI
