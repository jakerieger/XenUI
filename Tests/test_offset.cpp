// Copyright (C) 2024 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

// Author: Jake Rieger
// Created: 3/21/2024.
//

#define CATCH_CONFIG_MAIN

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <XenUI.h>

using namespace Xen;

TEST_CASE("Zero", "[Offset]") {
    auto zero = Offset(0, 0);
    REQUIRE(zero.X == 0);
    REQUIRE(zero.Y == 0);
}

TEST_CASE("Infinite", "[Offset]") {
    auto inf = Offset(Infinity<f32>(), Infinity<f32>());
    REQUIRE(inf.X == Infinity<f32>());
    REQUIRE(inf.Y == Infinity<f32>());
}

TEST_CASE("FromDirection", "[Offset]") {
    f64 direction = 2;
    f64 distance  = 10;
    f64 expectedX = distance * std::cos(direction);
    f64 expectedY = distance * std::sin(direction);
    auto offset   = Offset::FromDirection(direction, distance);
    REQUIRE(offset.X == Catch::Approx(expectedX).epsilon(0.0001));
    REQUIRE(offset.Y == Catch::Approx(expectedY).epsilon(0.0001));
}

TEST_CASE("Lerp", "[Offset]") {
    auto offsetA   = Offset(10, 5);
    auto offsetB   = Offset(20, 10);
    auto result    = Offset::Lerp(offsetA, offsetB, 0.5);
    auto expectedX = Math::Lerp(offsetA.X, offsetB.X, 0.5);
    auto expectedY = Math::Lerp(offsetA.Y, offsetB.Y, 0.5);
    auto expected  = Offset(expectedX, expectedY);

    REQUIRE(result == expected);
}

TEST_CASE("Direction", "[Offset]") {
    auto offset   = Offset(10, 5);
    auto expected = std::atan2(10, 5);
    REQUIRE(offset.Direction() == expected);
}

TEST_CASE("Distance", "[Offset]") {
    auto offset   = Offset(10, 5);
    auto expected = std::sqrt((10 * 10) + (5 * 5));
    REQUIRE(offset.Distance() == Catch::Approx(expected).epsilon(0.0001));
}

TEST_CASE("DistanceSqr", "[Offset]") {
    auto offset   = Offset(10, 5);
    auto expected = (10 * 10) + (5 * 5);
    REQUIRE(offset.DistanceSqr() == expected);
}

TEST_CASE("IsFinite", "[Offset]") {
    auto offset = Offset(10, 5);
    REQUIRE(offset.IsFinite());
    REQUIRE(!offset.IsInfinite());
}

TEST_CASE("IsInfinite", "[Offset]") {
    auto offset = Offset(Infinity<f32>(), Infinity<f32>());
    REQUIRE(!offset.IsFinite());
    REQUIRE(offset.IsInfinite());
}

TEST_CASE("Scale", "[Offset]") {
    auto offset = Offset(10, 5);
    auto scaled = offset.Scale(2, 2);
    REQUIRE(scaled.X == 20);
    REQUIRE(scaled.Y == 10);
}

TEST_CASE("Translate", "[Offset]") {
    auto offset     = Offset(10, 5);
    auto translated = offset.Translate(2, 2);
    REQUIRE(translated.X == 12);
    REQUIRE(translated.Y == 7);
}

TEST_CASE("Negate", "[Offset]") {
    auto offset  = Offset(10, 5);
    auto negated = -offset;
    REQUIRE(negated.X == -10);
    REQUIRE(negated.Y == -5);
}
