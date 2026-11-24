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

#include "XenUI/Graphics/GL/GLApi.hpp"
#include "XenUI/Core/Error.hpp"

#include <format>

namespace XenUI::GL {
    Functions Api;

    void Load(ProcLoader GetProc) {
        Functions Loaded;
#define XENUI_GL_LOAD(Return, Name, Arguments)                                                                        \
    Loaded.Name = reinterpret_cast<Return(*) Arguments>(GetProc("gl" #Name));                                         \
    if (Loaded.Name == nullptr) { throw Error(std::format("OpenGL: gl{} is not available", #Name)); }
        XENUI_GL_FUNCTIONS(XENUI_GL_LOAD)
#undef XENUI_GL_LOAD
        Api = Loaded;
    }

    bool IsLoaded() {
        return Api.GetString != nullptr;
    }
}  // namespace XenUI::GL
