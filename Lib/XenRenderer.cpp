//
// Created by conta on 3/12/2024.
//

#include "XenRenderer.h"
#include "AppWindow.h"

namespace Xen {
    XenRenderer::~XenRenderer() {
        DiscardDeviceResources();
        SafeRelease(&D2DFactory);
        SafeRelease(&D2DRenderTarget);
    }

    void XenRenderer::Init() {
        if (OwningWindow == nullptr) {
            throw std::runtime_error("Renderer has no owning window");
        }

        if (FAILED(CreateDeviceIndependentResources())) {
            throw std::runtime_error("Failed to create Direct2D independent resources");
        }

        if (FAILED(CreateDeviceResources())) {
            throw std::runtime_error("Failed to create Direct2D resources");
        }

        // Create some brushes for testing
        ID2D1SolidColorBrush* redBrush;
        if (FAILED(D2DRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0xFF3366), &redBrush))) {
            throw std::runtime_error("Failed to create solid color brush");
        }

        SolidColorBrushes.push_back(redBrush);
    }

    HRESULT XenRenderer::Render() const {
        D2DRenderTarget->BeginDraw();
        D2DRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
        D2DRenderTarget->Clear(D2D1::ColorF(0x11131C));

        auto [width, height]   = D2DRenderTarget->GetSize();
        const D2D1_RECT_F rect = D2D1::RectF(width / 2 - 100.0f,
                                             height / 2 - 100.0f,
                                             width / 2 + 100.0f,
                                             height / 2 + 100.0f);
        D2DRenderTarget->FillRectangle(&rect, SolidColorBrushes.at(0));

        return D2DRenderTarget->EndDraw();
    }

    void XenRenderer::OnResize(u32 width, u32 height) {}

    void XenRenderer::SetOwner(AppWindow* owner) {
        OwningWindow = owner;
    }

    HRESULT XenRenderer::CreateDeviceResources() {
        HRESULT hr = S_OK;

        if (!D2DRenderTarget) {
            RECT rc;
            ::GetClientRect(OwningWindow->GetHandle(), &rc);
            const D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
            hr                     = D2DFactory->CreateHwndRenderTarget(
              D2D1::RenderTargetProperties(),
              D2D1::HwndRenderTargetProperties(OwningWindow->GetHandle(), size),
              &D2DRenderTarget);
        }

        return hr;
    }

    HRESULT XenRenderer::CreateDeviceIndependentResources() {
        HRESULT hr = S_OK;

        hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &D2DFactory);
        return hr;
    }

    void XenRenderer::DiscardDeviceResources() const {
        for (auto brush : SolidColorBrushes) {
            SafeRelease(&brush);
        }
    }
}  // namespace Xen