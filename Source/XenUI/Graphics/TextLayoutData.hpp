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

#include "XenUI/Core/Math.hpp"

#include <dwrite.h>
#include <wrl/client.h>

#include <cstdint>
#include <vector>

namespace XenUI {
    class TextLayoutData {
    public:
        Microsoft::WRL::ComPtr<IDWriteTextLayout> Layout;
        Size BoxSize;
        Size MeasuredSize;
        Rect InkBounds;
        float Baseline     = 0.0f;
        uint32_t LineCount = 0;
        /// @brief UTF-8 byte offset of every UTF-16 position, plus one entry for the end of the text.
        std::vector<uint32_t> Utf8Offsets;

        [[nodiscard]] uint32_t ToUtf16(size_t Utf8Offset) const;
        [[nodiscard]] size_t ToUtf8(uint32_t Utf16Offset) const;
    };
}  // namespace XenUI
