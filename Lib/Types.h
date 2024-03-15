///
/// Type aliases for commonly used types, mostly for readability
///

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

using u8   = uint8_t;
using u16  = uint16_t;
using u32  = uint32_t;
using u64  = uint64_t;
using i8   = int8_t;
using i16  = int16_t;
using i32  = int32_t;
using i64  = int64_t;
using f32  = float;
using f64  = double;
using wstr = std::wstring;
using str  = std::string;

template<class T>
using Option = std::optional<T>;

template<class T, class D = std::default_delete<T>>
using Unique = std::unique_ptr<T, D>;

template<typename T>
struct InfinityHelper {
    static constexpr T value = std::numeric_limits<T>::max();
};

template<typename T>
constexpr T Infinity() {
    return InfinityHelper<T>::value;
}

// Not sure about this one, might remove it
#define MakeUnique std::make_unique

constexpr i64 MAX_SIZE   = 100000;
constexpr f32 MAX_SIZE_F = 100000.f;