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

#include "XenUI/Graphics/GL/GLRenderer.hpp"
#include "XenUI/Graphics/DisplayList.hpp"
#include "XenUI/Graphics/GL/FontLibrary.hpp"
#include "XenUI/Graphics/GL/GLApi.hpp"
#include "XenUI/Graphics/GL/GLShader.hpp"
#include "XenUI/Graphics/GL/GlyphCache.hpp"
#include "XenUI/Graphics/GL/ImageData.hpp"
#include "XenUI/Graphics/GL/PathData.hpp"
#include "XenUI/Graphics/GL/TextLayoutData.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <numbers>

namespace XenUI {
    namespace {
        constexpr size_t kMaxQuads          = 16384;
        constexpr uint32_t kMaskAtlasSize   = 2048;
        constexpr int32_t kMaxMaskTile      = 1024;
        constexpr int32_t kMaxCachedMask    = 256;
        constexpr uint32_t kGradientWidth   = 256;
        constexpr uint32_t kGradientRows    = 256;
        constexpr int kMaskFractionSteps    = 8;
        constexpr float kCurveTolerancePx   = 0.1f;

        constexpr GL::GLenum kGlyphUnit    = 0;
        constexpr GL::GLenum kMaskUnit     = 1;
        constexpr GL::GLenum kImageUnit    = 2;
        constexpr GL::GLenum kGradientUnit = 3;
        constexpr GL::GLenum kScratchUnit  = 4;

        const char* const kVertexShader = R"(#version 330 core
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aShape;
layout(location = 2) in vec2 aBrush;
layout(location = 3) in vec4 aShapeParams;
layout(location = 4) in vec4 aColor;
layout(location = 5) in vec4 aBrushParams;
layout(location = 6) in vec4 aBrushExtra;
layout(location = 7) in vec2 aKind;

uniform vec2 uViewport;

out vec2 vShape;
out vec2 vBrush;
flat out vec4 vShapeParams;
flat out vec4 vColor;
flat out vec4 vBrushParams;
flat out vec4 vBrushExtra;
flat out vec2 vKind;

void main() {
    gl_Position  = vec4(aPosition.x / uViewport.x * 2.0 - 1.0, 1.0 - aPosition.y / uViewport.y * 2.0, 0.0, 1.0);
    vShape       = aShape;
    vBrush       = aBrush;
    vShapeParams = aShapeParams;
    vColor       = aColor;
    vBrushParams = aBrushParams;
    vBrushExtra  = aBrushExtra;
    vKind        = aKind;
}
)";

        // Shape kinds and brush kinds match GLRenderer::ShapeKind and ResolveBrush.
        const char* const kFragmentShader = R"(#version 330 core
in vec2 vShape;
in vec2 vBrush;
flat in vec4 vShapeParams;
flat in vec4 vColor;
flat in vec4 vBrushParams;
flat in vec4 vBrushExtra;
flat in vec2 vKind;

uniform sampler2D uGlyphs;
uniform sampler2D uMasks;
uniform sampler2D uImage;
uniform sampler2D uGradients;

layout(location = 0) out vec4 oColor;

float PixelSize;

// Exact area of a pixel centred at C covered by the band [-H, H], both in shape units.
float Cover1D(float C, float H) {
    float P = PixelSize * 0.5;
    return clamp((min(C + P, H) - max(C - P, -H)) / PixelSize, 0.0, 1.0);
}

float CoverBox(vec2 P, vec2 H) {
    return Cover1D(P.x, H.x) * Cover1D(P.y, H.y);
}

float CoverDistance(float D) {
    return clamp(0.5 - D / PixelSize, 0.0, 1.0);
}

float RoundedBoxDistance(vec2 P, vec2 H, float R) {
    R = clamp(R, 0.0, min(H.x, H.y));
    vec2 Q = abs(P) - H + R;
    return length(max(Q, 0.0)) + min(max(Q.x, Q.y), 0.0) - R;
}

float CoverRoundedBox(vec2 P, vec2 H, float R) {
    if (H.x <= 0.0 || H.y <= 0.0) { return 0.0; }
    return R <= 0.0 ? CoverBox(P, H) : CoverDistance(RoundedBoxDistance(P, H, R));
}

float CoverEllipse(vec2 P, vec2 R) {
    if (R.x <= 0.0 || R.y <= 0.0) { return 0.0; }
    float K0 = length(P / R);
    float K1 = length(P / (R * R));
    return CoverDistance(K1 > 1e-6 ? K0 * (K0 - 1.0) / K1 : -min(R.x, R.y));
}

vec4 Paint() {
    if (vKind.y < 0.5) { return vColor; }

    float T;
    if (vKind.y < 1.5) {
        vec2 Axis = vBrushParams.zw - vBrushParams.xy;
        T = dot(vBrush - vBrushParams.xy, Axis) / max(dot(Axis, Axis), 1e-12);
    } else {
        vec2 Radius = max(abs(vBrushParams.zw), vec2(1e-6));
        vec2 Q      = (vBrush - vBrushParams.xy) / Radius;
        vec2 F      = vBrushExtra.xy / Radius;
        float FocusLength = length(F);
        if (FocusLength > 0.999) { F *= 0.999 / FocusLength; }
        vec2 Ray   = Q - F;
        float Span = length(Ray);
        if (Span < 1e-6) {
            T = 0.0;
        } else {
            Ray /= Span;
            float B = dot(F, Ray);
            T = Span / (-B + sqrt(max(B * B - dot(F, F) + 1.0, 0.0)));
        }
    }

    float Extend = vBrushExtra.w;
    if (Extend > 1.5) {
        T = 1.0 - abs(mod(T, 2.0) - 1.0);
    } else if (Extend > 0.5) {
        T = fract(T);
    } else {
        T = clamp(T, 0.0, 1.0);
    }
    return texture(uGradients, vec2((T * 255.0 + 0.5) / 256.0, vBrushExtra.z)) * vColor.a;
}

void main() {
    vec2 Dx = dFdx(vShape);
    vec2 Dy = dFdy(vShape);
    PixelSize = max(sqrt(0.5 * (dot(Dx, Dx) + dot(Dy, Dy))), 1e-6);

    int Kind  = int(vKind.x + 0.5);
    vec4 P    = vShapeParams;
    float Coverage = 1.0;

    if (Kind == 1) {
        Coverage = CoverRoundedBox(vShape, P.xy, P.z);
    } else if (Kind == 2) {
        float Outer = CoverRoundedBox(vShape, P.xy + P.w, P.z > 0.0 ? P.z + P.w : 0.0);
        float Inner = CoverRoundedBox(vShape, P.xy - P.w, max(P.z - P.w, 0.0));
        Coverage = max(Outer - Inner, 0.0);
    } else if (Kind == 3) {
        Coverage = CoverEllipse(vShape, P.xy);
    } else if (Kind == 4) {
        Coverage = max(CoverEllipse(vShape, P.xy + P.z) - CoverEllipse(vShape, P.xy - P.z), 0.0);
    } else if (Kind == 5) {
        float Length = P.x;
        float Half   = P.y;
        if (P.z > 1.5) {
            Coverage = CoverDistance(length(vec2(vShape.x - clamp(vShape.x, 0.0, Length), vShape.y)) - Half);
        } else {
            float Extension = P.z > 0.5 ? Half : 0.0;
            Coverage = CoverBox(vShape - vec2(Length * 0.5, 0.0), vec2(Length * 0.5 + Extension, Half));
        }
    } else if (Kind == 6) {
        Coverage = texture(uMasks, vShape).r;
    } else if (Kind == 7) {
        Coverage = texture(uGlyphs, vShape).a;
    } else if (Kind == 8) {
        oColor = texture(uGlyphs, vShape) * Paint().a;
        return;
    } else if (Kind == 9) {
        oColor = texture(uImage, vShape) * vColor.a;
        return;
    }

    oColor = Paint() * Coverage;
}
)";

        void HashCombine(size_t& Seed, size_t Value) {
            Seed ^= Value + 0x9E3779B97F4A7C15ull + (Seed << 6) + (Seed >> 2);
        }

        void HashFloat(size_t& Seed, float Value) {
            HashCombine(Seed, std::bit_cast<uint32_t>(Value));
        }

        uint64_t HashStroke(const StrokeStyle& Stroke) {
            size_t Seed = 0x5354524F4B45ull;
            HashFloat(Seed, Stroke.Width);
            HashCombine(Seed, static_cast<size_t>(Stroke.Cap));
            HashCombine(Seed, static_cast<size_t>(Stroke.Join));
            HashFloat(Seed, Stroke.MiterLimit);
            HashFloat(Seed, Stroke.DashOffset);
            for (const float Dash : Stroke.Dashes) {
                HashFloat(Seed, Dash);
            }
            return Seed | 1u;
        }

        /// @brief The smallest factor by which Transform stretches any direction.
        float GetMinScale(const Matrix3x2& Transform) {
            const float Sum  = Transform.M11 * Transform.M11 + Transform.M12 * Transform.M12 +
                              Transform.M21 * Transform.M21 + Transform.M22 * Transform.M22;
            const float Det  = std::abs(Transform.Determinant());
            const float Root = std::sqrt(std::max(Sum * Sum - 4.0f * Det * Det, 0.0f));
            return std::sqrt(std::max((Sum - Root) * 0.5f, 0.0f));
        }

        Rect Normalized(const Rect& Bounds) {
            return Rect::FromEdges(std::min(Bounds.Left(), Bounds.Right()),
                                   std::min(Bounds.Top(), Bounds.Bottom()),
                                   std::max(Bounds.Left(), Bounds.Right()),
                                   std::max(Bounds.Top(), Bounds.Bottom()));
        }

        PixelBox Intersect(const PixelBox& A, const PixelBox& B) {
            const int32_t Left   = std::max(A.X, B.X);
            const int32_t Top    = std::max(A.Y, B.Y);
            const int32_t Right  = std::min(A.X + A.Width, B.X + B.Width);
            const int32_t Bottom = std::min(A.Y + A.Height, B.Y + B.Height);
            return {Left, Top, std::max(0, Right - Left), std::max(0, Bottom - Top)};
        }

        bool Contains(const PixelBox& Outer, const PixelBox& Inner) {
            return Inner.X >= Outer.X && Inner.Y >= Outer.Y && Inner.X + Inner.Width <= Outer.X + Outer.Width &&
                   Inner.Y + Inner.Height <= Outer.Y + Outer.Height;
        }

        int ArcSteps(float Radius, float Sweep, float Tolerance) {
            if (Radius <= Tolerance) { return 1; }
            const float Step = 2.0f * std::acos(std::clamp(1.0f - Tolerance / Radius, -1.0f, 1.0f));
            return std::clamp(static_cast<int>(std::ceil(Sweep / std::max(Step, 1e-3f))), 1, 256);
        }

        void AppendArc(std::vector<Vec2>& Points,
                       const Vec2& Center,
                       const Vec2& Radius,
                       float From,
                       float Sweep,
                       float Tolerance) {
            const int Steps = ArcSteps(std::max(Radius.X, Radius.Y), Sweep, Tolerance);
            for (int I = 0; I <= Steps; ++I) {
                const float Angle = From + Sweep * static_cast<float>(I) / static_cast<float>(Steps);
                Points.push_back({Center.X + std::cos(Angle) * Radius.X, Center.Y + std::sin(Angle) * Radius.Y});
            }
        }

        std::vector<PathData::Figure> RoundedRectFigure(const Rect& Bounds, float Radius, float Tolerance) {
            const Rect Box = Normalized(Bounds);
            const float R  = std::clamp(Radius, 0.0f, std::min(Box.Width, Box.Height) * 0.5f);
            PathData::Figure Shape;
            Shape.Closed = true;
            if (R <= 0.0f) {
                Shape.Points = {{Box.Left(), Box.Top()},
                                {Box.Right(), Box.Top()},
                                {Box.Right(), Box.Bottom()},
                                {Box.Left(), Box.Bottom()}};
                return {Shape};
            }
            constexpr float kQuarter = std::numbers::pi_v<float> * 0.5f;
            AppendArc(Shape.Points, {Box.Left() + R, Box.Top() + R}, {R, R}, 2.0f * kQuarter, kQuarter, Tolerance);
            AppendArc(Shape.Points, {Box.Right() - R, Box.Top() + R}, {R, R}, 3.0f * kQuarter, kQuarter, Tolerance);
            AppendArc(Shape.Points, {Box.Right() - R, Box.Bottom() - R}, {R, R}, 0.0f, kQuarter, Tolerance);
            AppendArc(Shape.Points, {Box.Left() + R, Box.Bottom() - R}, {R, R}, kQuarter, kQuarter, Tolerance);
            return {Shape};
        }

        std::vector<PathData::Figure> EllipseFigure(const Vec2& Center, const Vec2& Radius, float Tolerance) {
            PathData::Figure Shape;
            Shape.Closed = true;
            AppendArc(Shape.Points,
                      Center,
                      {std::abs(Radius.X), std::abs(Radius.Y)},
                      -std::numbers::pi_v<float> * 0.5f,
                      2.0f * std::numbers::pi_v<float>,
                      Tolerance);
            Shape.Points.pop_back();
            return {Shape};
        }

        std::vector<Contour> ToContours(const std::vector<PathData::Figure>& Figures) {
            std::vector<Contour> Result;
            Result.reserve(Figures.size());
            for (const PathData::Figure& Shape : Figures) {
                if (Shape.Points.size() >= 3) { Result.push_back(Shape.Points); }
            }
            return Result;
        }

        /// @brief Corners of Bounds in drawing order: top-left, top-right, bottom-right, bottom-left.
        void GetCorners(const Rect& Bounds, Vec2 (&Corners)[4]) {
            Corners[0] = {Bounds.Left(), Bounds.Top()};
            Corners[1] = {Bounds.Right(), Bounds.Top()};
            Corners[2] = {Bounds.Right(), Bounds.Bottom()};
            Corners[3] = {Bounds.Left(), Bounds.Bottom()};
        }
    }  // namespace

    size_t GLRenderer::MaskKeyHash::operator()(const MaskKey& Key) const noexcept {
        size_t Seed = Key.Path;
        HashCombine(Seed, Key.Style);
        HashFloat(Seed, Key.M11);
        HashFloat(Seed, Key.M12);
        HashFloat(Seed, Key.M21);
        HashFloat(Seed, Key.M22);
        HashCombine(Seed, static_cast<size_t>(Key.FractionX));
        HashCombine(Seed, static_cast<size_t>(Key.FractionY));
        return Seed;
    }

    size_t GLRenderer::GradientKeyHash::operator()(const GradientKey& Key) const noexcept {
        size_t Seed = Key.Stops.size();
        for (const GradientStop& Stop : Key.Stops) {
            HashFloat(Seed, Stop.Position);
            HashFloat(Seed, Stop.Value.R);
            HashFloat(Seed, Stop.Value.G);
            HashFloat(Seed, Stop.Value.B);
            HashFloat(Seed, Stop.Value.A);
        }
        return Seed;
    }

    GLRenderer::GLRenderer(GlyphCache& Glyphs, uint64_t Generation)
        : _Glyphs(Glyphs), _Generation(Generation), _MaskPacker(kMaskAtlasSize, kMaskAtlasSize) {
        _Program         = CompileProgram(kVertexShader, kFragmentShader, "XenUI renderer");
        _ViewportUniform = GL::Api.GetUniformLocation(_Program, "uViewport");
        GL::Api.UseProgram(_Program);
        GL::Api.Uniform1i(GL::Api.GetUniformLocation(_Program, "uGlyphs"), static_cast<GL::GLint>(kGlyphUnit));
        GL::Api.Uniform1i(GL::Api.GetUniformLocation(_Program, "uMasks"), static_cast<GL::GLint>(kMaskUnit));
        GL::Api.Uniform1i(GL::Api.GetUniformLocation(_Program, "uImage"), static_cast<GL::GLint>(kImageUnit));
        GL::Api.Uniform1i(GL::Api.GetUniformLocation(_Program, "uGradients"), static_cast<GL::GLint>(kGradientUnit));

        GL::Api.GenVertexArrays(1, &_VertexArray);
        GL::Api.BindVertexArray(_VertexArray);
        GL::Api.GenBuffers(1, &_Vertices);
        GL::Api.BindBuffer(GL::ARRAY_BUFFER, _Vertices);
        GL::Api.BufferData(GL::ARRAY_BUFFER,
                           static_cast<GL::GLsizeiptr>(kMaxQuads * 4 * sizeof(Vertex)),
                           nullptr,
                           GL::STREAM_DRAW);

        struct Attribute {
            GL::GLint Size;
            size_t Offset;
        };
        const Attribute Attributes[] = {
          {2, offsetof(Vertex, Position)},
          {2, offsetof(Vertex, Shape)},
          {2, offsetof(Vertex, BrushPosition)},
          {4, offsetof(Vertex, ShapeParams)},
          {4, offsetof(Vertex, Color)},
          {4, offsetof(Vertex, BrushParams)},
          {4, offsetof(Vertex, BrushExtra)},
          {2, offsetof(Vertex, Kind)},
        };
        for (GL::GLuint Index = 0; Index < std::size(Attributes); ++Index) {
            GL::Api.EnableVertexAttribArray(Index);
            GL::Api.VertexAttribPointer(Index,
                                        Attributes[Index].Size,
                                        GL::FLOAT,
                                        0,
                                        static_cast<GL::GLsizei>(sizeof(Vertex)),
                                        reinterpret_cast<const void*>(Attributes[Index].Offset));
        }

        std::vector<uint16_t> Indices(kMaxQuads * 6);
        for (size_t Quad = 0; Quad < kMaxQuads; ++Quad) {
            const auto Base      = static_cast<uint16_t>(Quad * 4);
            const uint16_t Order[] = {0, 1, 2, 0, 2, 3};
            for (size_t I = 0; I < 6; ++I) {
                Indices[Quad * 6 + I] = static_cast<uint16_t>(Base + Order[I]);
            }
        }
        GL::Api.GenBuffers(1, &_Indices);
        GL::Api.BindBuffer(GL::ELEMENT_ARRAY_BUFFER, _Indices);
        GL::Api.BufferData(GL::ELEMENT_ARRAY_BUFFER,
                           static_cast<GL::GLsizeiptr>(Indices.size() * sizeof(uint16_t)),
                           Indices.data(),
                           GL::STATIC_DRAW);
        GL::Api.BindVertexArray(0);

        const auto CreateTexture = [&](GL::GLenum Unit, uint32_t Width, uint32_t Height, GL::GLenum Internal,
                                       GL::GLenum Format) {
            GL::GLuint Name = 0;
            GL::Api.GenTextures(1, &Name);
            GL::Api.ActiveTexture(GL::TEXTURE0 + Unit);
            GL::Api.BindTexture(GL::TEXTURE_2D, Name);
            GL::Api.TexImage2D(GL::TEXTURE_2D,
                               0,
                               static_cast<GL::GLint>(Internal),
                               static_cast<GL::GLsizei>(Width),
                               static_cast<GL::GLsizei>(Height),
                               0,
                               Format,
                               GL::UNSIGNED_BYTE,
                               nullptr);
            GL::Api.TexParameteri(GL::TEXTURE_2D, GL::TEXTURE_MIN_FILTER, static_cast<GL::GLint>(GL::LINEAR));
            GL::Api.TexParameteri(GL::TEXTURE_2D, GL::TEXTURE_MAG_FILTER, static_cast<GL::GLint>(GL::LINEAR));
            GL::Api.TexParameteri(GL::TEXTURE_2D, GL::TEXTURE_WRAP_S, static_cast<GL::GLint>(GL::CLAMP_TO_EDGE));
            GL::Api.TexParameteri(GL::TEXTURE_2D, GL::TEXTURE_WRAP_T, static_cast<GL::GLint>(GL::CLAMP_TO_EDGE));
            return GLTexture(Name, Generation);
        };
        _MaskAtlas     = CreateTexture(kMaskUnit, kMaskAtlasSize, kMaskAtlasSize, GL::R8, GL::RED);
        _GradientAtlas = CreateTexture(kGradientUnit, kGradientWidth, kGradientRows, GL::RGBA8, GL::RGBA);
        GL::Api.ActiveTexture(GL::TEXTURE0);
    }

    // The program, buffers and vertex array go with the context; the device is torn down with it at exit.
    GLRenderer::~GLRenderer() = default;

    void GLRenderer::BeginTarget(uint32_t Framebuffer, uint32_t Width, uint32_t Height) {
        if (!_Targets.empty()) { Flush(); }
        Target Next;
        Next.Framebuffer = Framebuffer;
        Next.Width       = Width;
        Next.Height      = Height;
        Next.Scissors.push_back({0, 0, static_cast<int32_t>(Width), static_cast<int32_t>(Height)});
        _Targets.push_back(std::move(Next));

        // Other GL code (the live renderer) may have run since the last target, so restore everything.
        GL::Api.UseProgram(_Program);
        GL::Api.BindVertexArray(_VertexArray);
        GL::Api.BindBuffer(GL::ARRAY_BUFFER, _Vertices);
        GL::Api.Disable(GL::DEPTH_TEST);
        GL::Api.Disable(GL::STENCIL_TEST);
        GL::Api.Disable(GL::CULL_FACE);
        GL::Api.Enable(GL::BLEND);
        GL::Api.BlendFunc(GL::ONE, GL::ONE_MINUS_SRC_ALPHA);
        GL::Api.ActiveTexture(GL::TEXTURE0 + kGlyphUnit);
        GL::Api.BindTexture(GL::TEXTURE_2D, _Glyphs.GetTexture());
        GL::Api.ActiveTexture(GL::TEXTURE0 + kMaskUnit);
        GL::Api.BindTexture(GL::TEXTURE_2D, _MaskAtlas.Get());
        GL::Api.ActiveTexture(GL::TEXTURE0 + kGradientUnit);
        GL::Api.BindTexture(GL::TEXTURE_2D, _GradientAtlas.Get());
        GL::Api.ActiveTexture(GL::TEXTURE0 + kImageUnit);
        GL::Api.BindTexture(GL::TEXTURE_2D, _BoundImage);
        ApplyTarget();
        ApplyScissor();
    }

    void GLRenderer::EndTarget() {
        Flush();
        while (!Current().Layers.empty()) {
            PopOpacity();
        }
        Flush();
        _Targets.pop_back();
        if (!_Targets.empty()) {
            ApplyTarget();
            ApplyScissor();
        }
    }

    void GLRenderer::ApplyTarget() {
        const Target& Bound = Current();
        GL::Api.BindFramebuffer(GL::FRAMEBUFFER, Bound.Framebuffer);
        GL::Api.Viewport(0, 0, static_cast<GL::GLsizei>(Bound.Width), static_cast<GL::GLsizei>(Bound.Height));
        GL::Api.Uniform2f(_ViewportUniform, static_cast<float>(Bound.Width), static_cast<float>(Bound.Height));
    }

    void GLRenderer::ApplyScissor() {
        const Target& Bound = Current();
        const PixelBox& Box = Bound.Scissors.back();
        GL::Api.Enable(GL::SCISSOR_TEST);
        GL::Api.Scissor(Box.X,
                        static_cast<GL::GLint>(Bound.Height) - (Box.Y + Box.Height),
                        std::max(0, Box.Width),
                        std::max(0, Box.Height));
    }

    void GLRenderer::Clear(const Color& Fill) {
        Flush();
        GL::Api.ClearColor(Fill.R * Fill.A, Fill.G * Fill.A, Fill.B * Fill.A, Fill.A);
        GL::Api.Clear(GL::COLOR_BUFFER_BIT);
    }

    void GLRenderer::PushScissor(const Rect& Area) {
        Flush();
        const auto Round = [](float Value) {
            return static_cast<int32_t>(std::floor(std::clamp(Value, -1e7f, 1e7f) + 0.5f));
        };
        const int32_t Left   = Round(Area.Left());
        const int32_t Top    = Round(Area.Top());
        const PixelBox Box {Left, Top, Round(Area.Right()) - Left, Round(Area.Bottom()) - Top};
        Current().Scissors.push_back(Intersect(GetScissor(), Box));
        ApplyScissor();
    }

    void GLRenderer::PopScissor() {
        if (Current().Scissors.size() <= 1) { return; }
        Flush();
        Current().Scissors.pop_back();
        ApplyScissor();
    }

    void GLRenderer::PushClip(const Rect& Local, const Matrix3x2& Transform) {
        PushScissor(Transform.TransformRect(Normalized(Local)));
    }

    size_t GLRenderer::AcquireLayer(uint32_t Width, uint32_t Height) {
        for (size_t I = 0; I < _LayerPool.size(); ++I) {
            LayerTexture& Entry = _LayerPool[I];
            if (!Entry.InUse && Entry.Width == Width && Entry.Height == Height) {
                Entry.InUse = true;
                return I;
            }
        }
        // Drop unused layers of other sizes, so a resized window doesn't keep its old layers alive.
        for (LayerTexture& Entry : _LayerPool) {
            if (!Entry.InUse) {
                Entry.Framebuffer.Reset();
                Entry.Texture.Reset();
            }
        }

        LayerTexture Entry;
        GL::GLuint Texture     = 0;
        GL::GLuint Framebuffer = 0;
        GL::Api.GenTextures(1, &Texture);
        GL::Api.ActiveTexture(GL::TEXTURE0 + kScratchUnit);
        GL::Api.BindTexture(GL::TEXTURE_2D, Texture);
        GL::Api.TexImage2D(GL::TEXTURE_2D,
                           0,
                           static_cast<GL::GLint>(GL::RGBA8),
                           static_cast<GL::GLsizei>(Width),
                           static_cast<GL::GLsizei>(Height),
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
        Entry.Texture     = GLTexture(Texture, _Generation);
        Entry.Framebuffer = GLFramebuffer(Framebuffer, _Generation);
        Entry.Width       = Width;
        Entry.Height      = Height;
        Entry.InUse       = true;

        // Open layers are in use, so they survive; PushOpacity finds their new indices by texture name.
        std::erase_if(_LayerPool, [](const LayerTexture& Old) { return !Old.Texture; });
        _LayerPool.push_back(std::move(Entry));
        return _LayerPool.size() - 1;
    }

    void GLRenderer::PushOpacity(float Opacity) {
        Flush();
        Target& Bound = Current();
        const PixelBox Area = GetScissor();

        // Indices into the pool move when unused entries are dropped, so remember open layers by texture.
        std::vector<uint32_t> OpenTextures;
        for (const OpenLayer& Open : _OpenLayers) {
            OpenTextures.push_back(_LayerPool[Open.Texture].Texture.Get());
        }
        const size_t Index = AcquireLayer(Bound.Width, Bound.Height);
        for (size_t I = 0; I < _OpenLayers.size(); ++I) {
            for (size_t J = 0; J < _LayerPool.size(); ++J) {
                if (_LayerPool[J].Texture.Get() == OpenTextures[I]) { _OpenLayers[I].Texture = J; }
            }
        }

        _OpenLayers.push_back({Index, std::clamp(Opacity, 0.0f, 1.0f), Area, Bound.Framebuffer});
        Bound.Layers.push_back(_OpenLayers.size() - 1);
        Bound.Framebuffer = _LayerPool[Index].Framebuffer.Get();
        ApplyTarget();
        ApplyScissor();
        GL::Api.ClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        GL::Api.Clear(GL::COLOR_BUFFER_BIT);
    }

    void GLRenderer::PopOpacity() {
        Target& Bound = Current();
        if (Bound.Layers.empty()) { return; }
        Flush();

        const OpenLayer Layer = _OpenLayers.back();
        _OpenLayers.pop_back();
        Bound.Layers.pop_back();
        Bound.Framebuffer = Layer.ParentFramebuffer;
        ApplyTarget();
        ApplyScissor();

        LayerTexture& Source = _LayerPool[Layer.Texture];
        if (!Layer.Area.IsEmpty() && Layer.Opacity > 0.0f) {
            const auto Width  = static_cast<float>(Source.Width);
            const auto Height = static_cast<float>(Source.Height);
            const Rect Area {static_cast<float>(Layer.Area.X),
                             static_cast<float>(Layer.Area.Y),
                             static_cast<float>(Layer.Area.Width),
                             static_cast<float>(Layer.Area.Height)};
            DrawTexture(Source.Texture.Get(),
                        Area,
                        {Area.X / Width, Area.Y / Height, Area.Width / Width, Area.Height / Height},
                        Matrix3x2::Identity(),
                        Layer.Opacity,
                        ImageInterpolation::NearestNeighbor,
                        true);
            Flush();
        }
        Source.InUse = false;
    }

    GLRenderer::Paint GLRenderer::ResolveBrush(const Brush& Source, float Opacity) {
        Paint Result;
        if (const auto* Solid = std::get_if<Color>(&Source)) {
            const float Alpha = Solid->A * Opacity;
            Result.Color[0]   = Solid->R * Alpha;
            Result.Color[1]   = Solid->G * Alpha;
            Result.Color[2]   = Solid->B * Alpha;
            Result.Color[3]   = Alpha;
            return Result;
        }

        Result.Color[0] = Result.Color[1] = Result.Color[2] = 1.0f;
        Result.Color[3]                                     = Opacity;
        if (const auto* Linear = std::get_if<LinearGradient>(&Source)) {
            Result.Kind      = 1.0f;
            Result.Params[0] = Linear->Start.X;
            Result.Params[1] = Linear->Start.Y;
            Result.Params[2] = Linear->End.X;
            Result.Params[3] = Linear->End.Y;
            Result.Extra[2]  = GetGradientRow(Linear->Stops);
            Result.Extra[3]  = static_cast<float>(Linear->Extend);
            return Result;
        }

        const auto& Radial = std::get<RadialGradient>(Source);
        Result.Kind        = 2.0f;
        Result.Params[0]   = Radial.Center.X;
        Result.Params[1]   = Radial.Center.Y;
        Result.Params[2]   = Radial.Radius.X;
        Result.Params[3]   = Radial.Radius.Y;
        Result.Extra[0]    = Radial.FocusOffset.X;
        Result.Extra[1]    = Radial.FocusOffset.Y;
        Result.Extra[2]    = GetGradientRow(Radial.Stops);
        Result.Extra[3]    = static_cast<float>(Radial.Extend);
        return Result;
    }

    float GLRenderer::GetGradientRow(const std::vector<GradientStop>& Stops) {
        GradientKey Key {Stops};
        if (const auto It = _GradientRows.find(Key); It != _GradientRows.end()) {
            return (static_cast<float>(It->second) + 0.5f) / static_cast<float>(kGradientRows);
        }
        if (_GradientRows.size() >= kGradientRows) {
            Flush();
            _GradientRows.clear();
        }
        const auto Row = static_cast<uint32_t>(_GradientRows.size());

        std::vector<GradientStop> Sorted = Stops;
        std::stable_sort(Sorted.begin(), Sorted.end(), [](const GradientStop& A, const GradientStop& B) {
            return A.Position < B.Position;
        });
        uint8_t Texels[kGradientWidth * 4] {};
        for (uint32_t I = 0; I < kGradientWidth; ++I) {
            const float T = static_cast<float>(I) / static_cast<float>(kGradientWidth - 1);
            Color Value {0.0f, 0.0f, 0.0f, 0.0f};
            if (!Sorted.empty()) {
                if (T <= Sorted.front().Position) {
                    Value = Sorted.front().Value;
                } else if (T >= Sorted.back().Position) {
                    Value = Sorted.back().Value;
                } else {
                    for (size_t S = 1; S < Sorted.size(); ++S) {
                        if (T <= Sorted[S].Position) {
                            const float Span = Sorted[S].Position - Sorted[S - 1].Position;
                            const float Amount = Span > 0.0f ? (T - Sorted[S - 1].Position) / Span : 1.0f;
                            Value              = Sorted[S - 1].Value.Lerp(Sorted[S].Value, Amount);
                            break;
                        }
                    }
                }
            }
            const float Alpha = std::clamp(Value.A, 0.0f, 1.0f);
            Texels[I * 4 + 0] = static_cast<uint8_t>(std::clamp(Value.R, 0.0f, 1.0f) * Alpha * 255.0f + 0.5f);
            Texels[I * 4 + 1] = static_cast<uint8_t>(std::clamp(Value.G, 0.0f, 1.0f) * Alpha * 255.0f + 0.5f);
            Texels[I * 4 + 2] = static_cast<uint8_t>(std::clamp(Value.B, 0.0f, 1.0f) * Alpha * 255.0f + 0.5f);
            Texels[I * 4 + 3] = static_cast<uint8_t>(Alpha * 255.0f + 0.5f);
        }
        GL::Api.ActiveTexture(GL::TEXTURE0 + kGradientUnit);
        GL::Api.BindTexture(GL::TEXTURE_2D, _GradientAtlas.Get());
        GL::Api.PixelStorei(GL::UNPACK_ALIGNMENT, 4);
        GL::Api.TexSubImage2D(GL::TEXTURE_2D,
                              0,
                              0,
                              static_cast<GL::GLint>(Row),
                              static_cast<GL::GLsizei>(kGradientWidth),
                              1,
                              GL::RGBA,
                              GL::UNSIGNED_BYTE,
                              Texels);
        _GradientRows.emplace(std::move(Key), Row);
        return (static_cast<float>(Row) + 0.5f) / static_cast<float>(kGradientRows);
    }

    void GLRenderer::EmitQuad(const Vec2 (&Device)[4],
                              const Vec2 (&Shape)[4],
                              const Vec2 (&BrushPosition)[4],
                              const float (&ShapeParams)[4],
                              ShapeKind Kind,
                              const Paint& Fill) {
        if (_Pending.size() + 4 > kMaxQuads * 4) { Flush(); }
        for (int I = 0; I < 4; ++I) {
            Vertex& Out          = _Pending.emplace_back();
            Out.Position[0]      = Device[I].X;
            Out.Position[1]      = Device[I].Y;
            Out.Shape[0]         = Shape[I].X;
            Out.Shape[1]         = Shape[I].Y;
            Out.BrushPosition[0] = BrushPosition[I].X;
            Out.BrushPosition[1] = BrushPosition[I].Y;
            std::copy_n(ShapeParams, 4, Out.ShapeParams);
            std::copy_n(Fill.Color, 4, Out.Color);
            std::copy_n(Fill.Params, 4, Out.BrushParams);
            std::copy_n(Fill.Extra, 4, Out.BrushExtra);
            Out.Kind[0] = static_cast<float>(Kind);
            Out.Kind[1] = Fill.Kind;
        }
    }

    void GLRenderer::EmitShape(const Rect& Bounds,
                               const Vec2& Center,
                               const float (&ShapeParams)[4],
                               ShapeKind Kind,
                               const Paint& Fill,
                               const Matrix3x2& Transform,
                               float Margin) {
        const float MinScale = GetMinScale(Transform);
        if (MinScale <= 1e-6f) { return; }
        const Rect Covered = Normalized(Bounds).Inflated(Margin + 1.0f / MinScale);

        Vec2 Local[4];
        GetCorners(Covered, Local);
        Vec2 Device[4];
        Vec2 Shape[4];
        for (int I = 0; I < 4; ++I) {
            Device[I] = Transform.TransformPoint(Local[I]);
            Shape[I]  = Local[I] - Center;
        }
        EmitQuad(Device, Shape, Local, ShapeParams, Kind, Fill);
    }

    void GLRenderer::FillRect(const Rect& Bounds, const Brush& Source, const Matrix3x2& Transform) {
        DrawRoundedRect(Bounds, 0.0f, Source, Transform);
    }

    void GLRenderer::DrawRoundedRect(const Rect& Bounds,
                                     float Radius,
                                     const Brush& Source,
                                     const Matrix3x2& Transform) {
        const Rect Box = Normalized(Bounds);
        if (Box.IsEmpty()) { return; }
        const float Params[4] = {Box.Width * 0.5f, Box.Height * 0.5f, std::max(Radius, 0.0f), 0.0f};
        const Vec2 Center {Box.X + Box.Width * 0.5f, Box.Y + Box.Height * 0.5f};
        EmitShape(Box, Center, Params, ShapeKind::RoundedRect, ResolveBrush(Source), Transform, 0.0f);
    }

    void GLRenderer::StrokeRect(const Rect& Bounds,
                                const Brush& Source,
                                const StrokeStyle& Stroke,
                                const Matrix3x2& Transform) {
        // Corners are right angles, so a miter join stays sharp unless the limit is under sqrt(2).
        const bool SharpCorners = (Stroke.Join == LineJoin::Miter || Stroke.Join == LineJoin::MiterOrBevel) &&
                                  Stroke.MiterLimit >= std::numbers::sqrt2_v<float>;
        if (!Stroke.Dashes.empty() || !SharpCorners) {
            StrokeFigures(RoundedRectFigure(Bounds, 0.0f, 0.0f), Source, Stroke, Transform, 0);
            return;
        }
        StrokeRoundedRect(Bounds, 0.0f, Source, Stroke, Transform);
    }

    void GLRenderer::StrokeRoundedRect(const Rect& Bounds,
                                       float Radius,
                                       const Brush& Source,
                                       const StrokeStyle& Stroke,
                                       const Matrix3x2& Transform) {
        if (Stroke.Width <= 0.0f) { return; }
        if (!Stroke.Dashes.empty()) {
            const float Tolerance = kCurveTolerancePx / std::max(Transform.GetMaxScale(), 1e-3f);
            StrokeFigures(RoundedRectFigure(Bounds, Radius, Tolerance), Source, Stroke, Transform, 0);
            return;
        }
        const Rect Box = Normalized(Bounds);
        const float Half = Stroke.Width * 0.5f;
        const float R    = std::clamp(Radius, 0.0f, std::min(Box.Width, Box.Height) * 0.5f);
        const float Params[4] = {Box.Width * 0.5f, Box.Height * 0.5f, R, Half};
        const Vec2 Center {Box.X + Box.Width * 0.5f, Box.Y + Box.Height * 0.5f};
        EmitShape(Box, Center, Params, ShapeKind::RoundedFrame, ResolveBrush(Source), Transform, Half);
    }

    void GLRenderer::FillEllipse(const Vec2& Center,
                                 const Vec2& Radius,
                                 const Brush& Source,
                                 const Matrix3x2& Transform) {
        const Vec2 R {std::abs(Radius.X), std::abs(Radius.Y)};
        if (R.X <= 0.0f || R.Y <= 0.0f) { return; }
        const float Params[4] = {R.X, R.Y, 0.0f, 0.0f};
        EmitShape({Center.X - R.X, Center.Y - R.Y, R.X * 2.0f, R.Y * 2.0f},
                  Center,
                  Params,
                  ShapeKind::Ellipse,
                  ResolveBrush(Source),
                  Transform,
                  0.0f);
    }

    void GLRenderer::StrokeEllipse(const Vec2& Center,
                                   const Vec2& Radius,
                                   const Brush& Source,
                                   const StrokeStyle& Stroke,
                                   const Matrix3x2& Transform) {
        if (Stroke.Width <= 0.0f) { return; }
        if (!Stroke.Dashes.empty()) {
            const float Tolerance = kCurveTolerancePx / std::max(Transform.GetMaxScale(), 1e-3f);
            StrokeFigures(EllipseFigure(Center, Radius, Tolerance), Source, Stroke, Transform, 0);
            return;
        }
        const Vec2 R {std::abs(Radius.X), std::abs(Radius.Y)};
        const float Half      = Stroke.Width * 0.5f;
        const float Params[4] = {R.X, R.Y, Half, 0.0f};
        EmitShape({Center.X - R.X, Center.Y - R.Y, R.X * 2.0f, R.Y * 2.0f},
                  Center,
                  Params,
                  ShapeKind::EllipseFrame,
                  ResolveBrush(Source),
                  Transform,
                  Half);
    }

    void GLRenderer::DrawLine(const Vec2& Start,
                              const Vec2& End,
                              const Brush& Source,
                              const StrokeStyle& Stroke,
                              const Matrix3x2& Transform) {
        if (Stroke.Width <= 0.0f) { return; }
        if (!Stroke.Dashes.empty() || Stroke.Cap == LineCap::Triangle) {
            PathData::Figure Segment;
            Segment.Points = {Start, End};
            StrokeFigures({Segment}, Source, Stroke, Transform, 0);
            return;
        }

        const float MinScale = GetMinScale(Transform);
        if (MinScale <= 1e-6f) { return; }
        const Vec2 Delta    = End - Start;
        const float Length  = std::sqrt(Delta.X * Delta.X + Delta.Y * Delta.Y);
        const Vec2 Along    = Length > 1e-6f ? Delta / Length : Vec2 {1.0f, 0.0f};
        const Vec2 Across   = {-Along.Y, Along.X};
        const float Half    = Stroke.Width * 0.5f;
        const float Cap     = Stroke.Cap == LineCap::Round ? 2.0f : Stroke.Cap == LineCap::Square ? 1.0f : 0.0f;
        if (Length <= 1e-6f && Cap == 0.0f) { return; }

        const float Reach   = Half + 1.0f / MinScale;
        const float Before  = Cap > 0.0f ? Reach : 1.0f / MinScale;
        const Vec2 Frame[4] = {{-Before, -Reach}, {Length + Before, -Reach}, {Length + Before, Reach}, {-Before, Reach}};
        Vec2 Local[4];
        Vec2 Device[4];
        for (int I = 0; I < 4; ++I) {
            Local[I]  = Start + Along * Frame[I].X + Across * Frame[I].Y;
            Device[I] = Transform.TransformPoint(Local[I]);
        }
        const float Params[4] = {Length, Half, Cap, 0.0f};
        EmitQuad(Device, Frame, Local, Params, ShapeKind::Line, ResolveBrush(Source));
    }

    void GLRenderer::StrokeFigures(const std::vector<PathData::Figure>& Figures,
                                   const Brush& Source,
                                   const StrokeStyle& Stroke,
                                   const Matrix3x2& Transform,
                                   uint64_t CacheId) {
        if (Stroke.Width <= 0.0f) { return; }
        const float Tolerance = kCurveTolerancePx / std::max(Transform.GetMaxScale(), 1e-3f);
        DrawCoverage([&] { return XenUI::StrokeFigures(Figures, Stroke, Tolerance); },
                     FillRule::NonZero,
                     Source,
                     Transform,
                     CacheId,
                     HashStroke(Stroke));
    }

    void GLRenderer::FillFigures(const std::vector<PathData::Figure>& Figures,
                                 FillRule Rule,
                                 const Brush& Source,
                                 const Matrix3x2& Transform,
                                 uint64_t CacheId) {
        DrawCoverage([&] { return ToContours(Figures); },
                     Rule,
                     Source,
                     Transform,
                     CacheId,
                     Rule == FillRule::EvenOdd ? 2 : 4);
    }

    template<class BuildContours>
    void GLRenderer::DrawCoverage(BuildContours&& Build,
                                  FillRule Rule,
                                  const Brush& Source,
                                  const Matrix3x2& Transform,
                                  uint64_t CacheId,
                                  uint64_t StyleKey) {
        const PixelBox Clip = GetScissor();
        if (Clip.IsEmpty() || GetMinScale(Transform) <= 1e-6f) { return; }
        const Paint Fill = ResolveBrush(Source);

        // A path drawn again with the same transform, up to whole pixels, reuses its mask from the atlas.
        if (CacheId != 0) {
            float WholeX    = std::floor(Transform.Dx);
            float WholeY    = std::floor(Transform.Dy);
            auto FractionX  = static_cast<int32_t>(std::lround((Transform.Dx - WholeX) * kMaskFractionSteps));
            auto FractionY  = static_cast<int32_t>(std::lround((Transform.Dy - WholeY) * kMaskFractionSteps));
            if (FractionX == kMaskFractionSteps) {
                FractionX = 0;
                WholeX += 1.0f;
            }
            if (FractionY == kMaskFractionSteps) {
                FractionY = 0;
                WholeY += 1.0f;
            }
            const MaskKey Key {CacheId,
                               StyleKey,
                               Transform.M11,
                               Transform.M12,
                               Transform.M21,
                               Transform.M22,
                               FractionX,
                               FractionY};
            const auto OffsetX = static_cast<int32_t>(WholeX);
            const auto OffsetY = static_cast<int32_t>(WholeY);
            Matrix3x2 Snapped  = Transform;
            Snapped.Dx         = static_cast<float>(FractionX) / kMaskFractionSteps;
            Snapped.Dy         = static_cast<float>(FractionY) / kMaskFractionSteps;
            Matrix3x2 Placed   = Snapped;
            Placed.Dx += WholeX;
            Placed.Dy += WholeY;
            const std::optional<Matrix3x2> Inverse = Placed.Inverted();
            if (!Inverse) { return; }

            if (const auto It = _MaskCache.find(Key); It != _MaskCache.end()) {
                const PixelBox Box {It->second.Box.X + OffsetX,
                                    It->second.Box.Y + OffsetY,
                                    It->second.Box.Width,
                                    It->second.Box.Height};
                if (Contains(Clip, Box)) {
                    EmitMask(It->second.Slot, Box, Fill, *Inverse);
                    return;
                }
            } else {
                std::vector<Contour> Contours = Build();
                TransformContours(Contours, Snapped);
                const PixelBox Relative = GetContourBounds(Contours);
                const PixelBox Box {Relative.X + OffsetX, Relative.Y + OffsetY, Relative.Width, Relative.Height};
                if (Relative.IsEmpty()) { return; }
                if (Contains(Clip, Box) && Relative.Width <= kMaxCachedMask && Relative.Height <= kMaxCachedMask) {
                    RasterizeCoverage(Contours, Rule, Relative, _MaskScratch);
                    const std::optional<AtlasPacker::Slot> Slot = AllocateMask(Relative);
                    if (!Slot) { return; }
                    _MaskCache.emplace(Key, CachedMask {*Slot, Relative});
                    EmitMask(*Slot, Box, Fill, *Inverse);
                    return;
                }
            }
        }

        const std::optional<Matrix3x2> Inverse = Transform.Inverted();
        if (!Inverse) { return; }
        std::vector<Contour> Contours = Build();
        TransformContours(Contours, Transform);
        const PixelBox Covered = Intersect(GetContourBounds(Contours), Clip);
        if (Covered.IsEmpty()) { return; }

        for (int32_t Y = Covered.Y; Y < Covered.Y + Covered.Height; Y += kMaxMaskTile) {
            for (int32_t X = Covered.X; X < Covered.X + Covered.Width; X += kMaxMaskTile) {
                const PixelBox Tile {X,
                                     Y,
                                     std::min(kMaxMaskTile, Covered.X + Covered.Width - X),
                                     std::min(kMaxMaskTile, Covered.Y + Covered.Height - Y)};
                RasterizeCoverage(Contours, Rule, Tile, _MaskScratch);
                if (std::all_of(_MaskScratch.begin(), _MaskScratch.end(), [](uint8_t Value) { return Value == 0; })) {
                    continue;
                }
                const std::optional<AtlasPacker::Slot> Slot = AllocateMask(Tile);
                if (Slot) { EmitMask(*Slot, Tile, Fill, *Inverse); }
            }
        }
    }

    std::optional<AtlasPacker::Slot> GLRenderer::AllocateMask(const PixelBox& Box) {
        const auto Width  = static_cast<uint32_t>(Box.Width);
        const auto Height = static_cast<uint32_t>(Box.Height);
        std::optional<AtlasPacker::Slot> Slot = _MaskPacker.Allocate(Width, Height);
        if (!Slot) {
            Flush();
            _MaskPacker.Reset();
            _MaskCache.clear();
            Slot = _MaskPacker.Allocate(Width, Height);
            if (!Slot) { return std::nullopt; }
        }
        GL::Api.ActiveTexture(GL::TEXTURE0 + kMaskUnit);
        GL::Api.BindTexture(GL::TEXTURE_2D, _MaskAtlas.Get());
        GL::Api.PixelStorei(GL::UNPACK_ALIGNMENT, 1);
        GL::Api.TexSubImage2D(GL::TEXTURE_2D,
                              0,
                              static_cast<GL::GLint>(Slot->X),
                              static_cast<GL::GLint>(Slot->Y),
                              static_cast<GL::GLsizei>(Width),
                              static_cast<GL::GLsizei>(Height),
                              GL::RED,
                              GL::UNSIGNED_BYTE,
                              _MaskScratch.data());
        GL::Api.PixelStorei(GL::UNPACK_ALIGNMENT, 4);
        return Slot;
    }

    void GLRenderer::EmitMask(const AtlasPacker::Slot& Slot,
                              const PixelBox& Box,
                              const Paint& Fill,
                              const Matrix3x2& Inverse) {
        const Rect Area {static_cast<float>(Box.X),
                         static_cast<float>(Box.Y),
                         static_cast<float>(Box.Width),
                         static_cast<float>(Box.Height)};
        const float Size = static_cast<float>(kMaskAtlasSize);
        const Rect Source {static_cast<float>(Slot.X) / Size,
                           static_cast<float>(Slot.Y) / Size,
                           Area.Width / Size,
                           Area.Height / Size};
        Vec2 Device[4];
        Vec2 Texture[4];
        Vec2 Local[4];
        GetCorners(Area, Device);
        GetCorners(Source, Texture);
        for (int I = 0; I < 4; ++I) {
            Local[I] = Inverse.TransformPoint(Device[I]);
        }
        constexpr float kNoParams[4] = {};
        EmitQuad(Device, Texture, Local, kNoParams, ShapeKind::Mask, Fill);
    }

    void GLRenderer::DrawText(const TextLayoutData& Layout,
                              const Vec2& Origin,
                              const Brush& Source,
                              const Matrix3x2& Transform) {
        if (Layout.Glyphs.empty()) { return; }
        const std::optional<Matrix3x2> Inverse = Transform.Inverted();
        if (!Inverse) { return; }

        const float Scale     = Transform.GetMaxScale();
        const float PixelSize = Layout.EmSize * Scale;
        if (PixelSize < 0.5f) { return; }
        // Axis-aligned, unrotated text snaps to the pixel grid like DirectWrite's: whole pixels vertically,
        // quarter pixels horizontally. Anything else is drawn from unsnapped bitmaps.
        const bool Aligned = Transform.M12 == 0.0f && Transform.M21 == 0.0f && Transform.M11 > 0.0f &&
                             std::abs(Transform.M11 - Transform.M22) <= 1e-4f * Transform.M11;

        const Paint Fill            = ResolveBrush(Source);
        const auto AtlasWidth       = static_cast<float>(_Glyphs.GetWidth());
        const auto AtlasHeight      = static_cast<float>(_Glyphs.GetHeight());
        constexpr float kNoParams[4] = {};

        for (const TextLayoutData::Glyph& Glyph : Layout.Glyphs) {
            const FontFace& Face = *Layout.Faces[Glyph.Face];
            const Vec2 Pen       = Origin + Glyph.Origin;
            const Vec2 DevicePen = Transform.TransformPoint(Pen);

            int Subpixel = 0;
            Vec2 Snapped = DevicePen;
            if (Aligned) {
                float Whole = std::floor(DevicePen.X);
                Subpixel    = static_cast<int>(std::lround((DevicePen.X - Whole) * GlyphCache::kSubpixelSteps));
                if (Subpixel == GlyphCache::kSubpixelSteps) {
                    Subpixel = 0;
                    Whole += 1.0f;
                }
                Snapped = {Whole, std::round(DevicePen.Y)};
            }

            const GlyphCache::Entry* Entry = _Glyphs.Get(Face, Glyph.Index, PixelSize, Subpixel);
            if (Entry == nullptr) {
                Flush();
                _Glyphs.Reset();
                Entry = _Glyphs.Get(Face, Glyph.Index, PixelSize, Subpixel);
                if (Entry == nullptr) { continue; }
            }
            if (Entry->IsEmpty()) { continue; }

            const Rect Bitmap {static_cast<float>(Entry->Left),
                               static_cast<float>(Entry->Top),
                               static_cast<float>(Entry->Width),
                               static_cast<float>(Entry->Height)};
            Vec2 Device[4];
            Vec2 Local[4];
            if (Aligned) {
                GetCorners({Snapped.X + Bitmap.X, Snapped.Y + Bitmap.Y, Bitmap.Width, Bitmap.Height}, Device);
                for (int I = 0; I < 4; ++I) {
                    Local[I] = Inverse->TransformPoint(Device[I]);
                }
            } else {
                GetCorners({Pen.X + Bitmap.X / Scale,
                            Pen.Y + Bitmap.Y / Scale,
                            Bitmap.Width / Scale,
                            Bitmap.Height / Scale},
                           Local);
                for (int I = 0; I < 4; ++I) {
                    Device[I] = Transform.TransformPoint(Local[I]);
                }
            }
            Vec2 Texture[4];
            GetCorners({static_cast<float>(Entry->AtlasX) / AtlasWidth,
                        static_cast<float>(Entry->AtlasY) / AtlasHeight,
                        Bitmap.Width / AtlasWidth,
                        Bitmap.Height / AtlasHeight},
                       Texture);
            EmitQuad(Device,
                     Texture,
                     Local,
                     kNoParams,
                     Entry->Color ? ShapeKind::ColorGlyph : ShapeKind::Glyph,
                     Fill);
        }
    }

    void GLRenderer::BindImage(uint32_t Texture, ImageInterpolation Interpolation) {
        if (Texture == _BoundImage && Interpolation == _BoundInterpolation) { return; }
        Flush();
        _BoundImage         = Texture;
        _BoundInterpolation = Interpolation;
        GL::Api.ActiveTexture(GL::TEXTURE0 + kImageUnit);
        GL::Api.BindTexture(GL::TEXTURE_2D, Texture);
        GL::GLenum Minify  = GL::LINEAR;
        GL::GLenum Magnify = GL::LINEAR;
        if (Interpolation == ImageInterpolation::NearestNeighbor) {
            Minify  = GL::NEAREST;
            Magnify = GL::NEAREST;
        }
        GL::Api.TexParameteri(GL::TEXTURE_2D, GL::TEXTURE_MIN_FILTER, static_cast<GL::GLint>(Minify));
        GL::Api.TexParameteri(GL::TEXTURE_2D, GL::TEXTURE_MAG_FILTER, static_cast<GL::GLint>(Magnify));
    }

    void GLRenderer::DrawTexture(uint32_t Texture,
                                 const Rect& Destination,
                                 const Rect& Source,
                                 const Matrix3x2& Transform,
                                 float Opacity,
                                 ImageInterpolation Interpolation,
                                 bool FlippedY) {
        if (Texture == 0 || Opacity <= 0.0f) { return; }
        BindImage(Texture, Interpolation);

        Vec2 Local[4];
        Vec2 Device[4];
        Vec2 Coordinates[4];
        GetCorners(Destination, Local);
        GetCorners(Source, Coordinates);
        for (int I = 0; I < 4; ++I) {
            Device[I] = Transform.TransformPoint(Local[I]);
            if (FlippedY) { Coordinates[I].Y = 1.0f - Coordinates[I].Y; }
        }
        Paint Fill;
        Fill.Color[0] = Fill.Color[1] = Fill.Color[2] = Fill.Color[3] = std::clamp(Opacity, 0.0f, 1.0f);
        constexpr float kNoParams[4]                                   = {};
        EmitQuad(Device, Coordinates, Local, kNoParams, ShapeKind::Texture, Fill);
    }

    void GLRenderer::DrawImage(const ImageData& Source,
                               const Rect& Destination,
                               const std::optional<Rect>& SourceRect,
                               float Opacity,
                               ImageInterpolation Interpolation,
                               const Matrix3x2& Transform) {
        if (Source.Width == 0 || Source.Height == 0) { return; }
        if (!Source.Texture || Source.Texture.GetGeneration() != _Generation) {
            Flush();
            GL::GLuint Name = 0;
            GL::Api.GenTextures(1, &Name);
            GL::Api.ActiveTexture(GL::TEXTURE0 + kImageUnit);
            GL::Api.BindTexture(GL::TEXTURE_2D, Name);
            GL::Api.PixelStorei(GL::UNPACK_ALIGNMENT, 4);
            GL::Api.TexImage2D(GL::TEXTURE_2D,
                               0,
                               static_cast<GL::GLint>(GL::RGBA8),
                               static_cast<GL::GLsizei>(Source.Width),
                               static_cast<GL::GLsizei>(Source.Height),
                               0,
                               GL::BGRA,
                               GL::UNSIGNED_BYTE,
                               Source.Pixels.data());
            GL::Api.GenerateMipmap(GL::TEXTURE_2D);
            GL::Api.TexParameteri(GL::TEXTURE_2D, GL::TEXTURE_WRAP_S, static_cast<GL::GLint>(GL::CLAMP_TO_EDGE));
            GL::Api.TexParameteri(GL::TEXTURE_2D, GL::TEXTURE_WRAP_T, static_cast<GL::GLint>(GL::CLAMP_TO_EDGE));
            Source.Texture = GLTexture(Name, _Generation);
            // Force BindImage to set this texture's filters.
            _BoundImage = 0;
        }

        const auto Width  = static_cast<float>(Source.Width);
        const auto Height = static_cast<float>(Source.Height);
        const Rect Pixels = SourceRect.value_or(Rect {0.0f, 0.0f, Width, Height});
        BindImage(Source.Texture.Get(), Interpolation);
        if (Interpolation == ImageInterpolation::HighQuality) {
            GL::Api.TexParameteri(GL::TEXTURE_2D,
                                  GL::TEXTURE_MIN_FILTER,
                                  static_cast<GL::GLint>(GL::LINEAR_MIPMAP_LINEAR));
        }
        DrawTexture(Source.Texture.Get(),
                    Destination,
                    {Pixels.X / Width, Pixels.Y / Height, Pixels.Width / Width, Pixels.Height / Height},
                    Transform,
                    Opacity,
                    Interpolation,
                    false);
    }

    void GLRenderer::Flush() {
        if (_Pending.empty()) { return; }
        constexpr size_t kCapacity = kMaxQuads * 4;
        GL::Api.BindBuffer(GL::ARRAY_BUFFER, _Vertices);
        // Append each batch after the last one, and orphan the buffer only when it fills, so the driver neither
        // reallocates it per batch nor waits for draws still reading earlier batches.
        if (_VertexCursor + _Pending.size() > kCapacity) {
            GL::Api.BufferData(GL::ARRAY_BUFFER,
                               static_cast<GL::GLsizeiptr>(kCapacity * sizeof(Vertex)),
                               nullptr,
                               GL::STREAM_DRAW);
            _VertexCursor = 0;
        }
        GL::Api.BufferSubData(GL::ARRAY_BUFFER,
                              static_cast<GL::GLintptr>(_VertexCursor * sizeof(Vertex)),
                              static_cast<GL::GLsizeiptr>(_Pending.size() * sizeof(Vertex)),
                              _Pending.data());
        GL::Api.DrawElementsBaseVertex(GL::TRIANGLES,
                                       static_cast<GL::GLsizei>(_Pending.size() / 4 * 6),
                                       GL::UNSIGNED_SHORT,
                                       nullptr,
                                       static_cast<GL::GLint>(_VertexCursor));
        _VertexCursor += _Pending.size();
        _Pending.clear();
    }

    /// @brief Walks a display list, keeping the transform, clip and opacity stacks like the Direct2D Replayer.
    class GLReplayer {
    public:
        GLReplayer(GLRenderer& Renderer, const Matrix3x2& Base) : _Renderer(Renderer), _Base(Base) {}

        ~GLReplayer() {
            while (!_Stack.empty()) {
                PopEntry(_Stack.back());
            }
        }

        GLReplayer(const GLReplayer&)            = delete;
        GLReplayer& operator=(const GLReplayer&) = delete;

        void operator()(const PushTransformCommand& Command) {
            _TransformStack.push_back(_Local);
            _Local = Command.Transform * _Local;
            _Stack.push_back(Entry::Transform);
        }

        void operator()(const PopTransformCommand&) { PopIfTop(Entry::Transform); }

        void operator()(const PushClipCommand& Command) {
            _Renderer.PushClip(Command.Bounds, World());
            _Stack.push_back(Entry::Clip);
        }

        void operator()(const PopClipCommand&) { PopIfTop(Entry::Clip); }

        void operator()(const PushOpacityCommand& Command) {
            _Renderer.PushOpacity(Command.Opacity);
            _Stack.push_back(Entry::Opacity);
        }

        void operator()(const PopOpacityCommand&) { PopIfTop(Entry::Opacity); }

        void operator()(const FillRectCommand& Command) { _Renderer.FillRect(Command.Bounds, Command.Paint, World()); }

        void operator()(const StrokeRectCommand& Command) {
            _Renderer.StrokeRect(Command.Bounds, Command.Paint, Command.Stroke, World());
        }

        void operator()(const FillRoundedRectCommand& Command) {
            _Renderer.DrawRoundedRect(Command.Bounds, Command.Radius, Command.Paint, World());
        }

        void operator()(const StrokeRoundedRectCommand& Command) {
            _Renderer.StrokeRoundedRect(Command.Bounds, Command.Radius, Command.Paint, Command.Stroke, World());
        }

        void operator()(const FillEllipseCommand& Command) {
            _Renderer.FillEllipse(Command.Center, Command.Radius, Command.Paint, World());
        }

        void operator()(const StrokeEllipseCommand& Command) {
            _Renderer.StrokeEllipse(Command.Center, Command.Radius, Command.Paint, Command.Stroke, World());
        }

        void operator()(const DrawLineCommand& Command) {
            _Renderer.DrawLine(Command.Start, Command.End, Command.Paint, Command.Stroke, World());
        }

        void operator()(const FillPathCommand& Command) {
            const PathData* Data = Command.Geometry.GetData();
            if (Data == nullptr) { return; }
            _Renderer.FillFigures(Data->Figures, Data->Rule, Command.Paint, World(), Data->Id);
        }

        void operator()(const StrokePathCommand& Command) {
            const PathData* Data = Command.Geometry.GetData();
            if (Data == nullptr) { return; }
            _Renderer.StrokeFigures(Data->Figures, Command.Paint, Command.Stroke, World(), Data->Id);
        }

        void operator()(const DrawTextLayoutCommand& Command) {
            const TextLayoutData* Data = Command.Layout.GetData();
            if (Data == nullptr) { return; }
            _Renderer.DrawText(*Data, Command.Origin, Command.Paint, World());
        }

        void operator()(const DrawImageCommand& Command) {
            const ImageData* Data = Command.Source.GetData();
            if (Data == nullptr) { return; }
            const ImageDrawOptions& Options = Command.Options;
            _Renderer.DrawImage(*Data,
                                Command.Destination,
                                Options.SourceRect,
                                Options.Opacity,
                                Options.Interpolation,
                                World());
        }

    private:
        enum class Entry : uint8_t { Transform, Clip, Opacity };

        [[nodiscard]] Matrix3x2 World() const { return _Local * _Base; }

        void PopIfTop(Entry Kind) {
            if (!_Stack.empty() && _Stack.back() == Kind) { PopEntry(Kind); }
        }

        void PopEntry(Entry Kind) {
            _Stack.pop_back();
            switch (Kind) {
                case Entry::Transform:
                    _Local = _TransformStack.back();
                    _TransformStack.pop_back();
                    break;
                case Entry::Clip:
                    _Renderer.PopScissor();
                    break;
                case Entry::Opacity:
                    _Renderer.PopOpacity();
                    break;
            }
        }

        GLRenderer& _Renderer;
        Matrix3x2 _Base;
        Matrix3x2 _Local;
        std::vector<Matrix3x2> _TransformStack;
        std::vector<Entry> _Stack;
    };

    void GLRenderer::DrawDisplayList(const DisplayList& List, const Matrix3x2& Base) {
        GLReplayer Visitor(*this, Base);
        for (const DrawCommand& Command : List.GetCommands()) {
            std::visit(Visitor, Command);
        }
    }
}  // namespace XenUI
