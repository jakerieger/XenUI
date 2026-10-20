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

#include "TestWidgets.hpp"

#include <doctest/doctest.h>

#include <format>
#include <string>

using namespace XenUI;
using namespace XenUI::Testing;

namespace {
    struct ManualClock {
        ManualClock() {
            TimerQueue::Get().SetTimeSource([this] { return Now; });
            AnimationTicker::SetAnimationsEnabled(true);
        }

        ~ManualClock() {
            TimerQueue::Get().SetTimeSource({});
            AnimationTicker::SetAnimationsEnabled(std::nullopt);
        }

        void Advance(double Seconds) {
            Now += Seconds;
            AnimationTicker::Get().Tick();
        }

        double Now = 100.0;
    };

    struct AnimatedLabel {
        Animated<float>* Value = nullptr;

        Node Build(BuildContext& Context) const {
            (void)Context;
            return Text {.Content = std::format("{:.1f}", Value->Get())};
        }
    };
}  // namespace

TEST_CASE("Easing curves start at 0, end at 1 and shape the middle") {
    for (const Easing Curve :
         {Easing::Linear, Easing::EaseIn, Easing::EaseOut, Easing::EaseInOut, Easing::EaseOutBack}) {
        CHECK(Ease(Curve, 0.0f) == doctest::Approx(0.0f));
        CHECK(Ease(Curve, 1.0f) == doctest::Approx(1.0f));
    }
    CHECK(Ease(Easing::Linear, 0.25f) == doctest::Approx(0.25f));
    CHECK(Ease(Easing::EaseIn, 0.5f) < 0.5f);
    CHECK(Ease(Easing::EaseOut, 0.5f) > 0.5f);
    CHECK(Ease(Easing::EaseInOut, 0.5f) == doctest::Approx(0.5f));
    CHECK(Ease(Easing::EaseOutBack, 0.7f) > 1.0f);
    CHECK(Ease(Easing::EaseOut, 2.0f) == doctest::Approx(1.0f));
}

TEST_CASE("Animated values move to their targets over time and can be retargeted") {
    ManualClock Clock;
    Animated<float> Value(0.0f);

    Value.AnimateTo(10.0f, 1.0, Easing::Linear);
    CHECK(Value.IsAnimating());
    CHECK(AnimationTicker::Get().HasActive());
    Clock.Advance(0.5);
    CHECK(Value.Peek() == doctest::Approx(5.0f));

    Value.AnimateTo(10.0f, 1.0, Easing::Linear);
    Clock.Advance(0.25);
    CHECK(Value.Peek() == doctest::Approx(7.5f));

    Value.AnimateTo(0.0f, 1.0, Easing::Linear);
    Clock.Advance(0.5);
    CHECK(Value.Peek() == doctest::Approx(3.75f));
    Clock.Advance(0.6);
    CHECK(Value.Peek() == doctest::Approx(0.0f));
    CHECK_FALSE(Value.IsAnimating());
    CHECK_FALSE(AnimationTicker::Get().HasActive());

    Animated<Color> Tint(Color::FromHex(0x000000));
    Tint.AnimateTo(Color::FromHex(0xFFFFFF), 1.0, Easing::Linear);
    Clock.Advance(0.5);
    CHECK(Tint.Peek().R == doctest::Approx(0.5f));
}

TEST_CASE("Animations jump to their targets when the system turns animations off") {
    ManualClock Clock;
    AnimationTicker::SetAnimationsEnabled(false);
    Animated<float> Value(0.0f);
    Value.AnimateTo(4.0f, 1.0);
    CHECK(Value.Peek() == doctest::Approx(4.0f));
    CHECK_FALSE(Value.IsAnimating());
}

TEST_CASE("Widgets that read an animated value rebuild on every tick") {
    (void)SharedApp();
    ManualClock Clock;
    Animated<float> Value(0.0f);
    WidgetHost Host;
    Host.SetViewportSize({200.0f, 50.0f});
    Host.SetRoot(AnimatedLabel {&Value});
    Host.Update();

    const auto Shows = [&Host](const std::string& Wanted) {
        const std::function<bool(const RenderObject&)> Search = [&](const RenderObject& Node) {
            if (const auto* Paragraph = dynamic_cast<const RenderParagraph*>(&Node);
                Paragraph != nullptr && Paragraph->GetText() == Wanted) {
                return true;
            }
            for (const RenderObject* Child : Node.GetChildren()) {
                if (Search(*Child)) { return true; }
            }
            return false;
        };
        return Search(*Host.GetRootBox()->GetParent());
    };
    CHECK(Shows("0.0"));

    Value.AnimateTo(2.0f, 1.0, Easing::Linear);
    Clock.Advance(0.5);
    Host.Update();
    CHECK(Shows("1.0"));
    Clock.Advance(1.0);
    Host.Update();
    CHECK(Shows("2.0"));
}

TEST_CASE("Light and dark themes differ and follow the requested appearance") {
    CHECK(Theme::For(Appearance::Dark).IsDark);
    CHECK_FALSE(Theme::For(Appearance::Light).IsDark);
    CHECK(Theme::Light().Background != Theme::Dark().Background);
    CHECK(Theme::FromSystem() == Theme::For(GetSystemAppearance()));
}
