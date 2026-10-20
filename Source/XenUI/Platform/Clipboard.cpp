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

#include "XenUI/Platform/Clipboard.hpp"
#include "XenUI/Platform/Utf8.hpp"

#include <Windows.h>

#include <cstring>

namespace XenUI {
    namespace {
        constexpr int kOpenAttempts = 10;

        HWND GetOwnerWindow() {
            static HWND Owner = [] {
                WNDCLASSEXW Class {.cbSize        = sizeof(WNDCLASSEXW),
                                   .lpfnWndProc   = DefWindowProcW,
                                   .hInstance     = GetModuleHandleW(nullptr),
                                   .lpszClassName = L"XenUI.Clipboard"};
                RegisterClassExW(&Class);
                return CreateWindowExW(0,
                                       Class.lpszClassName,
                                       L"",
                                       0,
                                       0,
                                       0,
                                       0,
                                       0,
                                       HWND_MESSAGE,
                                       nullptr,
                                       Class.hInstance,
                                       nullptr);
            }();
            return Owner;
        }

        /// @brief Holds the clipboard open for its lifetime. Other applications may have it open briefly, so
        /// opening is retried for a few milliseconds.
        class ClipboardSession {
        public:
            ClipboardSession() {
                for (int Attempt = 0; Attempt < kOpenAttempts && !_Open; ++Attempt) {
                    _Open = OpenClipboard(GetOwnerWindow()) != FALSE;
                    if (!_Open) { Sleep(2); }
                }
            }

            ~ClipboardSession() {
                if (_Open) { CloseClipboard(); }
            }

            ClipboardSession(const ClipboardSession&)            = delete;
            ClipboardSession& operator=(const ClipboardSession&) = delete;

            [[nodiscard]] bool IsOpen() const { return _Open; }

        private:
            bool _Open = false;
        };

        UINT RegisterFormat(std::string_view Format) {
            return RegisterClipboardFormatW(Widen(Format).c_str());
        }

        bool Put(UINT Format, const void* Data, size_t Bytes) {
            HGLOBAL Memory = GlobalAlloc(GMEM_MOVEABLE, Bytes);
            if (Memory == nullptr) { return false; }
            void* Target = GlobalLock(Memory);
            if (Target == nullptr) {
                GlobalFree(Memory);
                return false;
            }
            std::memcpy(Target, Data, Bytes);
            GlobalUnlock(Memory);
            if (SetClipboardData(Format, Memory) == nullptr) {
                GlobalFree(Memory);
                return false;
            }
            return true;
        }

        bool PutText(std::string_view Text) {
            std::wstring Wide;
            for (const wchar_t Unit : Widen(Text)) {
                if (Unit == L'\n') { Wide.push_back(L'\r'); }
                if (Unit != L'\r') { Wide.push_back(Unit); }
            }
            return Put(CF_UNICODETEXT, Wide.c_str(), (Wide.size() + 1) * sizeof(wchar_t));
        }
    }  // namespace

    bool Clipboard::SetText(std::string_view Text) {
        const ClipboardSession Session;
        return Session.IsOpen() && EmptyClipboard() != FALSE && PutText(Text);
    }

    std::optional<std::string> Clipboard::GetText() {
        const ClipboardSession Session;
        if (!Session.IsOpen()) { return std::nullopt; }
        HANDLE Memory = GetClipboardData(CF_UNICODETEXT);
        if (Memory == nullptr) { return std::nullopt; }
        const auto* Wide = static_cast<const wchar_t*>(GlobalLock(Memory));
        if (Wide == nullptr) { return std::nullopt; }
        const size_t Limit = GlobalSize(Memory) / sizeof(wchar_t);
        std::wstring Text;
        for (size_t i = 0; i < Limit && Wide[i] != L'\0'; ++i) {
            if (Wide[i] != L'\r') { Text.push_back(Wide[i]); }
        }
        GlobalUnlock(Memory);
        return Narrow(Text);
    }

    bool Clipboard::HasText() {
        return IsClipboardFormatAvailable(CF_UNICODETEXT) != FALSE;
    }

    bool
    Clipboard::SetData(std::string_view Format, std::span<const uint8_t> Data, std::optional<std::string_view> Text) {
        const UINT Registered = RegisterFormat(Format);
        if (Registered == 0) { return false; }
        const ClipboardSession Session;
        if (!Session.IsOpen() || EmptyClipboard() == FALSE) { return false; }
        const uint64_t Length = Data.size();
        std::vector<uint8_t> Payload(sizeof(Length) + Data.size());
        std::memcpy(Payload.data(), &Length, sizeof(Length));
        if (!Data.empty()) { std::memcpy(Payload.data() + sizeof(Length), Data.data(), Data.size()); }
        const bool Stored = Put(Registered, Payload.data(), Payload.size());
        return Stored && (!Text || PutText(*Text));
    }

    std::optional<std::vector<uint8_t>> Clipboard::GetData(std::string_view Format) {
        const UINT Registered = RegisterFormat(Format);
        if (Registered == 0) { return std::nullopt; }
        const ClipboardSession Session;
        if (!Session.IsOpen()) { return std::nullopt; }
        HANDLE Memory = GetClipboardData(Registered);
        if (Memory == nullptr) { return std::nullopt; }
        const auto* Bytes = static_cast<const uint8_t*>(GlobalLock(Memory));
        if (Bytes == nullptr) { return std::nullopt; }
        const size_t Available = GlobalSize(Memory);
        std::optional<std::vector<uint8_t>> Result;
        uint64_t Length = 0;
        if (Available >= sizeof(Length)) {
            std::memcpy(&Length, Bytes, sizeof(Length));
            if (Length <= Available - sizeof(Length)) {
                Result.emplace(Bytes + sizeof(Length), Bytes + sizeof(Length) + Length);
            }
        }
        GlobalUnlock(Memory);
        return Result;
    }

    bool Clipboard::HasData(std::string_view Format) {
        const UINT Registered = RegisterFormat(Format);
        return Registered != 0 && IsClipboardFormatAvailable(Registered) != FALSE;
    }
}  // namespace XenUI
