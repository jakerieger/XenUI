//
// Created by Jake Rieger on 3/12/2024.
//

#include "XenRenderer.h"
#include "AppWindow.h"
#include "UI/Color.h"

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

        auto [width, height] = D2DRenderTarget->GetSize();

        testBox = new Box(Rect::FromCenter(Offset(width / 2.f, height / 2.f), 100, 32),
                          Colors::Magenta,
                          D2DRenderTarget);
    }

    HRESULT XenRenderer::Render() const {
        D2DRenderTarget->BeginDraw();
        D2DRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
        D2DRenderTarget->Clear(D2D1::ColorF(0x11131C));

        auto [width, height] = D2DRenderTarget->GetSize();
        testBox->UpdateShape(Rect::FromCenter(Offset(width / 2.f, height / 2.f), 200, 64));
        testBox->Draw();

        return D2DRenderTarget->EndDraw();
    }

    void XenRenderer::OnResize(const u32 width, const u32 height) const {
        if (const auto hr = D2DRenderTarget->Resize(D2D1_SIZE_U(width, height)); FAILED(hr)) {
            throw std::runtime_error("Failed to resize Direct2D render target!");
        }
    }

    void XenRenderer::SetOwner(AppWindow* owner) { OwningWindow = owner; }

    void XenRenderer::CheckHit(Offset<i64>& mousePos) const {
        if (testBox->GetShape().Contains(mousePos.To<f32>())) {
            MessageBoxA(OwningWindow->GetHandle(), "Box clicked", "", MB_OK);
        }
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

    void XenRenderer::DiscardDeviceResources() const { delete testBox; }
}  // namespace Xen