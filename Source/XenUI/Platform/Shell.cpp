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

#include "XenUI/Platform/Shell.hpp"
#include "XenUI/Platform/Utf8.hpp"

#include <Windows.h>
#include <shellapi.h>
#include <ShlObj.h>

namespace XenUI {
    bool Shell::Open(std::string_view Target) {
        if (Target.empty()) { return false; }
        const std::wstring Wide = Widen(Target);
        const auto Result =
          reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", Wide.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
        return Result > 32;
    }

    bool Shell::Reveal(const std::filesystem::path& Item) {
        PIDLIST_ABSOLUTE List = ILCreateFromPathW(Item.c_str());
        if (List == nullptr) { return false; }
        const HRESULT Result = SHOpenFolderAndSelectItems(List, 0, nullptr, 0);
        ILFree(List);
        return SUCCEEDED(Result);
    }
}  // namespace XenUI
