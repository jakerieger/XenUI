//
// Created by conta on 3/21/2024.
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