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

#ifdef XENUI_EXPORTS
    #define XENUI_API __declspec(dllexport)
#else
    #define XENUI_API __declspec(dllimport)
#endif

/// @note C4251 and C4275 fire for std:: members and header-only template bases of exported classes. XenUI requires
/// consumers to use the same MSVC toolset and CRT, so these never cross an ABI boundary that could disagree on layout.
#pragma warning(disable : 4251 4275)
