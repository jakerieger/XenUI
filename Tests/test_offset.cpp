//
// Created by conta on 3/14/2024.
//
#define CATCH_CONFIG_MAIN

#include <catch2/catch_test_macros.hpp>
#include <UI/Offset.h>
#include <Types.h>

using namespace Xen;

TEST_CASE("Zero", "[Offset]") {
    auto zero = Offset<i64>::Zero;
    REQUIRE(zero.X == 0);
    REQUIRE(zero.Y == 0);
}

TEST_CASE("Infinite", "[Offset]") {
    auto inf = Offset<i64>::Infinite;
    REQUIRE(inf.X == Infinity<i64>());
    REQUIRE(inf.Y == Infinity<i64>());
}

TEST_CASE("FromDirection", "[Offset]") {
    double direction = 2;
    double distance  = 10;
    double expectedX = distance * std::cos(direction);
    double expectedY = distance * std::sin(direction);
    auto offset      = Offset<double>::FromDirection(direction, distance);
    REQUIRE(offset.X == expectedX);
    REQUIRE(offset.Y == expectedY);
}

TEST_CASE("Lerp", "[Offset]") {
    auto offsetA   = Offset<i64>(10, 5);
    auto offsetB   = Offset<i64>(20, 10);
    auto result    = Offset<i64>::Lerp(offsetA, offsetB, 0.5);
    auto expectedX = Lerp(offsetA.X, offsetB.X, 0.5);
    auto expectedY = Lerp(offsetA.Y, offsetB.Y, 0.5);
    auto expected  = Offset(expectedX, expectedY);

    REQUIRE(result == expected);
}

TEST_CASE("Direction", "[Offset]") {
    auto offset   = Offset<i64>(10, 5);
    auto expected = std::atan2(10, 5);
    REQUIRE(offset.Direction() == expected);
}

TEST_CASE("Distance", "[Offset]") {
    auto offset   = Offset<i64>(10, 5);
    auto expected = std::sqrt((10 * 10) + (5 * 5));
    REQUIRE(offset.Distance() == expected);
}

TEST_CASE("DistanceSqr", "[Offset]") {
    auto offset   = Offset<i64>(10, 5);
    auto expected = (10 * 10) + (5 * 5);
    REQUIRE(offset.DistanceSqr() == expected);
}

TEST_CASE("IsFinite", "[Offset]") {
    auto offset = Offset<i64>(10, 5);
    REQUIRE(offset.IsFinite());
    REQUIRE(!offset.IsInfinite());
}

TEST_CASE("IsInfinite", "[Offset]") {
    auto offset = Offset<i64>::Infinite;
    REQUIRE(!offset.IsFinite());
    REQUIRE(offset.IsInfinite());
}

TEST_CASE("Scale", "[Offset]") {
    auto offset = Offset<i64>(10, 5);
    auto scaled = offset.Scale(2, 2);
    REQUIRE(scaled.X == 20);
    REQUIRE(scaled.Y == 10);
}

TEST_CASE("Translate", "[Offset]") {
    auto offset     = Offset<i64>(10, 5);
    auto translated = offset.Translate(2, 2);
    REQUIRE(translated.X == 12);
    REQUIRE(translated.Y == 7);
}

TEST_CASE("To", "[Offset]") {
    auto offset = Offset<i64>(10, 5).To<double>();
    REQUIRE(std::is_same_v<decltype(offset.X), double>);
    REQUIRE(std::is_same_v<decltype(offset.Y), double>);
}

TEST_CASE("Negate", "[Offset]") {
    auto offset  = Offset<i64>(10, 5);
    auto negated = -offset;
    REQUIRE(negated.X == -10);
    REQUIRE(negated.Y == -5);
}
