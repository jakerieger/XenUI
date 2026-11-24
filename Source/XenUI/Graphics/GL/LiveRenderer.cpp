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

#include "XenUI/Graphics/GL/LiveRenderer.hpp"
#include "XenUI/Graphics/GL/GLApi.hpp"
#include "XenUI/Graphics/GL/GLShader.hpp"

#include <algorithm>
#include <cstddef>

namespace XenUI {
    namespace {
        constexpr size_t kInitialInstanceCapacity = 4096;

        const char* const kPrimitiveVertexShader = R"(#version 330 core
layout(location = 0) in vec2 aStart;
layout(location = 1) in vec2 aEnd;
layout(location = 2) in vec4 aColor;
layout(location = 3) in float aWidth;
layout(location = 4) in float aKind;

uniform vec2 uViewport;
uniform float uPixelsPerDip;

out vec2 vLocal;
flat out vec2 vStart;
flat out vec2 vEnd;
flat out vec4 vColor;
flat out vec2 vShape;

void main() {
    float Feather = 1.0 / uPixelsPerDip;
    vec2 Side     = vec2(gl_VertexID & 1, (gl_VertexID >> 1) & 1);

    vec2 Position;
    if (aKind < 0.5) {
        float Radius = max(aWidth, Feather) * 0.5 + Feather;
        vec2 Delta   = aEnd - aStart;
        float Length = length(Delta);
        vec2 Along   = Length > 1e-5 ? Delta / Length : vec2(1.0, 0.0);
        vec2 Across  = vec2(-Along.y, Along.x);
        vec2 Origin  = aStart - Along * Radius - Across * Radius;
        Position     = Origin + Along * (Length + Radius * 2.0) * Side.x + Across * (Radius * 2.0) * Side.y;
    } else {
        Position = mix(aStart - Feather, aEnd + Feather, Side);
    }

    gl_Position = vec4(Position / uViewport * vec2(2.0, -2.0) + vec2(-1.0, 1.0), 0.0, 1.0);
    vLocal      = Position;
    vStart      = aStart;
    vEnd        = aEnd;
    vColor      = aColor;
    vShape      = vec2(aWidth, aKind);
}
)";

        const char* const kPrimitiveFragmentShader = R"(#version 330 core
in vec2 vLocal;
flat in vec2 vStart;
flat in vec2 vEnd;
flat in vec4 vColor;
flat in vec2 vShape;

uniform float uPixelsPerDip;

layout(location = 0) out vec4 oColor;

void main() {
    float Distance;
    float Thinness = 1.0;
    if (vShape.y < 0.5) {
        float MinimumWidth = 1.0 / uPixelsPerDip;
        float Width        = max(vShape.x, MinimumWidth);
        vec2 FromStart     = vLocal - vStart;
        vec2 Segment       = vEnd - vStart;
        float Along        = clamp(dot(FromStart, Segment) / max(dot(Segment, Segment), 1e-6), 0.0, 1.0);
        Distance           = length(FromStart - Segment * Along) - Width * 0.5;
        Thinness           = clamp(vShape.x / MinimumWidth, 0.0, 1.0);
    } else {
        vec2 Center   = (vStart + vEnd) * 0.5;
        vec2 HalfSize = (vEnd - vStart) * 0.5;
        vec2 Outside  = abs(vLocal - Center) - HalfSize;
        Distance      = length(max(Outside, 0.0)) + min(max(Outside.x, Outside.y), 0.0);
    }

    float Coverage = clamp(0.5 - Distance * uPixelsPerDip, 0.0, 1.0) * Thinness;
    float Alpha    = vColor.a * Coverage;
    oColor         = vec4(vColor.rgb * Alpha, Alpha);
}
)";

        const char* const kTextureVertexShader = R"(#version 330 core
uniform vec2 uViewport;
uniform float uSourceOffset;
uniform vec4 uDestination;

out vec2 vCoordinate;

void main() {
    vec2 Side     = vec2(gl_VertexID & 1, (gl_VertexID >> 1) & 1);
    vec2 Position = uDestination.xy + uDestination.zw * Side;
    gl_Position   = vec4(Position / uViewport * vec2(2.0, -2.0) + vec2(-1.0, 1.0), 0.0, 1.0);
    vCoordinate   = vec2(Side.x + uSourceOffset, Side.y);
}
)";

        const char* const kTextureFragmentShader = R"(#version 330 core
in vec2 vCoordinate;

uniform sampler2D uSource;
uniform float uOpacity;

layout(location = 0) out vec4 oColor;

void main() {
    oColor = texture(uSource, vCoordinate) * uOpacity;
}
)";
    }  // namespace

    LiveRenderer::LiveRenderer(uint64_t Generation) : _Generation(Generation) {
        _PrimitiveProgram  = CompileProgram(kPrimitiveVertexShader, kPrimitiveFragmentShader, "live primitives");
        _TextureProgram    = CompileProgram(kTextureVertexShader, kTextureFragmentShader, "live texture");
        _PrimitiveViewport = GL::Api.GetUniformLocation(_PrimitiveProgram, "uViewport");
        _PrimitiveScale    = GL::Api.GetUniformLocation(_PrimitiveProgram, "uPixelsPerDip");
        _TextureViewport   = GL::Api.GetUniformLocation(_TextureProgram, "uViewport");
        _TextureDestination = GL::Api.GetUniformLocation(_TextureProgram, "uDestination");
        _TextureOffset     = GL::Api.GetUniformLocation(_TextureProgram, "uSourceOffset");
        _TextureOpacity    = GL::Api.GetUniformLocation(_TextureProgram, "uOpacity");
        GL::Api.UseProgram(_TextureProgram);
        GL::Api.Uniform1i(GL::Api.GetUniformLocation(_TextureProgram, "uSource"), 0);

        GL::Api.GenVertexArrays(1, &_TextureArray);
        GL::Api.GenVertexArrays(1, &_PrimitiveArray);
        GL::Api.BindVertexArray(_PrimitiveArray);
        GL::Api.GenBuffers(1, &_Instances);
        GL::Api.BindBuffer(GL::ARRAY_BUFFER, _Instances);
        _InstanceCapacity = kInitialInstanceCapacity;
        GL::Api.BufferData(GL::ARRAY_BUFFER,
                           static_cast<GL::GLsizeiptr>(_InstanceCapacity * sizeof(LiveInstance)),
                           nullptr,
                           GL::STREAM_DRAW);

        struct Attribute {
            GL::GLint Size;
            size_t Offset;
        };
        const Attribute Attributes[] = {
          {2, offsetof(LiveInstance, Start)},
          {2, offsetof(LiveInstance, End)},
          {4, offsetof(LiveInstance, Color)},
          {1, offsetof(LiveInstance, Width)},
          {1, offsetof(LiveInstance, Kind)},
        };
        for (GL::GLuint Index = 0; Index < std::size(Attributes); ++Index) {
            GL::Api.EnableVertexAttribArray(Index);
            GL::Api.VertexAttribPointer(Index,
                                        Attributes[Index].Size,
                                        GL::FLOAT,
                                        0,
                                        static_cast<GL::GLsizei>(sizeof(LiveInstance)),
                                        reinterpret_cast<const void*>(Attributes[Index].Offset));
            GL::Api.VertexAttribDivisor(Index, 1);
        }
        GL::Api.BindVertexArray(0);
    }

    void LiveRenderer::Render(uint32_t Framebuffer,
                              const Size& Extent,
                              float PixelsPerDip,
                              const LiveRecording& Recording) {
        GL::Api.BindFramebuffer(GL::FRAMEBUFFER, Framebuffer);
        GL::Api.Viewport(0,
                         0,
                         static_cast<GL::GLsizei>(Extent.Width * PixelsPerDip + 0.5f),
                         static_cast<GL::GLsizei>(Extent.Height * PixelsPerDip + 0.5f));
        GL::Api.Disable(GL::SCISSOR_TEST);
        GL::Api.ClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        GL::Api.Clear(GL::COLOR_BUFFER_BIT);
        GL::Api.Enable(GL::BLEND);
        GL::Api.BlendFunc(GL::ONE, GL::ONE_MINUS_SRC_ALPHA);

        if (!Recording.Instances.empty()) {
            GL::Api.BindBuffer(GL::ARRAY_BUFFER, _Instances);
            if (Recording.Instances.size() > _InstanceCapacity) {
                while (_InstanceCapacity < Recording.Instances.size()) {
                    _InstanceCapacity *= 2;
                }
            }
            GL::Api.BufferData(GL::ARRAY_BUFFER,
                               static_cast<GL::GLsizeiptr>(_InstanceCapacity * sizeof(LiveInstance)),
                               nullptr,
                               GL::STREAM_DRAW);
            GL::Api.BufferSubData(GL::ARRAY_BUFFER,
                                  0,
                                  static_cast<GL::GLsizeiptr>(Recording.Instances.size() * sizeof(LiveInstance)),
                                  Recording.Instances.data());
        }

        GL::Api.UseProgram(_PrimitiveProgram);
        GL::Api.Uniform2f(_PrimitiveViewport, Extent.Width, Extent.Height);
        GL::Api.Uniform1f(_PrimitiveScale, PixelsPerDip);
        GL::Api.UseProgram(_TextureProgram);
        GL::Api.Uniform2f(_TextureViewport, Extent.Width, Extent.Height);

        for (const LiveBatch& Batch : Recording.Batches) {
            if (!Batch.IsTexture) {
                GL::Api.UseProgram(_PrimitiveProgram);
                GL::Api.BindVertexArray(_PrimitiveArray);
                // Attribute offsets are fixed, so start mid-buffer by re-pointing them at the batch's first instance.
                const size_t Base = static_cast<size_t>(Batch.FirstInstance) * sizeof(LiveInstance);
                GL::Api.BindBuffer(GL::ARRAY_BUFFER, _Instances);
                const size_t Offsets[] = {offsetof(LiveInstance, Start),
                                          offsetof(LiveInstance, End),
                                          offsetof(LiveInstance, Color),
                                          offsetof(LiveInstance, Width),
                                          offsetof(LiveInstance, Kind)};
                const GL::GLint Sizes[] = {2, 2, 4, 1, 1};
                for (GL::GLuint Index = 0; Index < std::size(Offsets); ++Index) {
                    GL::Api.VertexAttribPointer(Index,
                                                Sizes[Index],
                                                GL::FLOAT,
                                                0,
                                                static_cast<GL::GLsizei>(sizeof(LiveInstance)),
                                                reinterpret_cast<const void*>(Base + Offsets[Index]));
                }
                GL::Api.DrawArraysInstanced(GL::TRIANGLE_STRIP, 0, 4, static_cast<GL::GLsizei>(Batch.InstanceCount));
                continue;
            }

            auto& Texture = const_cast<LiveTextureData&>(*Batch.TextureDraw.Texture);
            UploadTexture(Texture);

            const Rect& Destination = Batch.TextureDraw.Destination;
            GL::Api.UseProgram(_TextureProgram);
            GL::Api.BindVertexArray(_TextureArray);
            GL::Api.Uniform4f(_TextureDestination,
                              Destination.X,
                              Destination.Y,
                              Destination.Width,
                              Destination.Height);
            GL::Api.Uniform1f(_TextureOffset, Batch.TextureDraw.SourceOffset);
            GL::Api.Uniform1f(_TextureOpacity, Batch.TextureDraw.Opacity);
            GL::Api.ActiveTexture(GL::TEXTURE0);
            GL::Api.BindTexture(GL::TEXTURE_2D, Texture.Texture.Get());
            GL::Api.DrawArrays(GL::TRIANGLE_STRIP, 0, 4);
        }
        GL::Api.BindVertexArray(0);
    }

    void LiveRenderer::UploadTexture(LiveTextureData& Texture) {
        GL::Api.ActiveTexture(GL::TEXTURE0);
        if (!Texture.Texture || Texture.Texture.GetGeneration() != _Generation) {
            GL::GLuint Name = 0;
            GL::Api.GenTextures(1, &Name);
            GL::Api.BindTexture(GL::TEXTURE_2D, Name);
            GL::Api.TexImage2D(GL::TEXTURE_2D,
                               0,
                               static_cast<GL::GLint>(GL::RGBA8),
                               static_cast<GL::GLsizei>(Texture.Width),
                               static_cast<GL::GLsizei>(Texture.Height),
                               0,
                               GL::BGRA,
                               GL::UNSIGNED_BYTE,
                               nullptr);
            GL::Api.TexParameteri(GL::TEXTURE_2D, GL::TEXTURE_MIN_FILTER, static_cast<GL::GLint>(GL::LINEAR));
            GL::Api.TexParameteri(GL::TEXTURE_2D, GL::TEXTURE_MAG_FILTER, static_cast<GL::GLint>(GL::LINEAR));
            GL::Api.TexParameteri(GL::TEXTURE_2D, GL::TEXTURE_WRAP_S, static_cast<GL::GLint>(GL::REPEAT));
            GL::Api.TexParameteri(GL::TEXTURE_2D, GL::TEXTURE_WRAP_T, static_cast<GL::GLint>(GL::CLAMP_TO_EDGE));
            Texture.Texture = GLTexture(Name, _Generation);
            Texture.Dirty   = true;
        }

        GL::Api.BindTexture(GL::TEXTURE_2D, Texture.Texture.Get());
        if (!Texture.Dirty) { return; }
        GL::Api.PixelStorei(GL::UNPACK_ALIGNMENT, 4);
        GL::Api.TexSubImage2D(GL::TEXTURE_2D,
                              0,
                              0,
                              0,
                              static_cast<GL::GLsizei>(Texture.Width),
                              static_cast<GL::GLsizei>(Texture.Height),
                              GL::BGRA,
                              GL::UNSIGNED_BYTE,
                              Texture.Pixels.data());
        Texture.Dirty = false;
    }
}  // namespace XenUI
