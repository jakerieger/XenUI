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
#include "XenUI/Core/Color.hpp"
#include "XenUI/Core/Math.hpp"
#include "XenUI/Reactive/Signal.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <vector>

namespace XenUI {
    enum class Easing : uint8_t {
        Linear,
        EaseIn,
        EaseOut,
        EaseInOut,
        /// @brief Overshoots slightly before settling, for playful motion such as toggles.
        EaseOutBack,
    };

    /// @brief Maps linear progress from 0 to 1 onto an easing curve.
    [[nodiscard]] XENUI_API float Ease(Easing Curve, float Progress);

    [[nodiscard]] inline float Interpolate(float From, float To, float Amount) {
        return From + (To - From) * Amount;
    }
    [[nodiscard]] inline double Interpolate(double From, double To, float Amount) {
        return From + (To - From) * Amount;
    }
    [[nodiscard]] inline Vec2 Interpolate(const Vec2& From, const Vec2& To, float Amount) {
        return {Interpolate(From.X, To.X, Amount), Interpolate(From.Y, To.Y, Amount)};
    }
    [[nodiscard]] inline Size Interpolate(const Size& From, const Size& To, float Amount) {
        return {Interpolate(From.Width, To.Width, Amount), Interpolate(From.Height, To.Height, Amount)};
    }
    [[nodiscard]] inline Rect Interpolate(const Rect& From, const Rect& To, float Amount) {
        return {Interpolate(From.X, To.X, Amount),
                Interpolate(From.Y, To.Y, Amount),
                Interpolate(From.Width, To.Width, Amount),
                Interpolate(From.Height, To.Height, Amount)};
    }
    [[nodiscard]] inline Color Interpolate(const Color& From, const Color& To, float Amount) {
        return {Interpolate(From.R, To.R, Amount),
                Interpolate(From.G, To.G, Amount),
                Interpolate(From.B, To.B, Amount),
                Interpolate(From.A, To.A, Amount)};
    }

    /// @brief Something advanced by the AnimationTicker while it runs.
    class XENUI_API AnimationDriver {
    public:
        AnimationDriver() = default;
        virtual ~AnimationDriver();

        AnimationDriver(const AnimationDriver&)            = delete;
        AnimationDriver& operator=(const AnimationDriver&) = delete;

        /// @brief Moves to the state at Now. Returns false once finished.
        virtual bool Advance(double Now) = 0;

    protected:
        void StartTicking();
        void StopTicking();
        [[nodiscard]] bool IsTicking() const { return _Ticking; }

    private:
        friend class AnimationTicker;

        bool _Ticking = false;
    };

    /// @brief Advances running animations once per pass of the run loop, before frames are rendered. UI thread only.
    class XENUI_API AnimationTicker {
    public:
        [[nodiscard]] static AnimationTicker& Get();

        /// @brief Seconds on the TimerQueue clock, so tests can drive animations with a fake time source.
        [[nodiscard]] static double Now();

        /// @brief Whether animations play, following the Windows "Animation effects" setting unless overridden.
        [[nodiscard]] static bool AreAnimationsEnabled();
        /// @brief Forces animations on or off, or restores the system setting with nothing.
        static void SetAnimationsEnabled(std::optional<bool> Override);
        /// @internal Re-reads the system setting after it changed.
        static void RefreshSystemSetting();

        void Tick();
        [[nodiscard]] bool HasActive() const { return !_Active.empty(); }

        /// @internal
        void Add(AnimationDriver& Driver);
        /// @internal
        void Remove(AnimationDriver& Driver);

    private:
        std::vector<AnimationDriver*> _Active;
    };

    /// @brief A value that moves smoothly to new targets. Reads through Get are tracked like a Signal, so widgets
    /// and render objects that read it update on every frame of the animation. When animations are turned off in
    /// Windows, values jump straight to their targets.
    template<class T>
    class Animated final : public AnimationDriver {
    public:
        explicit Animated(T Initial = {}) : _Value(Initial), _From(Initial), _Target(Initial) {}
        ~Animated() override { StopTicking(); }

        [[nodiscard]] const T& Get() const { return _Value.Get(); }
        [[nodiscard]] const T& operator()() const { return _Value.Get(); }
        [[nodiscard]] const T& Peek() const { return _Value.Peek(); }
        [[nodiscard]] const T& GetTarget() const { return _Target; }
        [[nodiscard]] bool IsAnimating() const { return IsTicking(); }

        /// @brief Animates from the current value to Target. Asking for the target already in progress does not
        /// restart the animation.
        void AnimateTo(const T& Target, double Duration, Easing Curve = Easing::EaseOut) {
            if (IsTicking() && Target == _Target) { return; }
            if (Duration <= 0.0 || !AnimationTicker::AreAnimationsEnabled()) {
                Set(Target);
                return;
            }
            _From     = _Value.Peek();
            _Target   = Target;
            _Start    = AnimationTicker::Now();
            _Duration = Duration;
            _Curve    = Curve;
            if (_From == _Target) {
                StopTicking();
                return;
            }
            StartTicking();
        }

        /// @brief Jumps to Value and stops any animation.
        void Set(const T& Value) {
            StopTicking();
            _From   = Value;
            _Target = Value;
            _Value  = Value;
        }

        bool Advance(double Now) override {
            const float Progress = static_cast<float>(std::clamp((Now - _Start) / _Duration, 0.0, 1.0));
            _Value               = Progress >= 1.0f ? _Target : Interpolate(_From, _Target, Ease(_Curve, Progress));
            return Progress < 1.0f;
        }

    private:
        Signal<T> _Value;
        T _From;
        T _Target;
        double _Start    = 0.0;
        double _Duration = 0.0;
        Easing _Curve    = Easing::EaseOut;
    };
}  // namespace XenUI
