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

#include <cstdint>
#include <utility>

namespace XenUI {
    enum class GLObjectKind : uint8_t { Texture, Framebuffer };

    /// @brief Queues Name for deletion the next time the device's context is current. Names from an older device
    /// generation are dropped, since their context is gone.
    void QueueGLDelete(GLObjectKind Kind, uint32_t Name, uint64_t Generation);

    /// @brief Deletes everything queued for Generation. Call with the context current.
    void FlushGLDeletes(uint64_t Generation);

    /// @brief Owns one GL object name. Objects die wherever their owner does (often with no context current), so
    /// deletion is deferred to FlushGLDeletes.
    template<GLObjectKind Kind>
    class GLObject {
    public:
        GLObject() = default;
        GLObject(uint32_t Name, uint64_t Generation) : _Name(Name), _Generation(Generation) {}
        ~GLObject() { Reset(); }

        GLObject(const GLObject&)            = delete;
        GLObject& operator=(const GLObject&) = delete;

        GLObject(GLObject&& Other) noexcept
            : _Name(std::exchange(Other._Name, 0)), _Generation(std::exchange(Other._Generation, 0)) {}

        GLObject& operator=(GLObject&& Other) noexcept {
            if (this != &Other) {
                Reset();
                _Name       = std::exchange(Other._Name, 0);
                _Generation = std::exchange(Other._Generation, 0);
            }
            return *this;
        }

        void Reset() {
            if (_Name != 0) { QueueGLDelete(Kind, _Name, _Generation); }
            _Name       = 0;
            _Generation = 0;
        }

        [[nodiscard]] uint32_t Get() const { return _Name; }
        [[nodiscard]] uint64_t GetGeneration() const { return _Generation; }
        explicit operator bool() const { return _Name != 0; }

    private:
        uint32_t _Name       = 0;
        uint64_t _Generation = 0;
    };

    using GLTexture     = GLObject<GLObjectKind::Texture>;
    using GLFramebuffer = GLObject<GLObjectKind::Framebuffer>;
}  // namespace XenUI
