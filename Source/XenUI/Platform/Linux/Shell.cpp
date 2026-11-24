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

#include <SDL3/SDL.h>

#include <fcntl.h>
#include <spawn.h>
#include <unistd.h>
#include <sys/wait.h>

#include <string>
#include <system_error>
#include <vector>

extern char** environ;

namespace XenUI {
    namespace {
        /// @brief A file:// URI for Path, with everything but unreserved characters and slashes percent-encoded.
        std::string ToFileUri(const std::filesystem::path& Path) {
            std::error_code Error;
            std::filesystem::path Absolute = std::filesystem::absolute(Path, Error);
            if (Error) { Absolute = Path; }

            constexpr char kHex[] = "0123456789ABCDEF";
            std::string Result    = "file://";
            for (const char Character : Absolute.string()) {
                const auto Byte = static_cast<unsigned char>(Character);
                const bool Plain = (Byte >= 'a' && Byte <= 'z') || (Byte >= 'A' && Byte <= 'Z') ||
                                   (Byte >= '0' && Byte <= '9') || Byte == '-' || Byte == '.' || Byte == '_' ||
                                   Byte == '~' || Byte == '/';
                if (Plain) {
                    Result.push_back(Character);
                } else {
                    Result.push_back('%');
                    Result.push_back(kHex[Byte >> 4]);
                    Result.push_back(kHex[Byte & 0xF]);
                }
            }
            return Result;
        }

        bool IsUrl(std::string_view Target) {
            const size_t Scheme = Target.find(':');
            if (Scheme == std::string_view::npos || Scheme < 2) { return false; }
            for (size_t I = 0; I < Scheme; ++I) {
                const char C = Target[I];
                const bool Letter = (C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z');
                const bool Valid  = Letter || (I > 0 && ((C >= '0' && C <= '9') || C == '+' || C == '-' || C == '.'));
                if (!Valid) { return false; }
            }
            return true;
        }

        /// @brief Runs Arguments (no shell) and waits for it. Returns whether it exited successfully.
        bool RunProcess(const std::vector<std::string>& Arguments) {
            std::vector<char*> Argv;
            for (const std::string& Argument : Arguments) {
                Argv.push_back(const_cast<char*>(Argument.c_str()));
            }
            Argv.push_back(nullptr);

            // Keep the tool's output out of the application's terminal.
            posix_spawn_file_actions_t Actions;
            posix_spawn_file_actions_init(&Actions);
            posix_spawn_file_actions_addopen(&Actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
            posix_spawn_file_actions_addopen(&Actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
            pid_t Child         = 0;
            const int Spawned   = posix_spawnp(&Child, Argv[0], &Actions, nullptr, Argv.data(), environ);
            posix_spawn_file_actions_destroy(&Actions);
            if (Spawned != 0) { return false; }
            int Status = 0;
            if (waitpid(Child, &Status, 0) != Child) { return false; }
            return WIFEXITED(Status) && WEXITSTATUS(Status) == 0;
        }
    }  // namespace

    bool Shell::Open(std::string_view Target) {
        if (Target.empty()) { return false; }
        const std::string Url = IsUrl(Target) ? std::string(Target) : ToFileUri(std::filesystem::path(Target));
        return SDL_OpenURL(Url.c_str());
    }

    bool Shell::Reveal(const std::filesystem::path& Item) {
        std::error_code Error;
        if (!std::filesystem::exists(Item, Error)) { return false; }

        // The freedesktop file manager interface selects the item; without it, open the containing folder.
        const bool Shown = RunProcess({"dbus-send",
                                       "--session",
                                       "--print-reply",
                                       "--dest=org.freedesktop.FileManager1",
                                       "--type=method_call",
                                       "/org/freedesktop/FileManager1",
                                       "org.freedesktop.FileManager1.ShowItems",
                                       "array:string:" + ToFileUri(Item),
                                       "string:"});
        if (Shown) { return true; }
        return SDL_OpenURL(ToFileUri(std::filesystem::absolute(Item, Error).parent_path()).c_str());
    }
}  // namespace XenUI
