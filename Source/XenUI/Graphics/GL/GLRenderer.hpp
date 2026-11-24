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
#include "XenUI/Graphics/Brush.hpp"
#include "XenUI/Graphics/GL/AtlasPacker.hpp"
#include "XenUI/Graphics/GL/Coverage.hpp"
#include "XenUI/Graphics/GL/GLObjects.hpp"
#include "XenUI/Graphics/Image.hpp"
#include "XenUI/Graphics/StrokeStyle.hpp"

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace XenUI {
    class DisplayList;
    class GlyphCache;
    class ImageData;
    class TextLayoutData;

    /// @brief Draws display lists with OpenGL 3.3, mirroring Graphics/D2D/D2DRenderer.cpp.
    ///
    /// Everything goes through one shader and one vertex stream, flushed only when a texture, the scissor or the
    /// target changes. Rectangles, rounded rectangles, ellipses and lines are drawn as quads whose coverage the
    /// fragment shader computes from a signed distance, so their edges are antialiased under any transform. Paths,
    /// dashed strokes and unusual joins are rasterized on the CPU into a coverage atlas (see Coverage.hpp) and drawn
    /// as textured quads. Glyphs come from GlyphCache. All colour is premultiplied.
    ///
    /// Coordinates passed in are device pixels of the bound target, y down.
    class GLRenderer {
    public:
        GLRenderer(GlyphCache& Glyphs, uint64_t Generation);
        ~GLRenderer();

        GLRenderer(const GLRenderer&)            = delete;
        GLRenderer& operator=(const GLRenderer&) = delete;

        /// @brief Makes Framebuffer (Width by Height pixels) the target, unclipped, until the matching EndTarget.
        void BeginTarget(uint32_t Framebuffer, uint32_t Width, uint32_t Height);
        void EndTarget();

        /// @brief Replaces every pixel inside the current scissor with Fill.
        void Clear(const Color& Fill);

        /// @brief Clips to the pixels Area covers, rounded to whole pixels, intersected with the current clip.
        void PushScissor(const Rect& Area);
        void PopScissor();
        /// @brief Clips to the bounding box of Local mapped through Transform, like Direct2D's axis-aligned clip.
        void PushClip(const Rect& Local, const Matrix3x2& Transform);

        /// @brief Draws everything until PopOpacity into a separate layer, then blends it at Opacity.
        void PushOpacity(float Opacity);
        void PopOpacity();

        void DrawDisplayList(const DisplayList& List, const Matrix3x2& Base);

        void FillRect(const Rect& Bounds, const Brush& Paint, const Matrix3x2& Transform);
        void StrokeRect(const Rect& Bounds, const Brush& Paint, const StrokeStyle& Stroke, const Matrix3x2& Transform);

        /// @brief Draws a premultiplied texture over Destination. FlippedY marks textures rendered by GL, whose first
        /// row is the bottom of the image.
        void DrawTexture(uint32_t Texture,
                         const Rect& Destination,
                         const Rect& Source,
                         const Matrix3x2& Transform,
                         float Opacity,
                         ImageInterpolation Interpolation,
                         bool FlippedY);

        /// @brief Submits every queued vertex.
        void Flush();

    private:
        friend class GLReplayer;

        struct Vertex {
            float Position[2];
            float Shape[2];
            float BrushPosition[2];
            float ShapeParams[4];
            float Color[4];
            float BrushParams[4];
            float BrushExtra[4];
            float Kind[2];
        };

        /// @brief How the shader decides coverage. Must match kFragmentShader.
        enum class ShapeKind : uint8_t {
            Solid        = 0,
            RoundedRect  = 1,
            RoundedFrame = 2,
            Ellipse      = 3,
            EllipseFrame = 4,
            Line         = 5,
            Mask         = 6,
            Glyph        = 7,
            ColorGlyph   = 8,
            Texture      = 9,
        };

        /// @brief Brush parameters already resolved for the shader.
        struct Paint {
            float Kind = 0.0f;
            float Color[4] {};
            float Params[4] {};
            float Extra[4] {};
        };

        struct Target {
            uint32_t Framebuffer = 0;
            uint32_t Width       = 0;
            uint32_t Height      = 0;
            std::vector<PixelBox> Scissors;
            /// @brief Opacity layers this target is waiting to blend back, innermost last.
            std::vector<size_t> Layers;
        };

        struct LayerTexture {
            GLTexture Texture;
            GLFramebuffer Framebuffer;
            uint32_t Width  = 0;
            uint32_t Height = 0;
            bool InUse      = false;
        };

        struct OpenLayer {
            size_t Texture = 0;
            float Opacity  = 1.0f;
            PixelBox Area;
            uint32_t ParentFramebuffer = 0;
        };

        struct MaskKey {
            uint64_t Path  = 0;
            uint64_t Style = 0;
            float M11      = 0.0f;
            float M12      = 0.0f;
            float M21      = 0.0f;
            float M22      = 0.0f;
            int32_t FractionX = 0;
            int32_t FractionY = 0;

            bool operator==(const MaskKey&) const = default;
        };

        struct MaskKeyHash {
            size_t operator()(const MaskKey& Key) const noexcept;
        };

        struct CachedMask {
            AtlasPacker::Slot Slot;
            /// @brief The mask's pixels relative to the transform's whole-pixel translation.
            PixelBox Box;
        };

        struct GradientKey {
            std::vector<GradientStop> Stops;

            bool operator==(const GradientKey&) const = default;
        };

        struct GradientKeyHash {
            size_t operator()(const GradientKey& Key) const noexcept;
        };

        [[nodiscard]] Target& Current() { return _Targets.back(); }
        [[nodiscard]] PixelBox GetScissor() { return Current().Scissors.back(); }
        void ApplyTarget();
        void ApplyScissor();

        Paint ResolveBrush(const Brush& Source, float Opacity = 1.0f);
        float GetGradientRow(const std::vector<GradientStop>& Stops);

        void EmitQuad(const Vec2 (&Device)[4],
                      const Vec2 (&Shape)[4],
                      const Vec2 (&BrushPosition)[4],
                      const float (&ShapeParams)[4],
                      ShapeKind Kind,
                      const Paint& Fill);

        /// @brief An analytic shape: Bounds (local space, shape coordinates relative to Center) inflated by the
        /// antialiasing margin and mapped through Transform.
        void EmitShape(const Rect& Bounds,
                       const Vec2& Center,
                       const float (&ShapeParams)[4],
                       ShapeKind Kind,
                       const Paint& Fill,
                       const Matrix3x2& Transform,
                       float Margin);

        void DrawRoundedRect(const Rect& Bounds, float Radius, const Brush& Paint, const Matrix3x2& Transform);
        void StrokeRoundedRect(const Rect& Bounds,
                               float Radius,
                               const Brush& Paint,
                               const StrokeStyle& Stroke,
                               const Matrix3x2& Transform);
        void FillEllipse(const Vec2& Center, const Vec2& Radius, const Brush& Paint, const Matrix3x2& Transform);
        void StrokeEllipse(const Vec2& Center,
                           const Vec2& Radius,
                           const Brush& Paint,
                           const StrokeStyle& Stroke,
                           const Matrix3x2& Transform);
        void DrawLine(const Vec2& Start,
                      const Vec2& End,
                      const Brush& Paint,
                      const StrokeStyle& Stroke,
                      const Matrix3x2& Transform);

        /// @brief Strokes Figures on the CPU and draws the result as a coverage mask.
        void StrokeFigures(const std::vector<PathData::Figure>& Figures,
                           const Brush& Paint,
                           const StrokeStyle& Stroke,
                           const Matrix3x2& Transform,
                           uint64_t CacheId);
        void FillFigures(const std::vector<PathData::Figure>& Figures,
                         FillRule Rule,
                         const Brush& Paint,
                         const Matrix3x2& Transform,
                         uint64_t CacheId);
        /// @brief Rasterizes local-space Contours (built lazily by Build, so cache hits skip it) and draws them.
        template<class BuildContours>
        void DrawCoverage(BuildContours&& Build,
                          FillRule Rule,
                          const Brush& Paint,
                          const Matrix3x2& Transform,
                          uint64_t CacheId,
                          uint64_t StyleKey);
        void EmitMask(const AtlasPacker::Slot& Slot,
                      const PixelBox& Box,
                      const Paint& Fill,
                      const Matrix3x2& Inverse);
        std::optional<AtlasPacker::Slot> AllocateMask(const PixelBox& Box);

        void DrawText(const TextLayoutData& Layout, const Vec2& Origin, const Brush& Paint, const Matrix3x2& Transform);
        void DrawImage(const ImageData& Source,
                       const Rect& Destination,
                       const std::optional<Rect>& SourceRect,
                       float Opacity,
                       ImageInterpolation Interpolation,
                       const Matrix3x2& Transform);
        void BindImage(uint32_t Texture, ImageInterpolation Interpolation);

        size_t AcquireLayer(uint32_t Width, uint32_t Height);

        GlyphCache& _Glyphs;
        uint64_t _Generation;

        uint32_t _Program     = 0;
        uint32_t _VertexArray = 0;
        uint32_t _Vertices    = 0;
        uint32_t _Indices     = 0;
        int32_t _ViewportUniform = -1;
        std::vector<Vertex> _Pending;
        /// @brief Where the next batch goes in the vertex buffer, in vertices.
        size_t _VertexCursor = 0;

        std::vector<Target> _Targets;
        std::vector<LayerTexture> _LayerPool;
        std::vector<OpenLayer> _OpenLayers;

        GLTexture _MaskAtlas;
        AtlasPacker _MaskPacker;
        std::unordered_map<MaskKey, CachedMask, MaskKeyHash> _MaskCache;
        std::vector<uint8_t> _MaskScratch;

        GLTexture _GradientAtlas;
        std::unordered_map<GradientKey, uint32_t, GradientKeyHash> _GradientRows;

        uint32_t _BoundImage                    = 0;
        ImageInterpolation _BoundInterpolation = ImageInterpolation::Linear;
    };
}  // namespace XenUI
