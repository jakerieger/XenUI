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

#include "XenUI/Graphics/GL/FontLibrary.hpp"
#include "XenUI/Graphics/GL/GLApi.hpp"

#include <cstdint>
#include <memory>

namespace XenUI {
    class GLRenderer;
    class GlyphCache;
    class LiveRenderer;

    /// @brief The process-wide OpenGL device state. See Graphics/GraphicsDevice.hpp.
    ///
    /// Every window draws with one context (the platform makes it current on each window's drawable), so layer
    /// caches, glyphs and images are shared. Fonts need no context, so text can be laid out before any window
    /// exists; the GL side starts once the platform has a context and calls InitializeGL.
    class GraphicsDevice {
    public:
        GraphicsDevice();
        ~GraphicsDevice();

        GraphicsDevice(const GraphicsDevice&)            = delete;
        GraphicsDevice& operator=(const GraphicsDevice&) = delete;

        /// @brief Loads GL through Loader and creates the shared renderer state. Call once with the context current;
        /// later calls do nothing.
        void InitializeGL(GL::ProcLoader Loader);
        [[nodiscard]] bool IsGLReady() const { return _Renderer != nullptr; }

        /// @brief Rebuilds every device resource after a context loss, in the new context, which must be current.
        void Recreate();

        /// @brief Deletes GL objects released since the last frame. Call with the context current.
        void BeginFrame();

        [[nodiscard]] uint64_t GetGeneration() const { return _Generation; }

        [[nodiscard]] FontLibrary& GetFonts() { return _Fonts; }
        [[nodiscard]] GLRenderer& GetRenderer();
        [[nodiscard]] LiveRenderer& GetLiveRenderer();

    private:
        void CreateResources();
        void ReleaseResources();

        FontLibrary _Fonts;
        GL::ProcLoader _Loader = nullptr;
        std::unique_ptr<GlyphCache> _Glyphs;
        std::unique_ptr<GLRenderer> _Renderer;
        std::unique_ptr<LiveRenderer> _LiveRenderer;
        uint64_t _Generation = 1;
    };
}  // namespace XenUI
