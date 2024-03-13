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
using wstr = std::wstring;
using str  = std::string;

template<class T>
using Option = std::optional<T>;

template<class T, class D = std::default_delete<T>>
using Unique = std::unique_ptr<T, D>;

// Not sure about this one, might remove it
#define MakeUnique std::make_unique