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

#include "XenUI/Graphics/GL/GraphicsDevice.hpp"
#include "XenUI/Core/Error.hpp"
#include "XenUI/Graphics/GL/GLObjects.hpp"
#include "XenUI/Graphics/GL/GLRenderer.hpp"
#include "XenUI/Graphics/GL/GlyphCache.hpp"
#include "XenUI/Graphics/GL/LiveRenderer.hpp"

namespace XenUI {
    GraphicsDevice::GraphicsDevice() = default;

    GraphicsDevice::~GraphicsDevice() {
        ReleaseResources();
    }

    void GraphicsDevice::InitializeGL(GL::ProcLoader Loader) {
        if (_Renderer) { return; }
        GL::Load(Loader);
        _Loader = Loader;
        CreateResources();
    }

    void GraphicsDevice::Recreate() {
        ReleaseResources();
        ++_Generation;
        if (_Loader != nullptr) {
            GL::Load(_Loader);
            CreateResources();
        }
    }

    void GraphicsDevice::BeginFrame() {
        FlushGLDeletes(_Generation);
    }

    GLRenderer& GraphicsDevice::GetRenderer() {
        if (!_Renderer) { throw Error("GraphicsDevice: OpenGL is not initialized"); }
        return *_Renderer;
    }

    LiveRenderer& GraphicsDevice::GetLiveRenderer() {
        if (!_LiveRenderer) { throw Error("GraphicsDevice: OpenGL is not initialized"); }
        return *_LiveRenderer;
    }

    void GraphicsDevice::CreateResources() {
        _Glyphs = std::make_unique<GlyphCache>();
        _Glyphs->Initialize(_Generation);
        _Renderer     = std::make_unique<GLRenderer>(*_Glyphs, _Generation);
        _LiveRenderer = std::make_unique<LiveRenderer>(_Generation);
    }

    void GraphicsDevice::ReleaseResources() {
        _LiveRenderer.reset();
        _Renderer.reset();
        _Glyphs.reset();
    }
}  // namespace XenUI
