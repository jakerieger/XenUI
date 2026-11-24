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
#include "XenUI/Platform/Linux/SdlPlatform.hpp"

#include <map>
#include <memory>

// The system clipboard through SDL, with MIME types for custom formats. Before any window exists SDL's video
// subsystem isn't running (and in headless runs there may be no display at all), so the clipboard is then private to
// the process.

namespace XenUI {
    namespace {
        constexpr const char* kTextTypes[] = {"text/plain;charset=utf-8", "text/plain", "UTF8_STRING", "STRING"};

        struct LocalClipboard {
            std::optional<std::string> Text;
            std::map<std::string, std::vector<uint8_t>, std::less<>> Formats;
        };

        LocalClipboard& GetLocal() {
            static LocalClipboard Instance;
            return Instance;
        }

        /// @brief Whether the system clipboard is reachable.
        bool UseSystem() {
            return Sdl::HasVideo();
        }

        /// @brief The MIME type a custom format travels under: names already shaped like one are used as they are.
        std::string ToMimeType(std::string_view Format) {
            if (Format.find('/') != std::string_view::npos) { return std::string(Format); }
            std::string Result = "application/x-xenui-";
            for (const char Character : Format) {
                const bool Safe = (Character >= 'a' && Character <= 'z') || (Character >= 'A' && Character <= 'Z') ||
                                  (Character >= '0' && Character <= '9') || Character == '.' || Character == '-' ||
                                  Character == '_' || Character == '+';
                Result.push_back(Safe ? Character : '_');
            }
            return Result;
        }

        std::string NormalizeLineEndings(std::string Text) {
            std::string Result;
            Result.reserve(Text.size());
            for (size_t I = 0; I < Text.size(); ++I) {
                if (Text[I] == '\r') {
                    Result.push_back('\n');
                    if (I + 1 < Text.size() && Text[I + 1] == '\n') { ++I; }
                    continue;
                }
                Result.push_back(Text[I]);
            }
            return Result;
        }

        /// @brief What SetData offered, kept alive by SDL until the clipboard changes hands.
        struct Offer {
            std::string MimeType;
            std::vector<uint8_t> Data;
            std::optional<std::string> Text;
        };

        const void* SDLCALL ProvideOffer(void* UserData, const char* MimeType, size_t* Size) {
            const auto* Data = static_cast<const Offer*>(UserData);
            if (MimeType != nullptr && Data->MimeType == MimeType) {
                *Size = Data->Data.size();
                return Data->Data.data();
            }
            if (Data->Text) {
                *Size = Data->Text->size();
                return Data->Text->data();
            }
            *Size = 0;
            return nullptr;
        }

        void SDLCALL ReleaseOffer(void* UserData) {
            delete static_cast<Offer*>(UserData);
        }
    }  // namespace

    bool Clipboard::SetText(std::string_view Text) {
        if (UseSystem()) { return SDL_SetClipboardText(std::string(Text).c_str()); }
        LocalClipboard& Local = GetLocal();
        Local.Formats.clear();
        Local.Text = std::string(Text);
        return true;
    }

    std::optional<std::string> Clipboard::GetText() {
        if (!UseSystem()) { return GetLocal().Text; }
        if (!SDL_HasClipboardText()) { return std::nullopt; }
        char* Text = SDL_GetClipboardText();
        if (Text == nullptr) { return std::nullopt; }
        std::string Result = NormalizeLineEndings(Text);
        SDL_free(Text);
        return Result;
    }

    bool Clipboard::HasText() {
        return UseSystem() ? SDL_HasClipboardText() : GetLocal().Text.has_value();
    }

    bool Clipboard::SetData(std::string_view Format, std::span<const uint8_t> Data, std::optional<std::string_view> Text) {
        if (!UseSystem()) {
            LocalClipboard& Local = GetLocal();
            Local.Formats.clear();
            Local.Formats.emplace(std::string(Format), std::vector<uint8_t>(Data.begin(), Data.end()));
            Local.Text = Text ? std::optional<std::string>(std::string(*Text)) : std::nullopt;
            return true;
        }

        auto Payload      = std::make_unique<Offer>();
        Payload->MimeType = ToMimeType(Format);
        Payload->Data.assign(Data.begin(), Data.end());
        if (Text) { Payload->Text = std::string(*Text); }

        std::vector<const char*> Types {Payload->MimeType.c_str()};
        if (Text) { Types.insert(Types.end(), std::begin(kTextTypes), std::end(kTextTypes)); }
        Offer* Owned = Payload.release();
        // SDL owns the offer from here and releases it through ReleaseOffer when the clipboard changes hands.
        return SDL_SetClipboardData(&ProvideOffer, &ReleaseOffer, Owned, Types.data(), Types.size());
    }

    std::optional<std::vector<uint8_t>> Clipboard::GetData(std::string_view Format) {
        if (!UseSystem()) {
            const LocalClipboard& Local = GetLocal();
            if (const auto Found = Local.Formats.find(Format); Found != Local.Formats.end()) { return Found->second; }
            return std::nullopt;
        }

        const std::string MimeType = ToMimeType(Format);
        if (!SDL_HasClipboardData(MimeType.c_str())) { return std::nullopt; }
        size_t Size = 0;
        void* Data  = SDL_GetClipboardData(MimeType.c_str(), &Size);
        if (Data == nullptr) { return std::nullopt; }
        const auto* Bytes = static_cast<const uint8_t*>(Data);
        std::vector<uint8_t> Result(Bytes, Bytes + Size);
        SDL_free(Data);
        return Result;
    }

    bool Clipboard::HasData(std::string_view Format) {
        if (!UseSystem()) { return GetLocal().Formats.contains(Format); }
        return SDL_HasClipboardData(ToMimeType(Format).c_str());
    }
}  // namespace XenUI
