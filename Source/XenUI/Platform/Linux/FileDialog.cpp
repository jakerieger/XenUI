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

#include "XenUI/Platform/FileDialog.hpp"
#include "XenUI/AppContext.hpp"
#include "XenUI/Platform/Linux/SdlPlatform.hpp"
#include "XenUI/Platform/Window.hpp"

#include <mutex>
#include <string>
#include <vector>

// SDL shows the desktop's own dialogs (the xdg file chooser portal, or zenity) and reports the result through a
// callback, possibly from another thread. FileDialog's calls block like the Windows ones, so each runs a modal loop
// that keeps windows painting and posted work running while it ignores input to the application's windows.

namespace XenUI {
    namespace {
        struct DialogResult {
            std::mutex Mutex;
            bool Done = false;
            std::vector<std::filesystem::path> Files;
        };

        void SDLCALL OnDialogClosed(void* UserData, const char* const* Files, int Filter) {
            (void)Filter;
            auto* Result = static_cast<DialogResult*>(UserData);
            {
                std::lock_guard Lock(Result->Mutex);
                for (const char* const* File = Files; File != nullptr && *File != nullptr; ++File) {
                    Result->Files.emplace_back(*File);
                }
                Result->Done = true;
            }
            Sdl::Wake();
        }

        /// @brief SDL's filter pattern for Windows-style wildcards: "*.wav;*.flac" becomes "wav;flac".
        std::string ToSdlPattern(std::string_view Patterns) {
            std::string Result;
            size_t Start = 0;
            while (Start <= Patterns.size()) {
                const size_t End = std::min(Patterns.find(';', Start), Patterns.size());
                std::string_view Pattern = Patterns.substr(Start, End - Start);
                while (!Pattern.empty() && Pattern.front() == ' ') { Pattern.remove_prefix(1); }
                if (Pattern == "*" || Pattern == "*.*") { return "*"; }
                if (Pattern.starts_with("*.")) { Pattern.remove_prefix(2); }
                if (!Pattern.empty()) {
                    if (!Result.empty()) { Result.push_back(';'); }
                    Result.append(Pattern);
                }
                Start = End + 1;
            }
            return Result.empty() ? "*" : Result;
        }

        std::vector<std::filesystem::path>
        RunDialog(SDL_FileDialogType Type, const FileDialogOptions& Options, bool Many) {
            Sdl::EnsureVideo();

            std::vector<std::string> Names;
            std::vector<std::string> Patterns;
            for (const FileFilter& Filter : Options.Filters) {
                Names.push_back(Filter.Name);
                Patterns.push_back(ToSdlPattern(Filter.Patterns));
            }
            std::vector<SDL_DialogFileFilter> Filters;
            for (size_t I = 0; I < Names.size(); ++I) {
                Filters.push_back({Names[I].c_str(), Patterns[I].c_str()});
            }

            std::filesystem::path Location = Options.InitialFolder;
            if (Type == SDL_FILEDIALOG_SAVEFILE && !Options.FileName.empty()) { Location /= Options.FileName; }
            const std::string LocationText = Location.string();

            SDL_Window* Owner = Options.Owner != nullptr ? static_cast<SDL_Window*>(Options.Owner->GetNativeHandle())
                                                         : SDL_GetKeyboardFocus();

            const SDL_PropertiesID Properties = SDL_CreateProperties();
            if (!Filters.empty()) {
                SDL_SetPointerProperty(Properties, SDL_PROP_FILE_DIALOG_FILTERS_POINTER, Filters.data());
                SDL_SetNumberProperty(Properties,
                                      SDL_PROP_FILE_DIALOG_NFILTERS_NUMBER,
                                      static_cast<Sint64>(Filters.size()));
            }
            if (Owner != nullptr) { SDL_SetPointerProperty(Properties, SDL_PROP_FILE_DIALOG_WINDOW_POINTER, Owner); }
            if (!LocationText.empty()) {
                SDL_SetStringProperty(Properties, SDL_PROP_FILE_DIALOG_LOCATION_STRING, LocationText.c_str());
            }
            if (!Options.Title.empty()) {
                SDL_SetStringProperty(Properties, SDL_PROP_FILE_DIALOG_TITLE_STRING, Options.Title.c_str());
            }
            SDL_SetBooleanProperty(Properties, SDL_PROP_FILE_DIALOG_MANY_BOOLEAN, Many);

            DialogResult Result;
            SDL_ShowFileDialogWithProperties(Type, &OnDialogClosed, &Result, Properties);
            while (true) {
                {
                    std::lock_guard Lock(Result.Mutex);
                    if (Result.Done) { break; }
                }
                Sdl::PumpEvents(16, false);
                AppContext::Get().RenderReadyWindows();
            }
            SDL_DestroyProperties(Properties);

            std::lock_guard Lock(Result.Mutex);
            return Result.Files;
        }
    }  // namespace

    std::optional<std::filesystem::path> FileDialog::PickFolder(const FileDialogOptions& Options) {
        std::vector<std::filesystem::path> Chosen = RunDialog(SDL_FILEDIALOG_OPENFOLDER, Options, false);
        if (Chosen.empty()) { return std::nullopt; }
        return Chosen.front();
    }

    std::optional<std::filesystem::path> FileDialog::OpenFile(const FileDialogOptions& Options) {
        std::vector<std::filesystem::path> Chosen = RunDialog(SDL_FILEDIALOG_OPENFILE, Options, false);
        if (Chosen.empty()) { return std::nullopt; }
        return Chosen.front();
    }

    std::vector<std::filesystem::path> FileDialog::OpenFiles(const FileDialogOptions& Options) {
        return RunDialog(SDL_FILEDIALOG_OPENFILE, Options, true);
    }

    std::optional<std::filesystem::path> FileDialog::SaveFile(const FileDialogOptions& Options) {
        std::vector<std::filesystem::path> Chosen = RunDialog(SDL_FILEDIALOG_SAVEFILE, Options, false);
        if (Chosen.empty()) { return std::nullopt; }
        std::filesystem::path File = Chosen.front();
        if (!File.has_extension() && !Options.DefaultExtension.empty()) {
            std::string Extension = Options.DefaultExtension;
            if (Extension.front() != '.') { Extension.insert(Extension.begin(), '.'); }
            File += Extension;
        }
        return File;
    }
}  // namespace XenUI
