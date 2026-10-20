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
#include "XenUI/Platform/HResult.hpp"

#include <d2d1.h>
#include <dxgi.h>

#include <cstdint>
#include <format>
#include <string_view>

namespace XenUI {
    /// @brief Thrown when the GPU device was removed or reset and every device-dependent resource must be rebuilt.
    class DeviceLostError : public Error {
    public:
        using Error::Error;
    };

    inline void CheckDeviceResult(HRESULT Result, std::string_view Operation) {
        if (Result == DXGI_ERROR_DEVICE_REMOVED || Result == DXGI_ERROR_DEVICE_RESET ||
            Result == D2DERR_RECREATE_TARGET) {
            throw DeviceLostError(
              std::format("{} reported device loss (HRESULT 0x{:08X})", Operation, static_cast<uint32_t>(Result)));
        }
        ThrowIfFailed(Result, Operation);
    }
}  // namespace XenUI
