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

#include "XenUI/Graphics/GL/ImageData.hpp"

#include <filesystem>
#include <optional>

namespace XenUI {
    using IconPixels = DecodedImage;

    /// @brief Loads a window icon: the largest image of a Windows .ico file (PNG or 24/32-bit BMP entries), or any
    /// image stb_image reads. Returns nothing if the file is missing or unreadable.
    [[nodiscard]] std::optional<IconPixels> LoadIconFile(const std::filesystem::path& File);
}  // namespace XenUI
