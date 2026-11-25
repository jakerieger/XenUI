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

#include <SDL3/SDL.h>

#include <cstdint>

// Process-wide SDL state for the Linux platform: subsystem start-up, the GL context every window shares, the event
// that wakes the loop for posted work, and event dispatch to windows.

namespace XenUI {
    class WindowImpl;

    namespace Sdl {
        /// @brief Starts SDL's event subsystem. Video starts later, with the first window, so an App can exist
        /// (and run posted work) where no display is available, such as in tests.
        void Startup();
        /// @brief Destroys the GL context and shuts SDL down.
        void Shutdown();

        /// @brief Starts the video subsystem if it isn't running. Throws Error when no display can be opened.
        void EnsureVideo();
        [[nodiscard]] bool HasVideo();

        /// @brief Whether the desktop draws title bars for windows that don't draw their own: always on X11, and on
        /// Wayland when the compositor offers server-side decorations (not GNOME). Requires the video subsystem.
        [[nodiscard]] bool ServerDrawsTitleBars();

        /// @brief The GL context every window shares, created the first time with Window, which must have been
        /// created with SDL_WINDOW_OPENGL. Also initializes the graphics device's GL side.
        SDL_GLContext GetGLContext(SDL_Window* Window);

        /// @brief The user event type that wakes the loop to run posted work.
        [[nodiscard]] uint32_t GetWakeEvent();
        void Wake();

        /// @brief Records Owner as the WindowImpl that events for Window go to.
        void Attach(SDL_Window* Window, WindowImpl* Owner);
        /// @brief The WindowImpl that owns Window, or nullptr.
        [[nodiscard]] WindowImpl* FindWindow(SDL_Window* Window);

        /// @brief Waits up to TimeoutMs (negative waits indefinitely) for an event, then handles every queued event.
        /// Input is dropped when AllowInput is false, for modal loops.
        void PumpEvents(int32_t TimeoutMs, bool AllowInput);
    }  // namespace Sdl

    /// @internal Handles queued platform events and posted work without waiting, as the run loop would. For tests,
    /// which drive the loop themselves.
    XENUI_API void PumpPlatformEvents();
}  // namespace XenUI
