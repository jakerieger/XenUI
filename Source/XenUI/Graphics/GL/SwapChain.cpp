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

#include "XenUI/Graphics/GL/SwapChain.hpp"
#include "XenUI/Core/Error.hpp"
#include "XenUI/Graphics/GL/GLApi.hpp"

#include <algorithm>
#include <cstring>

namespace XenUI {
    SwapChain::SwapChain(uint32_t Width, uint32_t Height, float DpiScale, std::unique_ptr<GLSurface> Window)
        : _Window(std::move(Window)), _Width(std::max(Width, 1u)), _Height(std::max(Height, 1u)),
          _DpiScale(DpiScale) {}

    SwapChain::~SwapChain() = default;

    void SwapChain::Resize(uint32_t Width, uint32_t Height) {
        _Width  = std::max(Width, 1u);
        _Height = std::max(Height, 1u);
    }

    void SwapChain::MakeCurrent() {
        if (_Window) { _Window->MakeCurrent(); }
    }

    void SwapChain::Prepare(uint64_t Generation) {
        const bool Current = _Color && _Color.GetGeneration() == Generation && _AllocatedWidth == _Width &&
                             _AllocatedHeight == _Height;
        if (Current) { return; }

        _Framebuffer.Reset();
        _Color.Reset();
        GL::GLuint Texture     = 0;
        GL::GLuint Framebuffer = 0;
        GL::Api.GenTextures(1, &Texture);
        // Unit 4 is the renderer's scratch unit, so this doesn't disturb its bound atlases.
        GL::Api.ActiveTexture(GL::TEXTURE0 + 4);
        GL::Api.BindTexture(GL::TEXTURE_2D, Texture);
        GL::Api.TexImage2D(GL::TEXTURE_2D,
                           0,
                           static_cast<GL::GLint>(GL::RGBA8),
                           static_cast<GL::GLsizei>(_Width),
                           static_cast<GL::GLsizei>(_Height),
                           0,
                           GL::RGBA,
                           GL::UNSIGNED_BYTE,
                           nullptr);
        GL::Api.TexParameteri(GL::TEXTURE_2D, GL::TEXTURE_MIN_FILTER, static_cast<GL::GLint>(GL::NEAREST));
        GL::Api.TexParameteri(GL::TEXTURE_2D, GL::TEXTURE_MAG_FILTER, static_cast<GL::GLint>(GL::NEAREST));
        GL::Api.TexParameteri(GL::TEXTURE_2D, GL::TEXTURE_MAX_LEVEL, 0);
        GL::Api.GenFramebuffers(1, &Framebuffer);
        GL::Api.BindFramebuffer(GL::FRAMEBUFFER, Framebuffer);
        GL::Api.FramebufferTexture2D(GL::FRAMEBUFFER, GL::COLOR_ATTACHMENT0, GL::TEXTURE_2D, Texture, 0);
        if (GL::Api.CheckFramebufferStatus(GL::FRAMEBUFFER) != GL::FRAMEBUFFER_COMPLETE) {
            GL::Api.DeleteFramebuffers(1, &Framebuffer);
            GL::Api.DeleteTextures(1, &Texture);
            throw Error("OpenGL: the window back buffer is incomplete");
        }
        _Color           = GLTexture(Texture, Generation);
        _Framebuffer     = GLFramebuffer(Framebuffer, Generation);
        _AllocatedWidth  = _Width;
        _AllocatedHeight = _Height;
    }

    bool SwapChain::Present(const std::vector<PixelRect>&) {
        if (!_Window) { return true; }
        // TODO(linux): copy only the changed rects and pass them to EGL_KHR_swap_buffers_with_damage when SDL
        // exposes it. A full blit is cheap next to drawing, so damage tracking still pays off.
        const auto Width  = static_cast<GL::GLint>(_AllocatedWidth);
        const auto Height = static_cast<GL::GLint>(_AllocatedHeight);
        GL::Api.Disable(GL::SCISSOR_TEST);
        GL::Api.BindFramebuffer(GL::READ_FRAMEBUFFER, _Framebuffer.Get());
        GL::Api.BindFramebuffer(GL::DRAW_FRAMEBUFFER, 0);
        GL::Api.BlitFramebuffer(0, 0, Width, Height, 0, 0, Width, Height, GL::COLOR_BUFFER_BIT, GL::NEAREST);
        GL::Api.BindFramebuffer(GL::FRAMEBUFFER, 0);
        return _Window->SwapBuffers();
    }

    std::vector<uint8_t> SwapChain::ReadPixels() const {
        const size_t Row = static_cast<size_t>(_AllocatedWidth) * 4;
        std::vector<uint8_t> Pixels(Row * _AllocatedHeight);
        if (!_Framebuffer) { return Pixels; }
        GL::Api.BindFramebuffer(GL::FRAMEBUFFER, _Framebuffer.Get());
        GL::Api.PixelStorei(GL::PACK_ALIGNMENT, 4);
        GL::Api.ReadPixels(0,
                           0,
                           static_cast<GL::GLsizei>(_AllocatedWidth),
                           static_cast<GL::GLsizei>(_AllocatedHeight),
                           GL::BGRA,
                           GL::UNSIGNED_BYTE,
                           Pixels.data());
        // GL returns the bottom row first.
        std::vector<uint8_t> Flipped(Pixels.size());
        for (uint32_t Y = 0; Y < _AllocatedHeight; ++Y) {
            std::memcpy(Flipped.data() + Y * Row, Pixels.data() + (_AllocatedHeight - 1 - Y) * Row, Row);
        }
        return Flipped;
    }
}  // namespace XenUI
