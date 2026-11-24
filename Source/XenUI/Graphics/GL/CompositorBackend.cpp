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

#include "XenUI/Graphics/GL/CompositorBackend.hpp"
#include "XenUI/Core/Error.hpp"
#include "XenUI/Graphics/DisplayList.hpp"
#include "XenUI/Graphics/GL/GLApi.hpp"
#include "XenUI/Graphics/GL/GLRenderer.hpp"
#include "XenUI/Graphics/GL/GraphicsDevice.hpp"
#include "XenUI/Graphics/GL/LiveRenderer.hpp"
#include "XenUI/Graphics/GL/SwapChain.hpp"
#include "XenUI/Graphics/LiveRecording.hpp"

namespace XenUI {
    namespace {
        /// @brief A colour texture of the given size attached to a new framebuffer.
        void CreateTarget(GLTexture& Texture,
                          GLFramebuffer& Framebuffer,
                          uint32_t Width,
                          uint32_t Height,
                          uint64_t Generation) {
            Framebuffer.Reset();
            Texture.Reset();
            GL::GLuint TextureName     = 0;
            GL::GLuint FramebufferName = 0;
            GL::Api.GenTextures(1, &TextureName);
            GL::Api.ActiveTexture(GL::TEXTURE0 + 4);
            GL::Api.BindTexture(GL::TEXTURE_2D, TextureName);
            GL::Api.TexImage2D(GL::TEXTURE_2D,
                               0,
                               static_cast<GL::GLint>(GL::RGBA8),
                               static_cast<GL::GLsizei>(Width),
                               static_cast<GL::GLsizei>(Height),
                               0,
                               GL::RGBA,
                               GL::UNSIGNED_BYTE,
                               nullptr);
            GL::Api.TexParameteri(GL::TEXTURE_2D, GL::TEXTURE_MAX_LEVEL, 0);
            GL::Api.TexParameteri(GL::TEXTURE_2D, GL::TEXTURE_WRAP_S, static_cast<GL::GLint>(GL::CLAMP_TO_EDGE));
            GL::Api.TexParameteri(GL::TEXTURE_2D, GL::TEXTURE_WRAP_T, static_cast<GL::GLint>(GL::CLAMP_TO_EDGE));
            GL::Api.GenFramebuffers(1, &FramebufferName);
            GL::Api.BindFramebuffer(GL::FRAMEBUFFER, FramebufferName);
            GL::Api.FramebufferTexture2D(GL::FRAMEBUFFER, GL::COLOR_ATTACHMENT0, GL::TEXTURE_2D, TextureName, 0);
            Texture     = GLTexture(TextureName, Generation);
            Framebuffer = GLFramebuffer(FramebufferName, Generation);
            if (GL::Api.CheckFramebufferStatus(GL::FRAMEBUFFER) != GL::FRAMEBUFFER_COMPLETE) {
                Framebuffer.Reset();
                Texture.Reset();
                throw Error("OpenGL: an offscreen layer target is incomplete");
            }
        }
    }  // namespace

    CompositorBackend::~CompositorBackend() = default;

    void CompositorBackend::BeginFrame(SwapChain& Target, GraphicsDevice& Device) {
        _Target   = &Target;
        _Device   = &Device;
        _DpiScale = Target.GetDpiScale();
        _Target->MakeCurrent();
        _Device->BeginFrame();
    }

    uint64_t CompositorBackend::GetGeneration() const {
        return _Device->GetGeneration();
    }

    size_t CompositorBackend::GetBufferCount() const {
        return SwapChain::kBufferCount;
    }

    uint32_t CompositorBackend::GetPixelWidth() const {
        return _Target->GetWidth();
    }

    uint32_t CompositorBackend::GetPixelHeight() const {
        return _Target->GetHeight();
    }

    Size CompositorBackend::GetViewport() const {
        return {static_cast<float>(_Target->GetWidth()) / _DpiScale,
                static_cast<float>(_Target->GetHeight()) / _DpiScale};
    }

    // Presentation is paced by the swap interval the platform sets, so there's nothing to wait for here.
    void CompositorBackend::WaitForFrame() {}

    Matrix3x2 CompositorBackend::ToPixels(const Matrix3x2& World) const {
        const float Scale = _Scales.empty() ? _DpiScale : _Scales.back();
        return World * Matrix3x2::Scaling(Scale, Scale);
    }

    void CompositorBackend::BeginWindowDraw() {
        _Target->Prepare(_Device->GetGeneration());
        _Scales.push_back(_DpiScale);
        _Device->GetRenderer().BeginTarget(_Target->GetFramebuffer(), _Target->GetWidth(), _Target->GetHeight());
    }

    void CompositorBackend::EndWindowDraw() {
        _Device->GetRenderer().EndTarget();
        _Scales.pop_back();
    }

    void CompositorBackend::BeginDamage(const Rect& Area, const Color& Background) {
        GLRenderer& Renderer = _Device->GetRenderer();
        Renderer.PushScissor(Matrix3x2::Scaling(_DpiScale, _DpiScale).TransformRect(Area));
        Renderer.Clear(Background);
    }

    void CompositorBackend::EndDamage() {
        _Device->GetRenderer().PopScissor();
    }

    bool CompositorBackend::Present(const std::vector<PixelRect>& Changed) {
        return _Target->Present(Changed);
    }

    void CompositorBackend::AllocateCache(LayerCache& Cache, uint32_t PixelWidth, uint32_t PixelHeight, float Scale) {
        CreateTarget(Cache.Texture, Cache.Framebuffer, PixelWidth, PixelHeight, _Device->GetGeneration());
        Cache.PixelWidth  = PixelWidth;
        Cache.PixelHeight = PixelHeight;
        Cache.Generation  = _Device->GetGeneration();
        Cache.Scale       = Scale;
    }

    void CompositorBackend::ReleaseCache(LayerCache& Cache) {
        Cache.Framebuffer.Reset();
        Cache.Texture.Reset();
    }

    void CompositorBackend::BeginCacheDraw(LayerCache& Cache) {
        GLRenderer& Renderer = _Device->GetRenderer();
        _Scales.push_back(Cache.Scale);
        Renderer.BeginTarget(Cache.Framebuffer.Get(), Cache.PixelWidth, Cache.PixelHeight);
        Renderer.Clear(Color {0.0f, 0.0f, 0.0f, 0.0f});
    }

    void CompositorBackend::EndCacheDraw() {
        _Device->GetRenderer().EndTarget();
        _Scales.pop_back();
    }

    void CompositorBackend::DrawCache(const LayerCache& Cache, const Matrix3x2& World, float Opacity) {
        _Device->GetRenderer().DrawTexture(Cache.Texture.Get(),
                                           Cache.Bounds,
                                           {0.0f, 0.0f, 1.0f, 1.0f},
                                           ToPixels(World),
                                           Opacity,
                                           ImageInterpolation::Linear,
                                           true);
    }

    void CompositorBackend::AllocateLiveSurface(LiveSurface& Surface,
                                                uint32_t PixelWidth,
                                                uint32_t PixelHeight,
                                                float Scale) {
        CreateTarget(Surface.Texture, Surface.Framebuffer, PixelWidth, PixelHeight, _Device->GetGeneration());
        Surface.PixelWidth  = PixelWidth;
        Surface.PixelHeight = PixelHeight;
        Surface.Scale       = Scale;
        Surface.Generation  = _Device->GetGeneration();
    }

    void CompositorBackend::ReleaseLiveSurface(LiveSurface& Surface) {
        Surface = {};
    }

    void CompositorBackend::RenderLive(LiveSurface& Surface,
                                       const Size& Extent,
                                       float Scale,
                                       const LiveRecording& Recording) {
        _Device->GetLiveRenderer().Render(Surface.Framebuffer.Get(), Extent, Scale, Recording);
    }

    void CompositorBackend::DrawLiveSurface(const LiveSurface& Surface, const Size& Extent, const Matrix3x2& World) {
        _Device->GetRenderer().DrawTexture(Surface.Texture.Get(),
                                           {0.0f, 0.0f, Extent.Width, Extent.Height},
                                           {0.0f, 0.0f, 1.0f, 1.0f},
                                           ToPixels(World),
                                           1.0f,
                                           ImageInterpolation::Linear,
                                           true);
    }

    void CompositorBackend::PushOpacity(float Opacity) {
        _Device->GetRenderer().PushOpacity(Opacity);
    }

    void CompositorBackend::PopOpacity() {
        _Device->GetRenderer().PopOpacity();
    }

    void CompositorBackend::PushClip(const Rect& Local, const Matrix3x2& World) {
        _Device->GetRenderer().PushClip(Local, ToPixels(World));
    }

    void CompositorBackend::PopClip() {
        _Device->GetRenderer().PopScissor();
    }

    void CompositorBackend::DrawDisplayList(const DisplayList& Content, const Matrix3x2& World) {
        _Device->GetRenderer().DrawDisplayList(Content, ToPixels(World));
    }

    void CompositorBackend::DrawFlash(const Rect& Area, const Color& Fill, const Color& Outline) {
        GLRenderer& Renderer    = _Device->GetRenderer();
        const Matrix3x2 Pixels = ToPixels(Matrix3x2::Identity());
        Renderer.FillRect(Area, Fill, Pixels);
        Renderer.StrokeRect(Area.Inflated(-0.5f), Outline, StrokeStyle {}, Pixels);
    }
}  // namespace XenUI
