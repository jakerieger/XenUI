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
/// Selects the per-backend GraphicsDevice: the process-wide GPU device, its factories and caches. Each backend
/// defines a non-copyable GraphicsDevice with at least Recreate() and GetGeneration(); the generation increases
/// whenever the device is rebuilt, so cached GPU resources can tell they are stale.

#if defined(_WIN32)
    #include "XenUI/Graphics/D2D/GraphicsDevice.hpp"
#else
    #include "XenUI/Graphics/GL/GraphicsDevice.hpp"
#endif
