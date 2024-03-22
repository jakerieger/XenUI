// Copyright (C) 2024 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

// Author: Jake Rieger
// Created: 3/21/2024.
//

#pragma once

#include "Types.h"
#include <stdarg.h>
#include <stdio.h>

namespace Xen::Error {
    inline void RuntimeError(const i64 code, const char* _restrict fmt, ...) {
        va_list args;
        va_start(args, fmt);
        fprintf(stderr, "(Error) ");
        vfprintf(stderr, fmt, args);
        va_end(args);
        exit(code);
    }
}  // namespace Xen::Error