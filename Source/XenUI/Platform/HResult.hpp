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

#include "XenUI/Core/Error.hpp"

#include <Windows.h>

#include <cstdint>
#include <format>
#include <string_view>

namespace XenUI {
    inline void ThrowIfFailed(HRESULT Result, std::string_view Operation) {
        if (FAILED(Result)) {
            throw Error(std::format("{} failed (HRESULT 0x{:08X})", Operation, static_cast<uint32_t>(Result)));
        }
    }

    [[noreturn]] inline void ThrowLastError(std::string_view Operation) {
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()), Operation);
        throw Error(std::format("{} failed", Operation));
    }
}  // namespace XenUI
