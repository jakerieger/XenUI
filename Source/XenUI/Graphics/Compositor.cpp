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

#include "XenUI/Graphics/Compositor.hpp"
#include "XenUI/Graphics/D2DConvert.hpp"
#include "XenUI/Graphics/D2DRenderer.hpp"
#include "XenUI/Graphics/DeviceLost.hpp"
#include "XenUI/Graphics/GraphicsDevice.hpp"
#include "XenUI/Graphics/Layer.hpp"
#include "XenUI/Graphics/LayerImpl.hpp"
#include "XenUI/Graphics/LiveRenderer.hpp"
#include "XenUI/Graphics/SwapChain.hpp"
#include "XenUI/Platform/WindowImpl.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iterator>

namespace XenUI {
    namespace {
        constexpr uint32_t kMaxCacheDimension = 8192;
        constexpr uint32_t kFlashFrames       = 20;
        constexpr float kFlashOpacity         = 0.2f;
        constexpr float kFlashMergeOverlap    = 0.5f;
        constexpr uint32_t kFlashPalette[]    = {0xFF4D6A, 0x4DD2FF, 0x7CFF4D, 0xFFC94D, 0xB84DFF, 0x4DFFB8};
    }  // namespace

    Compositor::Compositor(WindowImpl& Owner) : _Owner(Owner) {}

    Compositor::~Compositor() {
        if (_Root) { _Root->GetImpl().AttachHost(nullptr); }
    }

    void Compositor::SetRoot(std::shared_ptr<Layer> Root) {
        if (_Root) { _Root->GetImpl().AttachHost(nullptr); }
        if (Root) { Root->GetImpl().RemoveFromParent(); }

        _Root = std::move(Root);
        if (_Root) { _Root->GetImpl().AttachHost(this); }
        AddFullDamage();
    }

    void Compositor::AddDamage(const Rect& Area) {
        if (Area.IsEmpty()) { return; }
        _Pending.Add(Area);
    }

    void Compositor::AddFullDamage() {
        _Pending.AddAll();
    }

    void Compositor::ResetBufferHistory() {
        _History.clear();
        AddFullDamage();
    }

    void Compositor::SetShowDamage(bool Enabled) {
        if (Enabled == _ShowDamage) { return; }
        _ShowDamage = Enabled;
        AddFullDamage();
    }

    void Compositor::Render(SwapChain& Target, GraphicsDevice& Device, const Color& Background) {
        _Context  = Target.GetContext();
        _Device   = &Device;
        _DpiScale = Target.GetDpi() / static_cast<float>(USER_DEFAULT_SCREEN_DPI);

        const size_t HistoryLength = SwapChain::kBufferCount - 1;

        _LiveActive = false;
        if (_Root) { PrepareLiveLayers(_Root->GetImpl(), Matrix3x2::Identity()); }

        DamageRegion Changed = _Pending;
        UpdateFlashes(Changed);

        DamageRegion Repaint = Changed;
        for (const DamageRegion& Previous : _History) {
            Repaint.Merge(Previous);
        }
        if (_History.size() < HistoryLength) { Repaint.AddAll(); }

        const std::vector<RECT> RepaintPixels = ToPixelRects(Repaint, Target.GetWidth(), Target.GetHeight());
        const std::vector<RECT> ChangedPixels =
          Changed.IsFull() ? std::vector<RECT> {} : ToPixelRects(Changed, Target.GetWidth(), Target.GetHeight());

        using Clock        = std::chrono::steady_clock;
        const auto Elapsed = [](Clock::time_point From, Clock::time_point To) {
            return std::chrono::duration<float, std::milli>(To - From).count();
        };
        const auto WaitStart = Clock::now();
        Target.WaitForFrame();
        const auto DrawStart = Clock::now();

        if (_Root) {
            UpdateCaches(_Root->GetImpl(), Matrix3x2::Identity());
            _Context->SetTarget(Target.GetTargetBitmap());
        }

        _Context->BeginDraw();
        for (const RECT& Pixels : RepaintPixels) {
            const Rect Area = Rect::FromEdges(static_cast<float>(Pixels.left) / _DpiScale,
                                              static_cast<float>(Pixels.top) / _DpiScale,
                                              static_cast<float>(Pixels.right) / _DpiScale,
                                              static_cast<float>(Pixels.bottom) / _DpiScale);

            _Context->SetTransform(D2D1::IdentityMatrix());
            _Context->PushAxisAlignedClip(ToD2D(Area), D2D1_ANTIALIAS_MODE_ALIASED);
            _Context->Clear(ToD2D(Background));
            if (_Root) { DrawLayer(_Root->GetImpl(), Matrix3x2::Identity(), Area); }
            DrawFlashes(Area);
            _Context->PopAxisAlignedClip();
        }
        CheckDeviceResult(_Context->EndDraw(), "ID2D1DeviceContext::EndDraw");

        const auto PresentStart      = Clock::now();
        const bool PresentedNormally = Target.Present(ChangedPixels);
        const auto PresentEnd        = Clock::now();

        float Area = 0.0f;
        for (const RECT& Pixels : ChangedPixels) {
            Area += static_cast<float>((Pixels.right - Pixels.left) * (Pixels.bottom - Pixels.top));
        }
        const float WindowArea = static_cast<float>(Target.GetWidth()) * static_cast<float>(Target.GetHeight());
        _LastTimings           = {
          .WaitMs    = Elapsed(WaitStart, DrawStart),
          .DrawMs    = Elapsed(DrawStart, PresentStart),
          .PresentMs = Elapsed(PresentStart, PresentEnd),
          .Damage    = Changed.IsFull() || WindowArea <= 0.0f ? 1.0f : std::min(1.0f, Area / WindowArea),
        };

        ++_PlacementFrame;
        if (_Root) { PlaceNativeAnchors(_Root->GetImpl(), Matrix3x2::Identity(), std::nullopt); }
        HideUnplacedAnchors();

        _History.insert(_History.begin(), std::move(Changed));
        if (_History.size() > HistoryLength) { _History.resize(HistoryLength); }
        if (!PresentedNormally) { _History.clear(); }
        _Pending.Clear();

        if (!_Flashes.empty() || _LiveActive) { _Owner.RequestFrame(); }
    }

    void Compositor::PrepareLiveLayers(LayerImpl& Node, const Matrix3x2& ParentWorld) {
        if (!Node.IsVisible()) { return; }

        const Matrix3x2 World = Node.GetLocalTransform() * ParentWorld;
        if (LiveState* Live = Node.GetLive()) {
            const LiveSurface& Surface = Live->Surface;
            const bool Stale           = !Surface.Bitmap || Surface.Generation != _Device->GetGeneration() ||
                                         Surface.Scale != World.GetMaxScale() * _DpiScale;
            if (Live->Continuous || Live->Dirty || Stale) {
                Live->Dirty = true;
                AddDamage(World.TransformRect({0.0f, 0.0f, Live->Extent.Width, Live->Extent.Height}));
            }
            _LiveActive = _LiveActive || Live->Continuous;
        }

        for (const std::shared_ptr<Layer>& Child : Node.GetChildren()) {
            PrepareLiveLayers(Child->GetImpl(), World);
        }
    }

    void Compositor::RenderLive(LayerImpl& Node, const Matrix3x2& World) {
        LiveState& Live = *Node.GetLive();
        if (!Live.Dirty) { return; }

        const float Scale      = World.GetMaxScale() * _DpiScale;
        const auto PixelWidth  = static_cast<uint32_t>(std::ceil(Live.Extent.Width * Scale));
        const auto PixelHeight = static_cast<uint32_t>(std::ceil(Live.Extent.Height * Scale));
        LiveSurface& Surface   = Live.Surface;
        if (PixelWidth == 0 || PixelHeight == 0 || PixelWidth > kMaxCacheDimension ||
            PixelHeight > kMaxCacheDimension) {
            Surface = {};
            return;
        }

        const uint64_t Generation = _Device->GetGeneration();
        if (!Surface.Bitmap || Surface.PixelWidth != PixelWidth || Surface.PixelHeight != PixelHeight ||
            Surface.Generation != Generation || Surface.Scale != Scale) {
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
            Surface.Generation  = Generation;
        }

        Live.Recording.Clear();
        if (Live.Drawer) {
            LiveCanvas Canvas(Live.Recording, Live.Extent);
            Live.Drawer(Canvas);
        }
        _Device->GetLiveRenderer().Render(*Surface.Target.Get(), Live.Extent, Scale, Live.Recording);
        Live.Dirty = false;
    }

    void Compositor::UpdateFlashes(DamageRegion& Changed) {
        for (const Flash& Entry : _Flashes) {
            Changed.Add(Entry.Area);
        }

        if (!_ShowDamage) {
            _Flashes.clear();
            return;
        }

        for (Flash& Entry : _Flashes) {
            ++Entry.Age;
        }
        std::erase_if(_Flashes, [](const Flash& Entry) { return Entry.Age >= kFlashFrames; });

        const auto AddFlash = [this](const Rect& Area) {
            for (Flash& Entry : _Flashes) {
                if (Entry.Area.ContainsRect(Area)) {
                    if (Area.ContainsRect(Entry.Area)) { Entry.Age = 0; }
                    return;
                }
                const float Overlap = Entry.Area.Intersection(Area).Area();
                if (Overlap >= kFlashMergeOverlap * std::min(Entry.Area.Area(), Area.Area())) {
                    Entry.Area = Entry.Area.Union(Area);
                    Entry.Age  = 0;
                    return;
                }
            }
            const uint32_t Tint = kFlashPalette[_FlashCounter++ % std::size(kFlashPalette)];
            _Flashes.push_back({.Area = Area, .Tint = Color::FromHex(Tint)});
        };

        if (_Pending.IsFull()) {
            const D2D1_SIZE_F Viewport = _Context->GetSize();
            AddFlash({0.0f, 0.0f, Viewport.width, Viewport.height});
        } else {
            for (const Rect& Area : _Pending.GetRects()) {
                AddFlash(Area);
            }
        }
    }

    void Compositor::UpdateCaches(LayerImpl& Node, const Matrix3x2& ParentWorld) {
        if (!Node.IsVisible()) { return; }

        const Matrix3x2 World = Node.GetLocalTransform() * ParentWorld;
        for (const std::shared_ptr<Layer>& Child : Node.GetChildren()) {
            UpdateCaches(Child->GetImpl(), World);
        }
        if (Node.GetLive() != nullptr) { RenderLive(Node, World); }

        LayerCache& Cache = Node.GetCache();
        if (!Node.IsCached()) { return; }

        const Rect& Bounds        = Node.GetSubtreeBounds();
        const float Scale         = World.GetMaxScale() * _DpiScale;
        const uint64_t Generation = _Device->GetGeneration();
        const bool UpToDate = Cache.Bitmap && !Cache.Dirty && Cache.Generation == Generation && Cache.Scale == Scale &&
                              Cache.Bounds == Bounds;
        if (UpToDate) { return; }

        const auto PixelWidth  = static_cast<uint32_t>(std::ceil(Bounds.Width * Scale));
        const auto PixelHeight = static_cast<uint32_t>(std::ceil(Bounds.Height * Scale));
        if (Bounds.IsEmpty() || PixelWidth == 0 || PixelHeight == 0 || PixelWidth > kMaxCacheDimension ||
            PixelHeight > kMaxCacheDimension) {
            Cache.Bitmap.Reset();
            return;
        }

        const bool NeedsBitmap = !Cache.Bitmap || Cache.PixelWidth != PixelWidth || Cache.PixelHeight != PixelHeight ||
                                 Cache.Generation != Generation || Cache.Scale != Scale;
        if (NeedsBitmap) {
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
            Cache.Generation  = Generation;
            Cache.Scale       = Scale;
        }

        _Context->SetTarget(Cache.Bitmap.Get());
        _Context->BeginDraw();
        _Context->SetTransform(D2D1::IdentityMatrix());
        _Context->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
        DrawLayerContents(Node,
                          Matrix3x2::Translation(-Bounds.X, -Bounds.Y),
                          {0.0f, 0.0f, Bounds.Width, Bounds.Height});
        CheckDeviceResult(_Context->EndDraw(), "ID2D1DeviceContext::EndDraw");

        Cache.Bounds = Bounds;
        Cache.Dirty  = false;
    }

    void Compositor::PlaceNativeAnchors(LayerImpl& Node,
                                        const Matrix3x2& ParentWorld,
                                        const std::optional<Rect>& ParentClip) {
        if (!Node.IsVisible() || Node.GetOpacity() <= 0.0f) { return; }
        const Matrix3x2 World = Node.GetLocalTransform() * ParentWorld;

        if (NativeAnchor* Anchor = Node.GetNativeAnchor()) {
            const Rect Bounds  = World.TransformRect({0.0f, 0.0f, Anchor->Extent.Width, Anchor->Extent.Height});
            const Rect Visible = ParentClip ? Bounds.Intersection(*ParentClip) : Bounds;
            std::optional<NativePlacement> Placement;
            if (!Visible.IsEmpty()) { Placement = NativePlacement {Bounds, Visible}; }
            Anchor->PlacedBy = this;
            Anchor->Frame    = _PlacementFrame;
            if (Placement != Anchor->Last) {
                Anchor->Last = Placement;
                if (Anchor->OnPlaced) { Anchor->OnPlaced(Placement); }
            }
        }

        std::optional<Rect> Clip = ParentClip;
        if (const std::optional<Rect>& Local = Node.GetClip()) {
            const Rect WorldClip = World.TransformRect(*Local);
            Clip                 = Clip ? Clip->Intersection(WorldClip) : WorldClip;
        }
        for (const std::shared_ptr<Layer>& Child : Node.GetChildren()) {
            PlaceNativeAnchors(Child->GetImpl(), World, Clip);
        }
    }

    void Compositor::HideUnplacedAnchors() {
        const std::vector<LayerImpl*> Anchored = LayerImpl::GetAnchoredLayers();
        for (LayerImpl* Layer : Anchored) {
            NativeAnchor* Anchor = Layer->GetNativeAnchor();
            if (Anchor == nullptr || Anchor->PlacedBy != this || Anchor->Frame == _PlacementFrame) { continue; }
            Anchor->PlacedBy = nullptr;
            if (Anchor->Last) {
                Anchor->Last.reset();
                if (Anchor->OnPlaced) { Anchor->OnPlaced(std::nullopt); }
            }
        }
    }

    void Compositor::DrawLayer(LayerImpl& Node, const Matrix3x2& ParentWorld, const Rect& Damage) {
        if (!Node.IsVisible() || Node.GetOpacity() <= 0.0f) { return; }

        const Matrix3x2 World = Node.GetLocalTransform() * ParentWorld;
        const Rect& Bounds    = Node.GetSubtreeBounds();
        if (Bounds.IsEmpty() || !World.TransformRect(Bounds).Intersects(Damage)) { return; }

        const LayerCache& Cache = Node.GetCache();
        if (Node.IsCached() && Cache.Bitmap && !Cache.Dirty && Cache.Generation == _Device->GetGeneration()) {
            const D2D1_RECT_F Destination = ToD2D(Cache.Bounds);
            _Context->SetTransform(ToD2D(World));
            _Context->DrawBitmap(Cache.Bitmap.Get(),
                                 &Destination,
                                 Node.GetOpacity(),
                                 D2D1_INTERPOLATION_MODE_LINEAR,
                                 nullptr);
            return;
        }

        const bool Translucent = Node.GetOpacity() < 1.0f;
        if (Translucent) {
            _Context->PushLayer(D2D1::LayerParameters1(D2D1::InfiniteRect(),
                                                       nullptr,
                                                       D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
                                                       D2D1::IdentityMatrix(),
                                                       Node.GetOpacity()),
                                nullptr);
        }
        DrawLayerContents(Node, World, Damage);
        if (Translucent) { _Context->PopLayer(); }
    }

    void Compositor::DrawLayerContents(LayerImpl& Node, const Matrix3x2& World, const Rect& Damage) {
        const std::optional<Rect>& Clip = Node.GetClip();
        if (Clip) {
            _Context->SetTransform(ToD2D(World));
            _Context->PushAxisAlignedClip(ToD2D(*Clip), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        }

        if (const LiveState* Live = Node.GetLive(); Live != nullptr && Live->Surface.Bitmap) {
            const D2D1_RECT_F Destination {0.0f, 0.0f, Live->Extent.Width, Live->Extent.Height};
            _Context->SetTransform(ToD2D(World));
            _Context->DrawBitmap(Live->Surface.Bitmap.Get(),
                                 &Destination,
                                 1.0f,
                                 D2D1_INTERPOLATION_MODE_LINEAR,
                                 nullptr);
        }

        const DisplayList& Content = Node.GetContent();
        if (!Content.IsEmpty() && World.TransformRect(Content.GetBounds()).Intersects(Damage)) {
            ReplayDisplayList(*_Context, *_Device, Content, World);
        }

        for (const std::shared_ptr<Layer>& Child : Node.GetChildren()) {
            DrawLayer(Child->GetImpl(), World, Damage);
        }

        if (Clip) { _Context->PopAxisAlignedClip(); }
    }

    void Compositor::DrawFlashes(const Rect& Area) {
        if (_Flashes.empty()) { return; }

        _Context->SetTransform(D2D1::IdentityMatrix());
        D2DResources& Resources = _Device->GetResources();
        for (const Flash& Entry : _Flashes) {
            if (!Entry.Area.Intersects(Area)) { continue; }
            const float Fade = 1.0f - static_cast<float>(Entry.Age) / static_cast<float>(kFlashFrames);
            _Context->FillRectangle(ToD2D(Entry.Area),
                                    Resources.GetBrush(*_Context, Entry.Tint.WithAlpha(kFlashOpacity * Fade)));
            _Context->DrawRectangle(ToD2D(Entry.Area.Inflated(-0.5f)),
                                    Resources.GetBrush(*_Context, Entry.Tint.WithAlpha(Fade)));
        }
    }

    std::vector<RECT> Compositor::ToPixelRects(const DamageRegion& Region, uint32_t Width, uint32_t Height) const {
        const LONG MaxX = static_cast<LONG>(Width);
        const LONG MaxY = static_cast<LONG>(Height);
        if (Region.IsFull()) { return {RECT {0, 0, MaxX, MaxY}}; }

        std::vector<RECT> Result;
        Result.reserve(Region.GetRects().size());
        for (const Rect& Area : Region.GetRects()) {
            const RECT Pixels {
              std::clamp(static_cast<LONG>(std::floor(Area.Left() * _DpiScale)), 0L, MaxX),
              std::clamp(static_cast<LONG>(std::floor(Area.Top() * _DpiScale)), 0L, MaxY),
              std::clamp(static_cast<LONG>(std::ceil(Area.Right() * _DpiScale)), 0L, MaxX),
              std::clamp(static_cast<LONG>(std::ceil(Area.Bottom() * _DpiScale)), 0L, MaxY),
            };
            if (Pixels.right > Pixels.left && Pixels.bottom > Pixels.top) { Result.push_back(Pixels); }
        }
        return Result;
    }
}  // namespace XenUI
