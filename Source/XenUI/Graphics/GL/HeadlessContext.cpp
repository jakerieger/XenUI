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

#include "XenUI/Graphics/GL/HeadlessContext.hpp"

#include <dlfcn.h>

#include <cstdint>

namespace XenUI {
    namespace {
        using EGLint     = int32_t;
        using EGLBoolean = unsigned int;
        using EGLenum    = unsigned int;

        constexpr EGLenum kPlatformSurfacelessMesa  = 0x31DD;
        constexpr EGLint kNone                      = 0x3038;
        constexpr EGLint kRenderableType            = 0x3040;
        constexpr EGLint kOpenGLBit                 = 0x0008;
        constexpr EGLint kSurfaceType               = 0x3033;
        constexpr EGLenum kOpenGLApi                = 0x30A2;
        constexpr EGLint kContextMajorVersion       = 0x3098;
        constexpr EGLint kContextMinorVersion       = 0x30FB;
        constexpr EGLint kContextOpenGLProfileMask  = 0x30FD;
        constexpr EGLint kContextOpenGLCoreProfile  = 0x0001;

        struct Egl {
            void* (*GetProcAddress)(const char*)                                   = nullptr;
            void* (*GetPlatformDisplay)(EGLenum, void*, const intptr_t*)           = nullptr;
            EGLBoolean (*Initialize)(void*, EGLint*, EGLint*)                      = nullptr;
            EGLBoolean (*Terminate)(void*)                                         = nullptr;
            EGLBoolean (*BindAPI)(EGLenum)                                         = nullptr;
            EGLBoolean (*ChooseConfig)(void*, const EGLint*, void**, EGLint, EGLint*) = nullptr;
            void* (*CreateContext)(void*, void*, void*, const EGLint*)             = nullptr;
            EGLBoolean (*DestroyContext)(void*, void*)                             = nullptr;
            EGLBoolean (*MakeCurrent)(void*, void*, void*, void*)                  = nullptr;
        };

        Egl Functions;

        template<class T>
        bool Resolve(void* Library, T& Target, const char* Name) {
            Target = reinterpret_cast<T>(dlsym(Library, Name));
            return Target != nullptr;
        }

        void* LoadGLProc(const char* Name) {
            return Functions.GetProcAddress != nullptr ? Functions.GetProcAddress(Name) : nullptr;
        }
    }  // namespace

    std::unique_ptr<HeadlessGLContext> HeadlessGLContext::TryCreate() {
        void* Library = dlopen("libEGL.so.1", RTLD_NOW | RTLD_LOCAL);
        if (Library == nullptr) { return nullptr; }

        std::unique_ptr<HeadlessGLContext> Result(new HeadlessGLContext());
        Result->_Library = Library;

        const bool Resolved = Resolve(Library, Functions.GetProcAddress, "eglGetProcAddress") &&
                              Resolve(Library, Functions.GetPlatformDisplay, "eglGetPlatformDisplay") &&
                              Resolve(Library, Functions.Initialize, "eglInitialize") &&
                              Resolve(Library, Functions.Terminate, "eglTerminate") &&
                              Resolve(Library, Functions.BindAPI, "eglBindAPI") &&
                              Resolve(Library, Functions.ChooseConfig, "eglChooseConfig") &&
                              Resolve(Library, Functions.CreateContext, "eglCreateContext") &&
                              Resolve(Library, Functions.DestroyContext, "eglDestroyContext") &&
                              Resolve(Library, Functions.MakeCurrent, "eglMakeCurrent");
        if (!Resolved) { return nullptr; }

        void* Display = Functions.GetPlatformDisplay(kPlatformSurfacelessMesa, nullptr, nullptr);
        EGLint Major = 0;
        EGLint Minor = 0;
        if (Display == nullptr || Functions.Initialize(Display, &Major, &Minor) == 0) { return nullptr; }
        Result->_Display = Display;
        if (Functions.BindAPI(kOpenGLApi) == 0) { return nullptr; }

        const EGLint ConfigAttributes[] = {kRenderableType, kOpenGLBit, kSurfaceType, 0, kNone};
        void* Config                    = nullptr;
        EGLint ConfigCount              = 0;
        if (Functions.ChooseConfig(Display, ConfigAttributes, &Config, 1, &ConfigCount) == 0 || ConfigCount == 0) {
            Config = nullptr;
        }

        const EGLint ContextAttributes[] = {kContextMajorVersion,
                                            3,
                                            kContextMinorVersion,
                                            3,
                                            kContextOpenGLProfileMask,
                                            kContextOpenGLCoreProfile,
                                            kNone};
        Result->_Context = Functions.CreateContext(Display, Config, nullptr, ContextAttributes);
        if (Result->_Context == nullptr) { return nullptr; }
        if (Functions.MakeCurrent(Display, nullptr, nullptr, Result->_Context) == 0) { return nullptr; }
        return Result;
    }

    HeadlessGLContext::~HeadlessGLContext() {
        if (_Display != nullptr) {
            Functions.MakeCurrent(_Display, nullptr, nullptr, nullptr);
            if (_Context != nullptr) { Functions.DestroyContext(_Display, _Context); }
            Functions.Terminate(_Display);
        }
        // libEGL stays loaded: drivers register atexit handlers that would run after an unload.
    }

    void HeadlessGLContext::MakeCurrent() {
        Functions.MakeCurrent(_Display, nullptr, nullptr, _Context);
    }

    GL::ProcLoader HeadlessGLContext::GetProcLoader() {
        return &LoadGLProc;
    }
}  // namespace XenUI
