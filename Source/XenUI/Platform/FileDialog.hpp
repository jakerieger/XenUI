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

#include "XenUI/Api.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace XenUI {
    class Window;

    /// @brief One entry of a file dialog's type list, such as {"Audio files", "*.wav;*.flac"}.
    struct FileFilter {
        std::string Name;
        /// @brief Semicolon-separated wildcard patterns.
        std::string Patterns;
    };

    struct FileDialogOptions {
        /// @brief The dialog's title; the system's default ("Open", "Save As", ...) when empty.
        std::string Title;
        /// @brief The folder the dialog starts in. When empty, the system picks the last folder used.
        std::filesystem::path InitialFolder;
        /// @brief The file name the save dialog starts with.
        std::string FileName;
        std::vector<FileFilter> Filters;
        /// @brief Extension added to a saved file name the user typed without one, such as "wav".
        std::string DefaultExtension;
        /// @brief The window the dialog belongs to; it is disabled while the dialog is open. When null, the active
        /// window of the calling thread, which is the right one when a dialog is opened from a widget's callback.
        const Window* Owner = nullptr;
    };

    /// @brief The system's open, save and folder dialogs. Each call blocks until the user closes the dialog, while
    /// the application's windows keep painting and posted work keeps running. UI thread only.
    class XENUI_API FileDialog {
    public:
        /// @return The chosen folder, or nothing if the user cancelled.
        [[nodiscard]] static std::optional<std::filesystem::path> PickFolder(const FileDialogOptions& Options = {});
        /// @return The chosen file, or nothing if the user cancelled.
        [[nodiscard]] static std::optional<std::filesystem::path> OpenFile(const FileDialogOptions& Options = {});
        /// @return The chosen files, or none if the user cancelled.
        [[nodiscard]] static std::vector<std::filesystem::path> OpenFiles(const FileDialogOptions& Options = {});
        /// @return Where to save, or nothing if the user cancelled. The dialog asks before overwriting a file.
        [[nodiscard]] static std::optional<std::filesystem::path> SaveFile(const FileDialogOptions& Options = {});
    };
}  // namespace XenUI
