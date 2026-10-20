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

#include "XenUI/Api.hpp"
#include "XenUI/Core/Color.hpp"
#include "XenUI/Core/Math.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <span>

namespace XenUI {
    class LiveRecording;
    class LiveTextureData;

    /// @brief CPU-side BGRA image streamed to the GPU when drawn, for content rewritten every frame such as
    /// spectrograms. Columns can be written into a ring and drawn with a horizontal scroll offset.
    class XENUI_API LiveTexture {
    public:
        LiveTexture(uint32_t Width, uint32_t Height);
        ~LiveTexture();

        LiveTexture(const LiveTexture&)            = delete;
        LiveTexture& operator=(const LiveTexture&) = delete;

        [[nodiscard]] uint32_t GetWidth() const;
        [[nodiscard]] uint32_t GetHeight() const;

        /// @brief Writes one column of premultiplied BGRA pixels, top to bottom. Pixels.size() must equal the height.
        void WriteColumn(uint32_t X, std::span<const uint32_t> Pixels);

        /// @brief Writes every pixel, row by row.
        void WriteAll(std::span<const uint32_t> Pixels);

        /// @internal
        [[nodiscard]] LiveTextureData& GetData() const { return *_Data; }

    private:
        std::unique_ptr<LiveTextureData> _Data;
    };

    /// @brief Immediate-mode drawing surface for high-frequency content, rendered with instanced Direct3D 11 draws.
    /// Everything recorded in one callback becomes a handful of draw calls regardless of primitive count, so tens of
    /// thousands of segments per frame are practical. Coordinates are device-independent pixels from the top-left.
    class XENUI_API LiveCanvas {
    public:
        explicit LiveCanvas(LiveRecording& Recording, const Size& Extent);

        [[nodiscard]] const Size& GetSize() const { return _Extent; }

        void FillRect(const Rect& Bounds, const Color& Fill);
        void DrawLine(const Vec2& Start, const Vec2& End, float Width, const Color& Stroke);

        /// @brief Connected line segments with round joins. Cost is one primitive per segment.
        void DrawPolyline(std::span<const Vec2> Points, float Width, const Color& Stroke);

        /// @brief Vertical bars from Baseline to each value, one per pixel column; values are y coordinates.
        void FillColumns(float Left, float ColumnWidth, float Baseline, std::span<const float> Tops, const Color& Fill);

        /// @param SourceOffset Horizontal offset into the texture, as a fraction of its width. The texture wraps,
        /// so advancing the offset scrolls a ring of columns.
        void DrawTexture(const LiveTexture& Texture,
                         const Rect& Destination,
                         float SourceOffset = 0.0f,
                         float Opacity      = 1.0f);

        /// @brief Primitives recorded so far, for profiling.
        [[nodiscard]] size_t GetPrimitiveCount() const;

    private:
        LiveRecording& _Recording;
        Size _Extent;
    };

    using LiveDrawer = std::function<void(LiveCanvas& Canvas)>;
}  // namespace XenUI
