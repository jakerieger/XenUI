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

#include "XenUI/Graphics/D2D/CompositorBackend.hpp"
#include "XenUI/Graphics/D2D/D2DConvert.hpp"
#include "XenUI/Graphics/D2D/D2DRenderer.hpp"
#include "XenUI/Graphics/D2D/GraphicsDevice.hpp"
#include "XenUI/Graphics/D2D/LiveRenderer.hpp"
#include "XenUI/Graphics/D2D/SwapChain.hpp"
#include "XenUI/Graphics/DeviceLost.hpp"
#include "XenUI/Graphics/DisplayList.hpp"
#include "XenUI/Graphics/LiveRecording.hpp"

namespace XenUI {
    CompositorBackend::~CompositorBackend() = default;

    void CompositorBackend::BeginFrame(SwapChain& Target, GraphicsDevice& Device) {
        _Target   = &Target;
        _Device   = &Device;
        _Context  = Target.GetContext();
        _DpiScale = Target.GetDpi() / static_cast<float>(USER_DEFAULT_SCREEN_DPI);
    }

    uint64_t CompositorBackend::GetGeneration() const {
        return _Device->GetGeneration();
    }

    size_t CompositorBackend::GetBufferCount() const {
        return SwapChain::kBufferCount;
    }

    uint32_t CompositorBackend::GetPixelWidth() const {
        return _Target->GetWidth();
    }

    uint32_t CompositorBackend::GetPixelHeight() const {
        return _Target->GetHeight();
    }

    Size CompositorBackend::GetViewport() const {
        const D2D1_SIZE_F Viewport = _Context->GetSize();
        return {Viewport.width, Viewport.height};
    }

    void CompositorBackend::WaitForFrame() {
        _Target->WaitForFrame();
    }

    void CompositorBackend::BeginWindowDraw() {
        _Context->SetTarget(_Target->GetTargetBitmap());
        _Context->BeginDraw();
    }

    void CompositorBackend::EndWindowDraw() {
        CheckDeviceResult(_Context->EndDraw(), "ID2D1DeviceContext::EndDraw");
    }

    void CompositorBackend::BeginDamage(const Rect& Area, const Color& Background) {
        _Context->SetTransform(D2D1::IdentityMatrix());
        _Context->PushAxisAlignedClip(ToD2D(Area), D2D1_ANTIALIAS_MODE_ALIASED);
        _Context->Clear(ToD2D(Background));
    }

    void CompositorBackend::EndDamage() {
        _Context->PopAxisAlignedClip();
    }

    bool CompositorBackend::Present(const std::vector<PixelRect>& Changed) {
        std::vector<RECT> DirtyRects;
        DirtyRects.reserve(Changed.size());
        for (const PixelRect& Pixels : Changed) {
            DirtyRects.push_back({Pixels.Left, Pixels.Top, Pixels.Right, Pixels.Bottom});
        }
        return _Target->Present(DirtyRects);
    }

    void CompositorBackend::AllocateCache(LayerCache& Cache, uint32_t PixelWidth, uint32_t PixelHeight, float Scale) {
        Cache.Bitmap.Reset();
        const float CacheDpi = static_cast<float>(USER_DEFAULT_SCREEN_DPI) * Scale;
        const D2D1_BITMAP_PROPERTIES1 Properties =
          D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET,
                                  D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
                                  CacheDpi,
                                  CacheDpi);
        CheckDeviceResult(
          _Context->CreateBitmap(D2D1::SizeU(PixelWidth, PixelHeight), nullptr, 0, &Properties, &Cache.Bitmap),
          "ID2D1DeviceContext::CreateBitmap");
        Cache.PixelWidth  = PixelWidth;
        Cache.PixelHeight = PixelHeight;
        Cache.Generation  = _Device->GetGeneration();
        Cache.Scale       = Scale;
    }

    void CompositorBackend::ReleaseCache(LayerCache& Cache) {
        Cache.Bitmap.Reset();
    }

    void CompositorBackend::BeginCacheDraw(LayerCache& Cache) {
        _Context->SetTarget(Cache.Bitmap.Get());
        _Context->BeginDraw();
        _Context->SetTransform(D2D1::IdentityMatrix());
        _Context->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
    }

    void CompositorBackend::EndCacheDraw() {
        CheckDeviceResult(_Context->EndDraw(), "ID2D1DeviceContext::EndDraw");
    }

    void CompositorBackend::DrawCache(const LayerCache& Cache, const Matrix3x2& World, float Opacity) {
        const D2D1_RECT_F Destination = ToD2D(Cache.Bounds);
        _Context->SetTransform(ToD2D(World));
        _Context->DrawBitmap(Cache.Bitmap.Get(), &Destination, Opacity, D2D1_INTERPOLATION_MODE_LINEAR, nullptr);
    }

    void CompositorBackend::AllocateLiveSurface(LiveSurface& Surface,
                                                uint32_t PixelWidth,
                                                uint32_t PixelHeight,
                                                float Scale) {
        Surface = {};
        const D3D11_TEXTURE2D_DESC Desc {
          .Width      = PixelWidth,
          .Height     = PixelHeight,
          .MipLevels  = 1,
          .ArraySize  = 1,
          .Format     = DXGI_FORMAT_B8G8R8A8_UNORM,
          .SampleDesc = {.Count = 1},
          .Usage      = D3D11_USAGE_DEFAULT,
          .BindFlags  = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
        };
        ID3D11Device& D3D = *_Device->GetD3DDevice();
        CheckDeviceResult(D3D.CreateTexture2D(&Desc, nullptr, &Surface.Texture), "ID3D11Device::CreateTexture2D");
        CheckDeviceResult(D3D.CreateRenderTargetView(Surface.Texture.Get(), nullptr, &Surface.Target),
                          "ID3D11Device::CreateRenderTargetView");

        Microsoft::WRL::ComPtr<IDXGISurface> Shared;
        CheckDeviceResult(Surface.Texture.As(&Shared), "Query IDXGISurface");
        const float SurfaceDpi = static_cast<float>(USER_DEFAULT_SCREEN_DPI) * Scale;
        const D2D1_BITMAP_PROPERTIES1 Properties =
          D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_NONE,
                                  D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
                                  SurfaceDpi,
                                  SurfaceDpi);
        CheckDeviceResult(_Context->CreateBitmapFromDxgiSurface(Shared.Get(), &Properties, &Surface.Bitmap),
                          "ID2D1DeviceContext::CreateBitmapFromDxgiSurface");
        Surface.PixelWidth  = PixelWidth;
        Surface.PixelHeight = PixelHeight;
        Surface.Scale       = Scale;
        Surface.Generation  = _Device->GetGeneration();
    }

    void CompositorBackend::ReleaseLiveSurface(LiveSurface& Surface) {
        Surface = {};
    }

    void CompositorBackend::RenderLive(LiveSurface& Surface,
                                       const Size& Extent,
                                       float Scale,
                                       const LiveRecording& Recording) {
        _Device->GetLiveRenderer().Render(*Surface.Target.Get(), Extent, Scale, Recording);
    }

    void CompositorBackend::DrawLiveSurface(const LiveSurface& Surface, const Size& Extent, const Matrix3x2& World) {
        const D2D1_RECT_F Destination {0.0f, 0.0f, Extent.Width, Extent.Height};
        _Context->SetTransform(ToD2D(World));
        _Context->DrawBitmap(Surface.Bitmap.Get(), &Destination, 1.0f, D2D1_INTERPOLATION_MODE_LINEAR, nullptr);
    }

    void CompositorBackend::PushOpacity(float Opacity) {
        _Context->PushLayer(D2D1::LayerParameters1(D2D1::InfiniteRect(),
                                                   nullptr,
                                                   D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
                                                   D2D1::IdentityMatrix(),
                                                   Opacity),
                            nullptr);
    }

    void CompositorBackend::PopOpacity() {
        _Context->PopLayer();
    }

    void CompositorBackend::PushClip(const Rect& Local, const Matrix3x2& World) {
        _Context->SetTransform(ToD2D(World));
        _Context->PushAxisAlignedClip(ToD2D(Local), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    }

    void CompositorBackend::PopClip() {
        _Context->PopAxisAlignedClip();
    }

    void CompositorBackend::DrawDisplayList(const DisplayList& Content, const Matrix3x2& World) {
        ReplayDisplayList(*_Context, *_Device, Content, World);
    }

    void CompositorBackend::DrawFlash(const Rect& Area, const Color& Fill, const Color& Outline) {
        _Context->SetTransform(D2D1::IdentityMatrix());
        D2DResources& Resources = _Device->GetResources();
        _Context->FillRectangle(ToD2D(Area), Resources.GetBrush(*_Context, Fill));
        _Context->DrawRectangle(ToD2D(Area.Inflated(-0.5f)), Resources.GetBrush(*_Context, Outline));
    }
}  // namespace XenUI
