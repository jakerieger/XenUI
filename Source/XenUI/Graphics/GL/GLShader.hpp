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

#include "XenUI/Core/Error.hpp"
#include "XenUI/Graphics/GL/GLApi.hpp"

#include <format>
#include <string>

namespace XenUI {
    namespace detail {
        inline GL::GLuint CompileShader(GL::GLenum Type, const char* Source, const char* Name) {
            const GL::GLuint Shader = GL::Api.CreateShader(Type);
            GL::Api.ShaderSource(Shader, 1, &Source, nullptr);
            GL::Api.CompileShader(Shader);
            GL::GLint Status = 0;
            GL::Api.GetShaderiv(Shader, GL::COMPILE_STATUS, &Status);
            if (Status == 0) {
                GL::GLint Length = 0;
                GL::Api.GetShaderiv(Shader, GL::INFO_LOG_LENGTH, &Length);
                std::string Log(static_cast<size_t>(std::max(Length, 1)), '\0');
                GL::Api.GetShaderInfoLog(Shader, Length, nullptr, Log.data());
                GL::Api.DeleteShader(Shader);
                throw Error(std::format("OpenGL: {} shader '{}' failed to compile: {}",
                                        Type == GL::VERTEX_SHADER ? "vertex" : "fragment",
                                        Name,
                                        Log.c_str()));
            }
            return Shader;
        }
    }  // namespace detail

    /// @brief Compiles and links a program. Throws Error with the driver's log on failure.
    inline GL::GLuint CompileProgram(const char* VertexSource, const char* FragmentSource, const char* Name) {
        const GL::GLuint Vertex   = detail::CompileShader(GL::VERTEX_SHADER, VertexSource, Name);
        const GL::GLuint Fragment = detail::CompileShader(GL::FRAGMENT_SHADER, FragmentSource, Name);
        const GL::GLuint Program  = GL::Api.CreateProgram();
        GL::Api.AttachShader(Program, Vertex);
        GL::Api.AttachShader(Program, Fragment);
        GL::Api.LinkProgram(Program);
        GL::Api.DeleteShader(Vertex);
        GL::Api.DeleteShader(Fragment);

        GL::GLint Status = 0;
        GL::Api.GetProgramiv(Program, GL::LINK_STATUS, &Status);
        if (Status == 0) {
            GL::GLint Length = 0;
            GL::Api.GetProgramiv(Program, GL::INFO_LOG_LENGTH, &Length);
            std::string Log(static_cast<size_t>(std::max(Length, 1)), '\0');
            GL::Api.GetProgramInfoLog(Program, Length, nullptr, Log.data());
            GL::Api.DeleteProgram(Program);
            throw Error(std::format("OpenGL: program '{}' failed to link: {}", Name, Log.c_str()));
        }
        return Program;
    }
}  // namespace XenUI
