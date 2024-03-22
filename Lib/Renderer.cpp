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

#include "Renderer.h"
#include "RenderTree.h"
#include "XenApp.h"
#include "UI/Box.h"

#include <chrono>

namespace Xen {
    class Box;
}
namespace Xen::Renderer {
    ID2D1Factory* g_D2DFactory               = nullptr;
    IDWriteFactory* g_DWFactory              = nullptr;
    ID2D1HwndRenderTarget* g_D2DRenderTarget = nullptr;
    XenApp* g_OwningApp                      = nullptr;
    std::chrono::time_point g_StartTime      = std::chrono::steady_clock::now();

    HRESULT
    CreateDeviceResources() {
        HRESULT hr = S_OK;

        if (!g_D2DRenderTarget) {
            RECT rc;
            ::GetClientRect(g_OwningApp->GetNativeWindow()->GetHandle(), &rc);
            const D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
            hr                     = g_D2DFactory->CreateHwndRenderTarget(
              D2D1::RenderTargetProperties(),
              D2D1::HwndRenderTargetProperties(g_OwningApp->GetNativeWindow()->GetHandle(), size),
              &g_D2DRenderTarget);
        }

        return hr;
    }

    HRESULT CreateDeviceIndependentResources() {
        HRESULT hr = S_OK;

        hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_D2DFactory);
        if (SUCCEEDED(hr)) {
            hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                                     __uuidof(IDWriteFactory),
                                     reinterpret_cast<IUnknown**>(&g_DWFactory));
        }

        return hr;
    }

    void DiscardDeviceResources() {
        SafeRelease(&g_D2DFactory);
        SafeRelease(&g_D2DRenderTarget);
        SafeRelease(&g_DWFactory);
    }

    void Init() {
        if (g_OwningApp->GetNativeWindow() == nullptr) {
            throw std::runtime_error("Renderer has no owning window");
        }

        if (FAILED(CreateDeviceIndependentResources())) {
            throw std::runtime_error("Failed to create Direct2D independent resources");
        }

        if (FAILED(CreateDeviceResources())) {
            throw std::runtime_error("Failed to create Direct2D resources");
        }
    }

    int Render() {
        g_D2DRenderTarget->BeginDraw();
        g_D2DRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
        g_D2DRenderTarget->Clear(g_OwningApp->GetContext().AppTheme.WindowBackground.GetD2DColor());

        RenderTree::Cleanup();
        RenderTree::RebuildUI();
        RenderTree::Render();

        return g_D2DRenderTarget->EndDraw();
    }

    void Shutdown() {
        DiscardDeviceResources();
        RenderTree::Cleanup();
    }

    void OnResize(const u32 width, const u32 height) {
        if (const auto hr = g_D2DRenderTarget->Resize(D2D1_SIZE_U(width, height)); FAILED(hr)) {
            throw std::runtime_error("Failed to resize Direct2D render target!");
        }

        // Rebuild UI
        RenderTree::RebuildUI();
    }

    void CheckHit(const Offset& mousePos) {
        auto currentElement   = RenderTree::GetRoot();
        Element* foundElement = nullptr;
        while (currentElement != nullptr) {
            if (currentElement->GetSize().Contains(mousePos)) {
                if (const auto asInteractive = currentElement->As<Interactive>();
                    asInteractive != nullptr) {
                    if (foundElement == nullptr) {
                        foundElement = currentElement;
                    } else {
                        if (currentElement->GetZIndex() > foundElement->GetZIndex()) {
                            foundElement = currentElement;
                        }
                    }
                }
            }

            currentElement = currentElement->GetChild();
        }

        if (foundElement) {
            foundElement->As<Interactive>()->OnPressed();
        }
    }

    void CheckOverlap(const Offset& mousePos) {}

    void SetOwner(XenApp* owner) { g_OwningApp = owner; }

    Offset GetRenderTargetCenter() {
        auto [width, height] = g_D2DRenderTarget->GetSize();
        return {width / 2.f, height / 2.f};
    }

    u64 GetTicks() {
        const auto currentTime = std::chrono::steady_clock::now();
        const auto duration =
          std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - g_StartTime).count();
        return duration;
    }

    ID2D1Factory* GetD2DFactory() { return g_D2DFactory; }
    IDWriteFactory* GetDWFactory() { return g_DWFactory; }
    ID2D1HwndRenderTarget* GetRenderTarget() { return g_D2DRenderTarget; }
    AppWindow* GetOwningWindow() { return g_OwningApp->GetNativeWindow(); }
}  // namespace Xen::Renderer