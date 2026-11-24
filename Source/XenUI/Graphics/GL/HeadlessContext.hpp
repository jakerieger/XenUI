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

#include "XenUI/Graphics/GL/GLApi.hpp"

#include <memory>

namespace XenUI {
    /// @brief An OpenGL 3.3 core context with no window, through EGL's surfaceless platform. For rendering offscreen
    /// (tests, pixel comparisons) where no display server is available. libEGL is opened at run time, so nothing
    /// links against it.
    class HeadlessGLContext {
    public:
        /// @return nullptr when EGL, the surfaceless platform or a 3.3 core context isn't available.
        [[nodiscard]] static std::unique_ptr<HeadlessGLContext> TryCreate();

        ~HeadlessGLContext();

        HeadlessGLContext(const HeadlessGLContext&)            = delete;
        HeadlessGLContext& operator=(const HeadlessGLContext&) = delete;

        void MakeCurrent();

        /// @brief Resolves GL entry points for this context; pass to GraphicsDevice::InitializeGL.
        [[nodiscard]] static GL::ProcLoader GetProcLoader();

    private:
        HeadlessGLContext() = default;

        void* _Library = nullptr;
        void* _Display = nullptr;
        void* _Context = nullptr;
    };
}  // namespace XenUI
