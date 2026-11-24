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

#include "XenUI/Core/Color.hpp"
#include "XenUI/Core/Math.hpp"
#include "XenUI/Core/Matrix.hpp"
#include "XenUI/Graphics/DamageRegion.hpp"
#include "XenUI/Graphics/GL/LayerSurfaces.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace XenUI {
    class DisplayList;
    class GraphicsDevice;
    class LiveRecording;
    class SwapChain;

    /// @brief OpenGL renderer for the Compositor, drawing through GLRenderer. See Graphics/CompositorBackend.hpp.
    class CompositorBackend {
    public:
        CompositorBackend() = default;
        ~CompositorBackend();

        CompositorBackend(const CompositorBackend&)            = delete;
        CompositorBackend& operator=(const CompositorBackend&) = delete;

        /// @brief Binds the target and device for this frame. Call first in every frame.
        void BeginFrame(SwapChain& Target, GraphicsDevice& Device);

        [[nodiscard]] float GetDpiScale() const { return _DpiScale; }
        [[nodiscard]] uint64_t GetGeneration() const;
        /// @brief How many back buffers the target cycles through, so damage is kept for that many frames.
        [[nodiscard]] size_t GetBufferCount() const;
        [[nodiscard]] uint32_t GetPixelWidth() const;
        [[nodiscard]] uint32_t GetPixelHeight() const;
        /// @brief The target's size in device-independent pixels.
        [[nodiscard]] Size GetViewport() const;

        /// @brief Blocks until the target can accept another frame.
        void WaitForFrame();

        void BeginWindowDraw();
        void EndWindowDraw();
        /// @brief Clips to Area (in DIPs) and clears it to Background until EndDamage.
        void BeginDamage(const Rect& Area, const Color& Background);
        void EndDamage();
        /// @return false when the target didn't present normally and its back buffers must be fully repainted.
        bool Present(const std::vector<PixelRect>& Changed);

        void AllocateCache(LayerCache& Cache, uint32_t PixelWidth, uint32_t PixelHeight, float Scale);
        void ReleaseCache(LayerCache& Cache);
        /// @brief Redirects drawing into Cache, cleared to transparent, until EndCacheDraw.
        void BeginCacheDraw(LayerCache& Cache);
        void EndCacheDraw();
        void DrawCache(const LayerCache& Cache, const Matrix3x2& World, float Opacity);

        void AllocateLiveSurface(LiveSurface& Surface, uint32_t PixelWidth, uint32_t PixelHeight, float Scale);
        void ReleaseLiveSurface(LiveSurface& Surface);
        void RenderLive(LiveSurface& Surface, const Size& Extent, float Scale, const LiveRecording& Recording);
        void DrawLiveSurface(const LiveSurface& Surface, const Size& Extent, const Matrix3x2& World);

        void PushOpacity(float Opacity);
        void PopOpacity();
        /// @brief Clips to Local, in the coordinate space that World maps to the window.
        void PushClip(const Rect& Local, const Matrix3x2& World);
        void PopClip();
        void DrawDisplayList(const DisplayList& Content, const Matrix3x2& World);
        void DrawFlash(const Rect& Area, const Color& Fill, const Color& Outline);

    private:
        /// @brief Maps device-independent pixels to the bound target's pixels.
        [[nodiscard]] Matrix3x2 ToPixels(const Matrix3x2& World) const;

        SwapChain* _Target      = nullptr;
        GraphicsDevice* _Device = nullptr;
        float _DpiScale         = 1.0f;
        /// @brief Pixels per DIP of each target being drawn: the window's DPI scale, or a cache's own scale.
        std::vector<float> _Scales;
    };
}  // namespace XenUI
