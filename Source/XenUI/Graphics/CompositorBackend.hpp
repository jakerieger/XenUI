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

/// @file
/// Selects the renderer that the shared Compositor draws through. Each backend defines the same non-virtual
/// CompositorBackend class:
///
/// - Frame: BeginFrame, WaitForFrame, BeginWindowDraw / EndWindowDraw, BeginDamage / EndDamage, Present.
/// - Layer caches: AllocateCache, ReleaseCache, BeginCacheDraw / EndCacheDraw, DrawCache.
/// - Live content: AllocateLiveSurface, ReleaseLiveSurface, RenderLive, DrawLiveSurface.
/// - Scene: PushOpacity / PopOpacity, PushClip / PopClip, DrawDisplayList, DrawFlash.
///
/// The shared code decides what to draw and when; a backend only knows how.

#if defined(_WIN32)
    #include "XenUI/Graphics/D2D/CompositorBackend.hpp"
#else
    #include "XenUI/Graphics/GL/CompositorBackend.hpp"
#endif
