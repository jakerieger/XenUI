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

#include "XenUI/Graphics/GL/GLObjects.hpp"
#include "XenUI/Graphics/GL/GLApi.hpp"

#include <mutex>
#include <vector>

namespace XenUI {
    namespace {
        struct PendingDelete {
            GLObjectKind Kind;
            uint32_t Name;
            uint64_t Generation;
        };

        struct DeleteQueue {
            std::mutex Mutex;
            std::vector<PendingDelete> Pending;
        };

        DeleteQueue& GetQueue() {
            static DeleteQueue Instance;
            return Instance;
        }
    }  // namespace

    void QueueGLDelete(GLObjectKind Kind, uint32_t Name, uint64_t Generation) {
        DeleteQueue& Queue = GetQueue();
        std::lock_guard Lock(Queue.Mutex);
        Queue.Pending.push_back({Kind, Name, Generation});
    }

    void FlushGLDeletes(uint64_t Generation) {
        std::vector<PendingDelete> Batch;
        {
            DeleteQueue& Queue = GetQueue();
            std::lock_guard Lock(Queue.Mutex);
            Batch.swap(Queue.Pending);
        }
        if (!GL::IsLoaded()) { return; }

        std::vector<GL::GLuint> Textures;
        std::vector<GL::GLuint> Framebuffers;
        for (const PendingDelete& Entry : Batch) {
            if (Entry.Generation != Generation) { continue; }
            (Entry.Kind == GLObjectKind::Texture ? Textures : Framebuffers).push_back(Entry.Name);
        }
        if (!Framebuffers.empty()) {
            GL::Api.DeleteFramebuffers(static_cast<GL::GLsizei>(Framebuffers.size()), Framebuffers.data());
        }
        if (!Textures.empty()) { GL::Api.DeleteTextures(static_cast<GL::GLsizei>(Textures.size()), Textures.data()); }
    }
}  // namespace XenUI
