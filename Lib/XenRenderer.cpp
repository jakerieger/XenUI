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

        // Create some brushes for testing
        ID2D1SolidColorBrush* redBrush;
        if (FAILED(
              D2DRenderTarget->CreateSolidColorBrush(Color(0xFFFF3366).GetD2DColor(), &redBrush))) {
            throw std::runtime_error("Failed to create solid color brush");
        }

        SolidColorBrushes.push_back(redBrush);
    }

    HRESULT XenRenderer::Render() {
        D2DRenderTarget->BeginDraw();
        D2DRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
        D2DRenderTarget->Clear(D2D1::ColorF(0x11131C));

        auto [width, height] = D2DRenderTarget->GetSize();
        testRect             = Rect::FromCircle(Offset(width / 2.f, height / 2.f), 48);
        auto boundingRect    = testRect.Grow(24);
        D2DRenderTarget->FillRectangle(testRect.GetD2DRect(), SolidColorBrushes.at(0));
        D2DRenderTarget->DrawRectangle(boundingRect.GetD2DRect(), SolidColorBrushes.at(0));

        return D2DRenderTarget->EndDraw();
    }

    void XenRenderer::OnResize(const u32 width, const u32 height) const {
        if (const auto hr = D2DRenderTarget->Resize(D2D1_SIZE_U(width, height)); FAILED(hr)) {
            throw std::runtime_error("Failed to resize Direct2D render target!");
        }
    }

    void XenRenderer::SetOwner(AppWindow* owner) { OwningWindow = owner; }

    void XenRenderer::CheckHit(Offset<i64>& mousePos) const {
        if (testRect.Contains(mousePos.To<f32>())) {
            std::cout << "Box clicked!\n";
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

    void XenRenderer::DiscardDeviceResources() const {
        for (auto brush : SolidColorBrushes) {
            SafeRelease(&brush);
        }
    }
}  // namespace Xen