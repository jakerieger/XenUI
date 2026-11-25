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

#include "XenUI/Platform/Linux/SdlPlatform.hpp"

#include <dlfcn.h>

#include <cstdint>
#include <cstring>
#include <optional>
#include <string_view>

// Whether the compositor draws title bars for us. On Wayland that is the xdg-decoration protocol: KDE, sway and most
// others offer it, GNOME doesn't. SDL would then fall back to libdecor's GTK title bar, which stalls interactive
// resizes, so XenUI draws its own bar instead. libwayland-client is opened at run time, like SDL does, so building
// XenUI needs no Wayland development package.

namespace XenUI::Sdl {
    namespace {
        // The parts of libwayland-client's ABI used here. wl_display_get_registry and wl_registry_add_listener are
        // inline functions in its headers, written out below over the exported proxy calls.
        struct WaylandInterface {
            const char* Name;
            int Version;
            int MethodCount;
            const void* Methods;
            int EventCount;
            const void* Events;
        };

        struct RegistryListener {
            void (*Global)(void* Data, void* Registry, uint32_t Name, const char* Interface, uint32_t Version);
            void (*GlobalRemove)(void* Data, void* Registry, uint32_t Name);
        };

        constexpr uint32_t kDisplayGetRegistry = 1;

        void OnGlobal(void* Data, void*, uint32_t, const char* Interface, uint32_t) {
            if (Interface != nullptr && std::string_view(Interface) == "zxdg_decoration_manager_v1") {
                *static_cast<bool*>(Data) = true;
            }
        }

        void OnGlobalRemove(void*, void*, uint32_t) {}

        /// @brief Lists the compositor's globals on a private queue, so SDL's own event handling is untouched.
        std::optional<bool> HasDecorationManager(void* Display) {
            void* Library = dlopen("libwayland-client.so.0", RTLD_NOW | RTLD_LOCAL);
            if (Library == nullptr) { return std::nullopt; }

            using CreateQueue    = void* (*)(void*);
            using CreateWrapper  = void* (*)(void*);
            using SetQueue       = void (*)(void*, void*);
            using GetVersion     = uint32_t (*)(void*);
            using MarshalFlags   = void* (*)(void*, uint32_t, const WaylandInterface*, uint32_t, uint32_t, ...);
            using AddListener    = int (*)(void*, void (**)(void), void*);
            using RoundtripQueue = int (*)(void*, void*);
            using DestroyProxy   = void (*)(void*);
            using DestroyQueue   = void (*)(void*);

            const auto Create    = reinterpret_cast<CreateQueue>(dlsym(Library, "wl_display_create_queue"));
            const auto Wrap      = reinterpret_cast<CreateWrapper>(dlsym(Library, "wl_proxy_create_wrapper"));
            const auto Assign    = reinterpret_cast<SetQueue>(dlsym(Library, "wl_proxy_set_queue"));
            const auto Version   = reinterpret_cast<GetVersion>(dlsym(Library, "wl_proxy_get_version"));
            const auto Marshal   = reinterpret_cast<MarshalFlags>(dlsym(Library, "wl_proxy_marshal_flags"));
            const auto Listen    = reinterpret_cast<AddListener>(dlsym(Library, "wl_proxy_add_listener"));
            const auto Roundtrip = reinterpret_cast<RoundtripQueue>(dlsym(Library, "wl_display_roundtrip_queue"));
            const auto Destroy   = reinterpret_cast<DestroyProxy>(dlsym(Library, "wl_proxy_destroy"));
            const auto Unwrap    = reinterpret_cast<DestroyProxy>(dlsym(Library, "wl_proxy_wrapper_destroy"));
            const auto Release   = reinterpret_cast<DestroyQueue>(dlsym(Library, "wl_event_queue_destroy"));
            const auto* RegistryInterface =
              static_cast<const WaylandInterface*>(dlsym(Library, "wl_registry_interface"));
            if (Create == nullptr || Wrap == nullptr || Assign == nullptr || Version == nullptr || Marshal == nullptr ||
                Listen == nullptr || Roundtrip == nullptr || Destroy == nullptr || Unwrap == nullptr ||
                Release == nullptr || RegistryInterface == nullptr) {
                return std::nullopt;
            }

            void* Queue   = Create(Display);
            void* Wrapper = Wrap(Display);
            Assign(Wrapper, Queue);
            void* Registry = Marshal(Wrapper, kDisplayGetRegistry, RegistryInterface, Version(Wrapper), 0, nullptr);

            bool Found = false;
            static const RegistryListener Listener {&OnGlobal, &OnGlobalRemove};
            std::optional<bool> Result;
            if (Registry != nullptr) {
                Listen(Registry, reinterpret_cast<void (**)(void)>(const_cast<RegistryListener*>(&Listener)), &Found);
                if (Roundtrip(Display, Queue) >= 0) { Result = Found; }
                Destroy(Registry);
            }
            Unwrap(Wrapper);
            Release(Queue);
            // libwayland-client stays loaded: SDL uses it too.
            return Result;
        }
    }  // namespace

    bool ServerDrawsTitleBars() {
        static const bool Draws = [] {
            const char* Driver = SDL_GetCurrentVideoDriver();
            if (Driver == nullptr || std::strcmp(Driver, "wayland") != 0) { return true; }
            void* Display = SDL_GetPointerProperty(SDL_GetGlobalProperties(),
                                                   SDL_PROP_GLOBAL_VIDEO_WAYLAND_WL_DISPLAY_POINTER,
                                                   nullptr);
            if (Display == nullptr) { return true; }
            return HasDecorationManager(Display).value_or(true);
        }();
        return Draws;
    }
}  // namespace XenUI::Sdl
