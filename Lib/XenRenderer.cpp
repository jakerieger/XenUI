//
// Created by Jake Rieger on 3/12/2024.
//

#include "XenRenderer.h"
#include "AppWindow.h"
#include "UI/XenBox.h"
#include "UI/XenText.h"

namespace Xen::Renderer {
    ID2D1Factory* g_D2DFactory               = nullptr;
    IDWriteFactory* g_DWFactory              = nullptr;
    ID2D1HwndRenderTarget* g_D2DRenderTarget = nullptr;
    AppWindow* g_OwningWindow                = nullptr;

    // TEST RESOURCES
    XenText* demoText = nullptr;
    XenBox* demoBox   = nullptr;

    HRESULT CreateDeviceResources() {
        HRESULT hr = S_OK;

        if (!g_D2DRenderTarget) {
            RECT rc;
            ::GetClientRect(g_OwningWindow->GetHandle(), &rc);
            const D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
            hr                     = g_D2DFactory->CreateHwndRenderTarget(
              D2D1::RenderTargetProperties(),
              D2D1::HwndRenderTargetProperties(g_OwningWindow->GetHandle(), size),
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

    void CreateTestResources() {
        auto [width, height] = g_D2DRenderTarget->GetSize();
        demoText = new XenText("Click Me", "Inter", Offset(width / 2.f, height / 2.f), 400, 16.f);
        demoBox  = new XenBox(Rect::FromCenter(Offset(width / 2.f, height / 2.f), 200, 64),
                             Color(0xFFFF3366));
    }

    void DiscardDeviceResources() {
        SafeRelease(&g_D2DFactory);
        SafeRelease(&g_D2DRenderTarget);
        SafeRelease(&g_DWFactory);
    }

    void Init() {
        if (g_OwningWindow == nullptr) {
            throw std::runtime_error("Renderer has no owning window");
        }

        if (FAILED(CreateDeviceIndependentResources())) {
            throw std::runtime_error("Failed to create Direct2D independent resources");
        }

        if (FAILED(CreateDeviceResources())) {
            throw std::runtime_error("Failed to create Direct2D resources");
        }

        CreateTestResources();
    }

    int Render() {
        g_D2DRenderTarget->BeginDraw();
        g_D2DRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
        g_D2DRenderTarget->Clear(D2D1::ColorF(0x11131C));

        demoBox->Draw();
        auto [width, height] = g_D2DRenderTarget->GetSize();
        demoBox->UpdateShape(Rect::FromCenter(Offset(width / 2.f, height / 2.f), 200, 64));

        demoText->Draw();

        // auto [width, height] = D2DRenderTarget->GetSize();
        // testBox->UpdateShape(Rect::FromCenter(Offset(width / 2.f, height / 2.f), 200, 64));
        // testBox->Draw();
        //
        // D2DRenderTarget->DrawText(L"Click Me",
        //                           wcslen(L"Click Me"),
        //                           TextFormat,
        //                           D2D1::RectF(0, 0, 200, 100),
        //                           testBox->GetBrush());

        return g_D2DRenderTarget->EndDraw();
    }

    void Shutdown() {
        DiscardDeviceResources();
        g_OwningWindow = nullptr;
    }

    void OnResize(const u32 width, const u32 height) {
        if (const auto hr = g_D2DRenderTarget->Resize(D2D1_SIZE_U(width, height)); FAILED(hr)) {
            throw std::runtime_error("Failed to resize Direct2D render target!");
        }
    }

    void CheckHit(Offset<i64>& mousePos) {}

    void SetOwningWindow(AppWindow* owner) { g_OwningWindow = owner; }

    ID2D1Factory* GetD2DFactory() { return g_D2DFactory; }
    IDWriteFactory* GetDWFactory() { return g_DWFactory; }
    ID2D1HwndRenderTarget* GetRenderTarget() { return g_D2DRenderTarget; }
    AppWindow* GetOwningWindow() { return g_OwningWindow; }
}  // namespace Xen::Renderer