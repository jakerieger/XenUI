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

#include <concepts>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace XenUI {
    /// @brief Identity of a widget among its siblings. Elements with matching keys are preserved, with their state,
    /// when a list is reordered. Integer and string keys are hashed into the same 64-bit space.
    class Key {
    public:
        constexpr Key() = default;

        template<std::integral T>
        constexpr Key(T Value) : _Hash(Mix(static_cast<uint64_t>(Value))), _Valid(true) {}

        constexpr Key(std::string_view Name) : _Hash(HashString(Name)), _Valid(true) {}
        constexpr Key(const char* Name) : Key(std::string_view(Name)) {}
        Key(const std::string& Name) : Key(std::string_view(Name)) {}

        [[nodiscard]] constexpr bool IsValid() const { return _Valid; }
        [[nodiscard]] constexpr uint64_t GetHash() const { return _Hash; }

        constexpr bool operator==(const Key&) const = default;

    private:
        static constexpr uint64_t Mix(uint64_t Value) {
            Value += 0x9E3779B97F4A7C15ull;
            Value = (Value ^ (Value >> 30)) * 0xBF58476D1CE4E5B9ull;
            Value = (Value ^ (Value >> 27)) * 0x94D049BB133111EBull;
            return Value ^ (Value >> 31);
        }

        static constexpr uint64_t HashString(std::string_view Name) {
            uint64_t Hash = 0xCBF29CE484222325ull;
            for (const char Character : Name) {
                Hash ^= static_cast<uint8_t>(Character);
                Hash *= 0x100000001B3ull;
            }
            return Mix(Hash);
        }

        uint64_t _Hash = 0;
        bool _Valid    = false;
    };

    struct KeyHash {
        size_t operator()(const Key& Value) const noexcept { return static_cast<size_t>(Value.GetHash()); }
    };
}  // namespace XenUI
