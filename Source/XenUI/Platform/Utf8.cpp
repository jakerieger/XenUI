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

#include "XenUI/Platform/Utf8.hpp"
#include "XenUI/Platform/HResult.hpp"

#include <Windows.h>

namespace XenUI {
    std::wstring Widen(std::string_view Utf8) {
        if (Utf8.empty()) { return {}; }

        const int SourceLength = static_cast<int>(Utf8.size());
        const int Length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, Utf8.data(), SourceLength, nullptr, 0);
        if (Length == 0) { ThrowLastError("MultiByteToWideChar"); }

        std::wstring Result(static_cast<size_t>(Length), L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, Utf8.data(), SourceLength, Result.data(), Length);
        return Result;
    }

    std::string Narrow(std::wstring_view Utf16) {
        if (Utf16.empty()) { return {}; }

        const int SourceLength = static_cast<int>(Utf16.size());
        const int Length =
          WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, Utf16.data(), SourceLength, nullptr, 0, nullptr, nullptr);
        if (Length == 0) { ThrowLastError("WideCharToMultiByte"); }

        std::string Result(static_cast<size_t>(Length), '\0');
        WideCharToMultiByte(CP_UTF8,
                            WC_ERR_INVALID_CHARS,
                            Utf16.data(),
                            SourceLength,
                            Result.data(),
                            Length,
                            nullptr,
                            nullptr);
        return Result;
    }
}  // namespace XenUI
