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

#include "XenUI/Core/Animation.hpp"
#include "XenUI/Core/Timer.hpp"
#include "XenUI/Reactive/Reactive.hpp"

#include <Windows.h>

#include <cmath>
#include <numbers>

namespace XenUI {
    namespace {
        std::optional<bool> Override;
        std::optional<bool> SystemSetting;

        bool ReadSystemSetting() {
            BOOL Enabled = TRUE;
            if (SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &Enabled, 0) == FALSE) { return true; }
            return Enabled != FALSE;
        }
    }  // namespace

    float Ease(Easing Curve, float Progress) {
        const float T = std::clamp(Progress, 0.0f, 1.0f);
        switch (Curve) {
            case Easing::Linear:
                return T;
            case Easing::EaseIn:
                return T * T * T;
            case Easing::EaseOut: {
                const float Inverse = 1.0f - T;
                return 1.0f - Inverse * Inverse * Inverse;
            }
            case Easing::EaseInOut:
                return T < 0.5f ? 4.0f * T * T * T : 1.0f - std::pow(-2.0f * T + 2.0f, 3.0f) * 0.5f;
            case Easing::EaseOutBack: {
                constexpr float kOvershoot = 1.70158f;
                const float Shifted        = T - 1.0f;
                return 1.0f + (kOvershoot + 1.0f) * Shifted * Shifted * Shifted + kOvershoot * Shifted * Shifted;
            }
        }
        return T;
    }

    AnimationDriver::~AnimationDriver() {
        StopTicking();
    }

    void AnimationDriver::StartTicking() {
        if (_Ticking) { return; }
        _Ticking = true;
        AnimationTicker::Get().Add(*this);
    }

    void AnimationDriver::StopTicking() {
        if (!_Ticking) { return; }
        _Ticking = false;
        AnimationTicker::Get().Remove(*this);
    }

    AnimationTicker& AnimationTicker::Get() {
        static AnimationTicker Instance;
        return Instance;
    }

    double AnimationTicker::Now() {
        return TimerQueue::Get().Now();
    }

    bool AnimationTicker::AreAnimationsEnabled() {
        if (Override) { return *Override; }
        if (!SystemSetting) { SystemSetting = ReadSystemSetting(); }
        return *SystemSetting;
    }

    void AnimationTicker::SetAnimationsEnabled(std::optional<bool> Enabled) {
        Override = Enabled;
    }

    void AnimationTicker::RefreshSystemSetting() {
        SystemSetting = ReadSystemSetting();
    }

    void AnimationTicker::Add(AnimationDriver& Driver) {
        _Active.push_back(&Driver);
    }

    void AnimationTicker::Remove(AnimationDriver& Driver) {
        std::erase(_Active, &Driver);
    }

    void AnimationTicker::Tick() {
        if (_Active.empty()) { return; }
        const double Time                           = Now();
        const std::vector<AnimationDriver*> Running = _Active;
        Batch([&] {
            for (AnimationDriver* Driver : Running) {
                if (std::find(_Active.begin(), _Active.end(), Driver) == _Active.end()) { continue; }
                if (!Driver->Advance(Time)) {
                    std::erase(_Active, Driver);
                    Driver->_Ticking = false;
                }
            }
        });
    }
}  // namespace XenUI
