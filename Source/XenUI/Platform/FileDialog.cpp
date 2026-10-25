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
#include "XenUI/Platform/HResult.hpp"
#include "XenUI/Platform/Utf8.hpp"
#include "XenUI/Platform/Window.hpp"

#include <Windows.h>
#include <ShObjIdl.h>
#include <wrl/client.h>

namespace XenUI {
    namespace {
        using Microsoft::WRL::ComPtr;

        enum class DialogKind : uint8_t { Folder, Open, OpenMany, Save };

        std::filesystem::path GetPath(IShellItem& Item) {
            PWSTR Path = nullptr;
            if (FAILED(Item.GetDisplayName(SIGDN_FILESYSPATH, &Path))) { return {}; }
            std::filesystem::path Result(Path);
            CoTaskMemFree(Path);
            return Result;
        }

        /// @brief Shows the dialog and returns every chosen path; empty when the user cancelled.
        std::vector<std::filesystem::path> Show(DialogKind Kind, const FileDialogOptions& Options) {
            const bool Saving = Kind == DialogKind::Save;
            ComPtr<IFileDialog> Dialog;
            ThrowIfFailed(CoCreateInstance(Saving ? CLSID_FileSaveDialog : CLSID_FileOpenDialog,
                                           nullptr,
                                           CLSCTX_INPROC_SERVER,
                                           IID_PPV_ARGS(&Dialog)),
                          "CoCreateInstance(FileDialog)");

            FILEOPENDIALOGOPTIONS Flags = 0;
            ThrowIfFailed(Dialog->GetOptions(&Flags), "IFileDialog::GetOptions");
            Flags |= FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR;
            switch (Kind) {
                case DialogKind::Folder:
                    Flags |= FOS_PICKFOLDERS | FOS_PATHMUSTEXIST;
                    break;
                case DialogKind::Open:
                    Flags |= FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST;
                    break;
                case DialogKind::OpenMany:
                    Flags |= FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST | FOS_ALLOWMULTISELECT;
                    break;
                case DialogKind::Save:
                    Flags |= FOS_OVERWRITEPROMPT | FOS_PATHMUSTEXIST;
                    break;
            }
            ThrowIfFailed(Dialog->SetOptions(Flags), "IFileDialog::SetOptions");

            if (!Options.Title.empty()) { Dialog->SetTitle(Widen(Options.Title).c_str()); }
            if (!Options.FileName.empty()) { Dialog->SetFileName(Widen(Options.FileName).c_str()); }
            if (!Options.DefaultExtension.empty()) {
                Dialog->SetDefaultExtension(Widen(Options.DefaultExtension).c_str());
            }
            if (!Options.InitialFolder.empty()) {
                ComPtr<IShellItem> Folder;
                if (SUCCEEDED(
                      SHCreateItemFromParsingName(Options.InitialFolder.c_str(), nullptr, IID_PPV_ARGS(&Folder)))) {
                    Dialog->SetFolder(Folder.Get());
                }
            }

            if (Kind != DialogKind::Folder && !Options.Filters.empty()) {
                std::vector<std::wstring> Strings;
                Strings.reserve(Options.Filters.size() * 2);
                for (const FileFilter& Filter : Options.Filters) {
                    Strings.push_back(Widen(Filter.Name));
                    Strings.push_back(Widen(Filter.Patterns));
                }
                std::vector<COMDLG_FILTERSPEC> Specs;
                for (size_t i = 0; i < Strings.size(); i += 2) {
                    Specs.push_back({Strings[i].c_str(), Strings[i + 1].c_str()});
                }
                Dialog->SetFileTypes(static_cast<UINT>(Specs.size()), Specs.data());
            }

            const HWND Owner =
              Options.Owner != nullptr ? static_cast<HWND>(Options.Owner->GetNativeHandle()) : GetActiveWindow();
            const HRESULT Shown = Dialog->Show(Owner);
            if (Shown == HRESULT_FROM_WIN32(ERROR_CANCELLED)) { return {}; }
            ThrowIfFailed(Shown, "IFileDialog::Show");

            std::vector<std::filesystem::path> Chosen;
            if (Kind == DialogKind::OpenMany) {
                ComPtr<IFileOpenDialog> OpenDialog;
                ComPtr<IShellItemArray> Items;
                DWORD Count = 0;
                if (SUCCEEDED(Dialog.As(&OpenDialog)) && SUCCEEDED(OpenDialog->GetResults(&Items)) &&
                    SUCCEEDED(Items->GetCount(&Count))) {
                    for (DWORD i = 0; i < Count; ++i) {
                        ComPtr<IShellItem> Item;
                        if (SUCCEEDED(Items->GetItemAt(i, &Item))) { Chosen.push_back(GetPath(*Item.Get())); }
                    }
                }
            } else {
                ComPtr<IShellItem> Item;
                if (SUCCEEDED(Dialog->GetResult(&Item))) { Chosen.push_back(GetPath(*Item.Get())); }
            }
            std::erase_if(Chosen, [](const std::filesystem::path& Path) { return Path.empty(); });
            return Chosen;
        }

        std::optional<std::filesystem::path> ShowSingle(DialogKind Kind, const FileDialogOptions& Options) {
            std::vector<std::filesystem::path> Chosen = Show(Kind, Options);
            if (Chosen.empty()) { return std::nullopt; }
            return std::move(Chosen.front());
        }
    }  // namespace

    std::optional<std::filesystem::path> FileDialog::PickFolder(const FileDialogOptions& Options) {
        return ShowSingle(DialogKind::Folder, Options);
    }

    std::optional<std::filesystem::path> FileDialog::OpenFile(const FileDialogOptions& Options) {
        return ShowSingle(DialogKind::Open, Options);
    }

    std::vector<std::filesystem::path> FileDialog::OpenFiles(const FileDialogOptions& Options) {
        return Show(DialogKind::OpenMany, Options);
    }

    std::optional<std::filesystem::path> FileDialog::SaveFile(const FileDialogOptions& Options) {
        return ShowSingle(DialogKind::Save, Options);
    }
}  // namespace XenUI
