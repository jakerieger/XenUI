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

#include "XenUI/Graphics/DamageRegion.hpp"
#include "XenUI/Graphics/GL/GLObjects.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace XenUI {
    /// @brief What a window's GL drawable can do. The platform layer implements it (Platform/Linux/WindowImpl.cpp).
    class GLSurface {
    public:
        virtual ~GLSurface() = default;

        /// @brief Makes the device's context current on this window's drawable.
        virtual void MakeCurrent() = 0;
        /// @return false when the drawable is gone and the window must be fully repainted.
        virtual bool SwapBuffers() = 0;
    };

    /// @brief A window's (or an offscreen) render target. The compositor draws into a persistent back buffer
    /// texture, so damage-limited repaints work whatever the platform's buffer age; Present copies it to the window.
    /// Since only one buffer keeps content, the compositor needs no damage history.
    class SwapChain {
    public:
        static constexpr size_t kBufferCount = 1;

        /// @param Window nullptr for an offscreen target, read back with ReadPixels.
        SwapChain(uint32_t Width, uint32_t Height, float DpiScale, std::unique_ptr<GLSurface> Window = nullptr);
        ~SwapChain();

        SwapChain(const SwapChain&)            = delete;
        SwapChain& operator=(const SwapChain&) = delete;

        void Resize(uint32_t Width, uint32_t Height);
        void SetDpiScale(float DpiScale) { _DpiScale = DpiScale; }

        [[nodiscard]] uint32_t GetWidth() const { return _Width; }
        [[nodiscard]] uint32_t GetHeight() const { return _Height; }
        [[nodiscard]] float GetDpiScale() const { return _DpiScale; }

        void MakeCurrent();

        /// @brief Creates or resizes the back buffer for the device generation. Call with the context current.
        void Prepare(uint64_t Generation);
        [[nodiscard]] uint32_t GetFramebuffer() const { return _Framebuffer.Get(); }

        /// @return false when the frame didn't reach the window normally.
        bool Present(const std::vector<PixelRect>& Changed);

        /// @brief The back buffer as premultiplied BGRA, top row first. Call with the context current.
        [[nodiscard]] std::vector<uint8_t> ReadPixels() const;

    private:
        std::unique_ptr<GLSurface> _Window;
        uint32_t _Width  = 0;
        uint32_t _Height = 0;
        float _DpiScale  = 1.0f;

        GLTexture _Color;
        GLFramebuffer _Framebuffer;
        uint32_t _AllocatedWidth  = 0;
        uint32_t _AllocatedHeight = 0;
    };
}  // namespace XenUI
