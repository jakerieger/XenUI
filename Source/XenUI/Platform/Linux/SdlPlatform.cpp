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
#include "XenUI/AppContext.hpp"
#include "XenUI/Core/Error.hpp"
#include "XenUI/Platform/Linux/WindowImpl.hpp"

#include <atomic>
#include <format>
#include <vector>

namespace XenUI::Sdl {
    namespace {
        constexpr const char* kWindowProperty = "XenUI.WindowImpl";

        std::atomic<uint32_t> WakeEvent {0};
        SDL_GLContext Context = nullptr;

        void* LoadGLFunction(const char* Name) {
            return reinterpret_cast<void*>(SDL_GL_GetProcAddress(Name));
        }

        bool IsInputEvent(uint32_t Type) {
            switch (Type) {
                case SDL_EVENT_KEY_DOWN:
                case SDL_EVENT_KEY_UP:
                case SDL_EVENT_TEXT_EDITING:
                case SDL_EVENT_TEXT_INPUT:
                case SDL_EVENT_MOUSE_MOTION:
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                case SDL_EVENT_MOUSE_BUTTON_UP:
                case SDL_EVENT_MOUSE_WHEEL:
                case SDL_EVENT_FINGER_DOWN:
                case SDL_EVENT_FINGER_UP:
                case SDL_EVENT_FINGER_MOTION:
                case SDL_EVENT_DROP_BEGIN:
                case SDL_EVENT_DROP_FILE:
                case SDL_EVENT_DROP_TEXT:
                case SDL_EVENT_DROP_COMPLETE:
                case SDL_EVENT_DROP_POSITION:
                    return true;
                default:
                    return false;
            }
        }

        void Dispatch(const SDL_Event& Event, bool AllowInput) {
            if (Event.type == WakeEvent.load(std::memory_order_relaxed)) {
                if (AppContext* App = AppContext::TryGet()) { App->RunPosted(); }
                return;
            }
            if (!AllowInput && IsInputEvent(Event.type)) { return; }

            SDL_Window* Native = SDL_GetWindowFromEvent(&Event);
            if (Native == nullptr) { return; }
            if (WindowImpl* Target = FindWindow(Native)) { Target->HandleEvent(Event); }
        }
    }  // namespace

    void Startup() {
        // Ctrl+C in a terminal should end the process as it does on Windows, not become a quit event; and the last
        // window closing is the App's decision.
        SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
        SDL_SetHint(SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE, "0");
        SDL_SetHint(SDL_HINT_APP_NAME, "XenUI");
        // libdecor's GTK title bar stalls interactive resizes on GNOME (the window only catches up when the pointer
        // is released). Without it SDL uses server-side decorations where the compositor has them, and XenUI draws
        // its own title bar where it doesn't (see Decorations.cpp).
        SDL_SetHint(SDL_HINT_VIDEO_WAYLAND_ALLOW_LIBDECOR, "0");
        if (!SDL_Init(SDL_INIT_EVENTS)) { throw Error(std::format("SDL: initialization failed: {}", SDL_GetError())); }
        if (WakeEvent.load() == 0) { WakeEvent.store(SDL_RegisterEvents(1)); }
    }

    void Shutdown() {
        if (Context != nullptr) {
            SDL_GL_DestroyContext(Context);
            Context = nullptr;
        }
        SDL_Quit();
    }

    void EnsureVideo() {
        if (HasVideo()) { return; }
        if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
            throw Error(std::format("Window: no display is available ({})", SDL_GetError()));
        }
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
        // Windows are opaque; an alpha channel would make X11 pick a translucent visual.
        SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
    }

    bool HasVideo() {
        return SDL_WasInit(SDL_INIT_VIDEO) != 0;
    }

    SDL_GLContext GetGLContext(SDL_Window* Window) {
        if (Context != nullptr) { return Context; }
        Context = SDL_GL_CreateContext(Window);
        if (Context == nullptr) {
            throw Error(std::format("Window: an OpenGL 3.3 context could not be created ({})", SDL_GetError()));
        }
        SDL_GL_MakeCurrent(Window, Context);
        AppContext::Get().GetGraphicsDevice().InitializeGL(&LoadGLFunction);
        return Context;
    }

    uint32_t GetWakeEvent() {
        return WakeEvent.load(std::memory_order_relaxed);
    }

    void Wake() {
        SDL_Event Event {};
        Event.type = WakeEvent.load(std::memory_order_relaxed);
        SDL_PushEvent(&Event);
    }

    void Attach(SDL_Window* Window, WindowImpl* Owner) {
        SDL_SetPointerProperty(SDL_GetWindowProperties(Window), kWindowProperty, Owner);
    }

    WindowImpl* FindWindow(SDL_Window* Window) {
        return static_cast<WindowImpl*>(SDL_GetPointerProperty(SDL_GetWindowProperties(Window), kWindowProperty, nullptr));
    }

    void PumpEvents(int32_t TimeoutMs, bool AllowInput) {
        // Local, not reused: a handler may open a file dialog, whose modal loop pumps events again.
        std::vector<SDL_Event> Batch;
        SDL_Event Event {};
        if (SDL_WaitEventTimeout(&Event, TimeoutMs)) {
            do {
                Batch.push_back(Event);
            } while (SDL_PollEvent(&Event));
        }

        // SDL reports every motion sample (1000 a second from some mice), where Windows merges moves until they're
        // read. Each one is a full hit test and drag update, so merge runs of moves the same way: keep the latest
        // position and add up the relative motion.
        for (size_t Index = 0; Index < Batch.size(); ++Index) {
            const SDL_Event& Current = Batch[Index];
            if (Current.type == SDL_EVENT_MOUSE_MOTION && Index + 1 < Batch.size()) {
                SDL_Event& Next = Batch[Index + 1];
                if (Next.type == SDL_EVENT_MOUSE_MOTION && Next.motion.windowID == Current.motion.windowID &&
                    Next.motion.which == Current.motion.which && Next.motion.state == Current.motion.state) {
                    Next.motion.xrel += Current.motion.xrel;
                    Next.motion.yrel += Current.motion.yrel;
                    continue;
                }
            }
            try {
                Dispatch(Current, AllowInput);
            } catch (...) {
                if (AppContext* App = AppContext::TryGet()) { App->ReportException(std::current_exception()); }
            }
        }
        WindowImpl::DestroyClosedPopups();
    }
}  // namespace XenUI::Sdl

namespace XenUI {
    void PumpPlatformEvents() {
        Sdl::PumpEvents(0, true);
    }
}  // namespace XenUI
