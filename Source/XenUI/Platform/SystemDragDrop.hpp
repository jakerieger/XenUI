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

#include "XenUI/Input/DragDrop.hpp"

#include <Windows.h>

#include <memory>
#include <optional>

namespace XenUI {
    class WindowImpl;

    /// @brief Registers a window with OLE so drags from other applications, and from XenUI windows through the
    /// system, reach its drop targets. Revokes the registration when destroyed.
    class SystemDropRegistration {
    public:
        explicit SystemDropRegistration(WindowImpl& Window);
        ~SystemDropRegistration();

        SystemDropRegistration(const SystemDropRegistration&)            = delete;
        SystemDropRegistration& operator=(const SystemDropRegistration&) = delete;

    private:
        struct Impl;
        std::unique_ptr<Impl> _Impl;
    };

    /// @brief Runs an OLE drag of Data until it is dropped or cancelled, offering Files as a file list and Text as
    /// plain text. Keeps rendering windows while the system's modal drag loop runs.
    [[nodiscard]] DropEffect RunSystemDrag(const DragData& Data, DropEffect Allowed);

    /// @brief True while RunSystemDrag is in progress.
    [[nodiscard]] bool IsSystemDragActive();
}  // namespace XenUI
