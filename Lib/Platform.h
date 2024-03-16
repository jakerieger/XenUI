//
// Created by Jake Rieger on 3/12/2024.
//

#pragma once

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
// #pragma comment(lib, "winmm.lib")

#define NOMINMAX
#include <Windows.h>
#include <windowsx.h>
#include <d2d1.h>
#include <dwrite.h>

template<class Interface>
void SafeRelease(Interface** ppInterface) {
    if (*ppInterface != NULL) {
        (*ppInterface)->Release();
        (*ppInterface) = NULL;
    }
}