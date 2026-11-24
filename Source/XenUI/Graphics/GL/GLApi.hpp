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

#include <cstddef>
#include <cstdint>

// The OpenGL 3.3 core entry points XenUI uses, loaded at run time through whatever GetProcAddress the context came
// from (SDL or EGL). Declared here instead of taken from GL headers, so building needs no GL development package.

namespace XenUI::GL {
    using GLenum     = unsigned int;
    using GLboolean  = unsigned char;
    using GLbitfield = unsigned int;
    using GLint      = int;
    using GLuint     = unsigned int;
    using GLsizei    = int;
    using GLfloat    = float;
    using GLchar     = char;
    using GLubyte    = unsigned char;
    using GLsizeiptr = std::ptrdiff_t;
    using GLintptr   = std::ptrdiff_t;

    inline constexpr GLenum NO_ERROR                 = 0;
    inline constexpr GLenum VENDOR                   = 0x1F00;
    inline constexpr GLenum RENDERER                 = 0x1F01;
    inline constexpr GLenum VERSION                  = 0x1F02;
    inline constexpr GLenum TRIANGLES                = 0x0004;
    inline constexpr GLenum TRIANGLE_STRIP           = 0x0005;
    inline constexpr GLenum BLEND                    = 0x0BE2;
    inline constexpr GLenum SCISSOR_TEST             = 0x0C11;
    inline constexpr GLenum CULL_FACE                = 0x0B44;
    inline constexpr GLenum DEPTH_TEST               = 0x0B71;
    inline constexpr GLenum STENCIL_TEST             = 0x0B90;
    inline constexpr GLenum FRAMEBUFFER_SRGB         = 0x8DB9;
    inline constexpr GLenum ONE                      = 1;
    inline constexpr GLenum ONE_MINUS_SRC_ALPHA      = 0x0303;
    inline constexpr GLenum COLOR_BUFFER_BIT         = 0x00004000;
    inline constexpr GLenum UNSIGNED_BYTE            = 0x1401;
    inline constexpr GLenum UNSIGNED_SHORT           = 0x1403;
    inline constexpr GLenum FLOAT                    = 0x1406;
    inline constexpr GLenum TEXTURE_2D               = 0x0DE1;
    inline constexpr GLenum TEXTURE0                 = 0x84C0;
    inline constexpr GLenum TEXTURE_MAG_FILTER       = 0x2800;
    inline constexpr GLenum TEXTURE_MIN_FILTER       = 0x2801;
    inline constexpr GLenum TEXTURE_WRAP_S           = 0x2802;
    inline constexpr GLenum TEXTURE_WRAP_T           = 0x2803;
    inline constexpr GLenum TEXTURE_MAX_LEVEL        = 0x813D;
    inline constexpr GLenum NEAREST                  = 0x2600;
    inline constexpr GLenum LINEAR                   = 0x2601;
    inline constexpr GLenum LINEAR_MIPMAP_LINEAR     = 0x2703;
    inline constexpr GLenum CLAMP_TO_EDGE            = 0x812F;
    inline constexpr GLenum REPEAT                   = 0x2901;
    inline constexpr GLenum RED                      = 0x1903;
    inline constexpr GLenum R8                       = 0x8229;
    inline constexpr GLenum RGBA                     = 0x1908;
    inline constexpr GLenum RGBA8                    = 0x8058;
    inline constexpr GLenum BGRA                     = 0x80E1;
    inline constexpr GLenum UNPACK_ALIGNMENT         = 0x0CF5;
    inline constexpr GLenum UNPACK_ROW_LENGTH        = 0x0CF2;
    inline constexpr GLenum PACK_ALIGNMENT           = 0x0D05;
    inline constexpr GLenum FRAMEBUFFER              = 0x8D40;
    inline constexpr GLenum READ_FRAMEBUFFER         = 0x8CA8;
    inline constexpr GLenum DRAW_FRAMEBUFFER         = 0x8CA9;
    inline constexpr GLenum COLOR_ATTACHMENT0        = 0x8CE0;
    inline constexpr GLenum FRAMEBUFFER_COMPLETE     = 0x8CD5;
    inline constexpr GLenum ARRAY_BUFFER             = 0x8892;
    inline constexpr GLenum ELEMENT_ARRAY_BUFFER     = 0x8893;
    inline constexpr GLenum STREAM_DRAW              = 0x88E0;
    inline constexpr GLenum STATIC_DRAW              = 0x88E4;
    inline constexpr GLenum FRAGMENT_SHADER          = 0x8B30;
    inline constexpr GLenum VERTEX_SHADER            = 0x8B31;
    inline constexpr GLenum COMPILE_STATUS           = 0x8B81;
    inline constexpr GLenum LINK_STATUS              = 0x8B82;
    inline constexpr GLenum INFO_LOG_LENGTH          = 0x8B84;
    inline constexpr GLenum MAX_TEXTURE_SIZE         = 0x0D33;
    inline constexpr GLenum TEXTURE_SWIZZLE_RGBA     = 0x8E46;

#define XENUI_GL_FUNCTIONS(X)                                                                                         \
    X(const GLubyte*, GetString, (GLenum Name))                                                                       \
    X(GLenum, GetError, ())                                                                                           \
    X(void, GetIntegerv, (GLenum Name, GLint * Data))                                                                 \
    X(void, Enable, (GLenum Cap))                                                                                     \
    X(void, Disable, (GLenum Cap))                                                                                    \
    X(void, BlendFunc, (GLenum Source, GLenum Destination))                                                           \
    X(void, Viewport, (GLint X, GLint Y, GLsizei Width, GLsizei Height))                                              \
    X(void, Scissor, (GLint X, GLint Y, GLsizei Width, GLsizei Height))                                               \
    X(void, ClearColor, (GLfloat Red, GLfloat Green, GLfloat Blue, GLfloat Alpha))                                    \
    X(void, Clear, (GLbitfield Mask))                                                                                 \
    X(void, Flush, ())                                                                                                \
    X(void, Finish, ())                                                                                               \
    X(void, PixelStorei, (GLenum Name, GLint Value))                                                                  \
    X(void, ReadPixels,                                                                                               \
      (GLint X, GLint Y, GLsizei Width, GLsizei Height, GLenum Format, GLenum Type, void* Pixels))                    \
    X(void, GenTextures, (GLsizei Count, GLuint * Textures))                                                          \
    X(void, DeleteTextures, (GLsizei Count, const GLuint* Textures))                                                  \
    X(void, BindTexture, (GLenum Target, GLuint Texture))                                                             \
    X(void, ActiveTexture, (GLenum Unit))                                                                             \
    X(void, TexParameteri, (GLenum Target, GLenum Name, GLint Value))                                                 \
    X(void, TexParameteriv, (GLenum Target, GLenum Name, const GLint* Values))                                        \
    X(void, TexImage2D,                                                                                               \
      (GLenum Target,                                                                                                 \
       GLint Level,                                                                                                   \
       GLint InternalFormat,                                                                                          \
       GLsizei Width,                                                                                                 \
       GLsizei Height,                                                                                                \
       GLint Border,                                                                                                  \
       GLenum Format,                                                                                                 \
       GLenum Type,                                                                                                   \
       const void* Pixels))                                                                                           \
    X(void, TexSubImage2D,                                                                                            \
      (GLenum Target,                                                                                                 \
       GLint Level,                                                                                                   \
       GLint X,                                                                                                       \
       GLint Y,                                                                                                       \
       GLsizei Width,                                                                                                 \
       GLsizei Height,                                                                                                \
       GLenum Format,                                                                                                 \
       GLenum Type,                                                                                                   \
       const void* Pixels))                                                                                           \
    X(void, GenerateMipmap, (GLenum Target))                                                                          \
    X(void, GenFramebuffers, (GLsizei Count, GLuint * Framebuffers))                                                  \
    X(void, DeleteFramebuffers, (GLsizei Count, const GLuint* Framebuffers))                                          \
    X(void, BindFramebuffer, (GLenum Target, GLuint Framebuffer))                                                     \
    X(void, FramebufferTexture2D,                                                                                     \
      (GLenum Target, GLenum Attachment, GLenum TextureTarget, GLuint Texture, GLint Level))                          \
    X(GLenum, CheckFramebufferStatus, (GLenum Target))                                                                \
    X(void, BlitFramebuffer,                                                                                          \
      (GLint SourceX0,                                                                                                \
       GLint SourceY0,                                                                                                \
       GLint SourceX1,                                                                                                \
       GLint SourceY1,                                                                                                \
       GLint DestinationX0,                                                                                           \
       GLint DestinationY0,                                                                                           \
       GLint DestinationX1,                                                                                           \
       GLint DestinationY1,                                                                                           \
       GLbitfield Mask,                                                                                               \
       GLenum Filter))                                                                                                \
    X(GLuint, CreateShader, (GLenum Type))                                                                            \
    X(void, ShaderSource, (GLuint Shader, GLsizei Count, const GLchar* const* Sources, const GLint* Lengths))         \
    X(void, CompileShader, (GLuint Shader))                                                                           \
    X(void, GetShaderiv, (GLuint Shader, GLenum Name, GLint * Value))                                                 \
    X(void, GetShaderInfoLog, (GLuint Shader, GLsizei Capacity, GLsizei * Length, GLchar * Log))                      \
    X(void, DeleteShader, (GLuint Shader))                                                                            \
    X(GLuint, CreateProgram, ())                                                                                      \
    X(void, AttachShader, (GLuint Program, GLuint Shader))                                                            \
    X(void, BindAttribLocation, (GLuint Program, GLuint Index, const GLchar* Name))                                   \
    X(void, LinkProgram, (GLuint Program))                                                                            \
    X(void, GetProgramiv, (GLuint Program, GLenum Name, GLint * Value))                                               \
    X(void, GetProgramInfoLog, (GLuint Program, GLsizei Capacity, GLsizei * Length, GLchar * Log))                    \
    X(void, DeleteProgram, (GLuint Program))                                                                          \
    X(void, UseProgram, (GLuint Program))                                                                             \
    X(GLint, GetUniformLocation, (GLuint Program, const GLchar* Name))                                                \
    X(void, Uniform1i, (GLint Location, GLint Value))                                                                 \
    X(void, Uniform1f, (GLint Location, GLfloat Value))                                                               \
    X(void, Uniform2f, (GLint Location, GLfloat X, GLfloat Y))                                                        \
    X(void, Uniform4f, (GLint Location, GLfloat X, GLfloat Y, GLfloat Z, GLfloat W))                                  \
    X(void, GenVertexArrays, (GLsizei Count, GLuint * Arrays))                                                        \
    X(void, DeleteVertexArrays, (GLsizei Count, const GLuint* Arrays))                                                \
    X(void, BindVertexArray, (GLuint Array))                                                                          \
    X(void, GenBuffers, (GLsizei Count, GLuint * Buffers))                                                            \
    X(void, DeleteBuffers, (GLsizei Count, const GLuint* Buffers))                                                    \
    X(void, BindBuffer, (GLenum Target, GLuint Buffer))                                                               \
    X(void, BufferData, (GLenum Target, GLsizeiptr Size, const void* Data, GLenum Usage))                             \
    X(void, BufferSubData, (GLenum Target, GLintptr Offset, GLsizeiptr Size, const void* Data))                       \
    X(void, VertexAttribPointer,                                                                                      \
      (GLuint Index, GLint Size, GLenum Type, GLboolean Normalized, GLsizei Stride, const void* Offset))              \
    X(void, EnableVertexAttribArray, (GLuint Index))                                                                  \
    X(void, VertexAttribDivisor, (GLuint Index, GLuint Divisor))                                                      \
    X(void, DrawArrays, (GLenum Mode, GLint First, GLsizei Count))                                                    \
    X(void, DrawArraysInstanced, (GLenum Mode, GLint First, GLsizei Count, GLsizei Instances))                        \
    X(void, DrawElements, (GLenum Mode, GLsizei Count, GLenum Type, const void* Offset))                              \
    X(void, DrawElementsBaseVertex, (GLenum Mode, GLsizei Count, GLenum Type, const void* Offset, GLint BaseVertex))

    /// @brief The loaded entry points. Valid only on the thread whose context was current when Load ran.
    struct Functions {
#define XENUI_GL_DECLARE(Return, Name, Arguments) Return(*Name) Arguments = nullptr;
        XENUI_GL_FUNCTIONS(XENUI_GL_DECLARE)
#undef XENUI_GL_DECLARE
    };

    using ProcLoader = void* (*)(const char* Name);

    /// @brief The process-wide table, filled by Load.
    extern Functions Api;

    /// @brief Loads every entry point through GetProc. Throws Error naming the first one missing.
    void Load(ProcLoader GetProc);

    [[nodiscard]] bool IsLoaded();
}  // namespace XenUI::GL
