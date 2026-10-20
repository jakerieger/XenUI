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

#include "XenUI/Graphics/SwapChain.hpp"
#include "XenUI/Graphics/DeviceLost.hpp"

#include <utility>

namespace XenUI {
    namespace {
        constexpr UINT kSwapChainFlags     = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
        constexpr DWORD kFrameWaitTimeout  = 1000;
        constexpr DXGI_FORMAT kPixelFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
    }  // namespace

    SwapChain::SwapChain(GraphicsDevice& Device, HWND Hwnd, float Dpi) : _Dpi(Dpi) {
        const DXGI_SWAP_CHAIN_DESC1 Desc {
          .Format      = kPixelFormat,
          .SampleDesc  = {.Count = 1},
          .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
          .BufferCount = kBufferCount,
          .Scaling     = DXGI_SCALING_NONE,
          .SwapEffect  = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL,
          .AlphaMode   = DXGI_ALPHA_MODE_IGNORE,
          .Flags       = kSwapChainFlags,
        };

        IDXGIFactory2* Factory = Device.GetDxgiFactory();
        ComPtr<IDXGISwapChain1> BaseSwapChain;
        CheckDeviceResult(
          Factory->CreateSwapChainForHwnd(Device.GetD3DDevice(), Hwnd, &Desc, nullptr, nullptr, &BaseSwapChain),
          "IDXGIFactory2::CreateSwapChainForHwnd");
        ThrowIfFailed(BaseSwapChain.As(&_SwapChain), "Query IDXGISwapChain2");
        Factory->MakeWindowAssociation(Hwnd, DXGI_MWA_NO_ALT_ENTER);

        ThrowIfFailed(_SwapChain->SetMaximumFrameLatency(1), "IDXGISwapChain2::SetMaximumFrameLatency");
        _FrameLatencyWaitable = _SwapChain->GetFrameLatencyWaitableObject();

        DXGI_SWAP_CHAIN_DESC1 ActualDesc {};
        _SwapChain->GetDesc1(&ActualDesc);
        _Width  = ActualDesc.Width;
        _Height = ActualDesc.Height;

        CheckDeviceResult(Device.GetD2DDevice()->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &_Context),
                          "ID2D1Device::CreateDeviceContext");
        _Context->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        CreateTargetBitmap();
    }

    SwapChain::~SwapChain() {
        if (_Context) { _Context->SetTarget(nullptr); }
        if (_FrameLatencyWaitable != nullptr) { CloseHandle(_FrameLatencyWaitable); }
    }

    void SwapChain::Resize(uint32_t Width, uint32_t Height) {
        if (Width == 0 || Height == 0 || (Width == _Width && Height == _Height)) { return; }

        _Context->SetTarget(nullptr);
        _TargetBitmap.Reset();

        CheckDeviceResult(_SwapChain->ResizeBuffers(0, Width, Height, DXGI_FORMAT_UNKNOWN, kSwapChainFlags),
                          "IDXGISwapChain::ResizeBuffers");
        _Width  = Width;
        _Height = Height;

        CreateTargetBitmap();
    }

    void SwapChain::SetDpi(float Dpi) {
        if (Dpi == _Dpi) { return; }
        _Dpi = Dpi;

        _Context->SetTarget(nullptr);
        _TargetBitmap.Reset();
        CreateTargetBitmap();
    }

    void SwapChain::WaitForFrame() {
        if (!std::exchange(_FrameReady, false)) {
            WaitForSingleObjectEx(_FrameLatencyWaitable, kFrameWaitTimeout, TRUE);
        }
    }

    bool SwapChain::Present(const std::vector<RECT>& DirtyRects) {
        DXGI_PRESENT_PARAMETERS Parameters {};
        if (!DirtyRects.empty()) {
            Parameters.DirtyRectsCount = static_cast<UINT>(DirtyRects.size());
            Parameters.pDirtyRects     = const_cast<RECT*>(DirtyRects.data());
        }

        const HRESULT Result = _SwapChain->Present1(1, 0, &Parameters);
        CheckDeviceResult(Result, "IDXGISwapChain1::Present1");
        return Result == S_OK;
    }

    void SwapChain::CreateTargetBitmap() {
        ComPtr<IDXGISurface> Surface;
        CheckDeviceResult(_SwapChain->GetBuffer(0, IID_PPV_ARGS(&Surface)), "IDXGISwapChain::GetBuffer");

        const D2D1_BITMAP_PROPERTIES1 Properties =
          D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
                                  D2D1::PixelFormat(kPixelFormat, D2D1_ALPHA_MODE_PREMULTIPLIED),
                                  _Dpi,
                                  _Dpi);
        CheckDeviceResult(_Context->CreateBitmapFromDxgiSurface(Surface.Get(), &Properties, &_TargetBitmap),
                          "ID2D1DeviceContext::CreateBitmapFromDxgiSurface");
        _Context->SetTarget(_TargetBitmap.Get());
    }
}  // namespace XenUI
