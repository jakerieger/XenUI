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

#include "XenUI/Graphics/GraphicsDevice.hpp"

#include <Windows.h>

#include <cstdint>
#include <vector>

namespace XenUI {
    /// @brief Flip-sequential swapchain for a single HWND, with a Direct2D context targeting its back buffer.
    /// Sequential flips preserve buffer contents, which is what makes partial repaints possible.
    class SwapChain {
    public:
        static constexpr uint32_t kBufferCount = 2;

        SwapChain(GraphicsDevice& Device, HWND Hwnd, float Dpi);
        ~SwapChain();

        SwapChain(const SwapChain&)            = delete;
        SwapChain& operator=(const SwapChain&) = delete;

        /// @brief Resizes the back buffers to the given physical pixel size. Zero-sized requests are ignored.
        void Resize(uint32_t Width, uint32_t Height);
        void SetDpi(float Dpi);

        /// @brief Blocks until DXGI can accept another frame, unless MarkFrameReady already consumed that signal.
        void WaitForFrame();

        /// @brief Records that the caller already waited on GetFrameWaitable.
        void MarkFrameReady() { _FrameReady = true; }

        /// @param DirtyRects Pixels changed since the previous present. Empty marks the whole buffer as changed.
        /// @return false if the frame was not displayed normally (for example, occluded).
        bool Present(const std::vector<RECT>& DirtyRects);

        [[nodiscard]] ID2D1DeviceContext* GetContext() const { return _Context.Get(); }
        [[nodiscard]] ID2D1Bitmap1* GetTargetBitmap() const { return _TargetBitmap.Get(); }
        [[nodiscard]] HANDLE GetFrameWaitable() const { return _FrameLatencyWaitable; }
        [[nodiscard]] uint32_t GetWidth() const { return _Width; }
        [[nodiscard]] uint32_t GetHeight() const { return _Height; }
        [[nodiscard]] float GetDpi() const { return _Dpi; }

    private:
        void CreateTargetBitmap();

        ComPtr<IDXGISwapChain2> _SwapChain;
        ComPtr<ID2D1DeviceContext> _Context;
        ComPtr<ID2D1Bitmap1> _TargetBitmap;
        HANDLE _FrameLatencyWaitable = nullptr;
        uint32_t _Width              = 0;
        uint32_t _Height             = 0;
        float _Dpi                   = 96.0f;
        bool _FrameReady             = false;
    };
}  // namespace XenUI
