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
#include "XenUI/Graphics/CompositorBackend.hpp"
#include "XenUI/Graphics/Layer.hpp"
#include "XenUI/Graphics/LayerImpl.hpp"

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

    Compositor::Compositor(std::function<void()> RequestFrame)
        : _RequestFrame(std::move(RequestFrame)), _Backend(std::make_unique<CompositorBackend>()) {}

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

    void Compositor::SetFrameBorder(const std::optional<Color>& Border) {
        if (Border == _FrameBorder) { return; }
        _FrameBorder = Border;
        AddFullDamage();
    }

    void Compositor::SetShowDamage(bool Enabled) {
        if (Enabled == _ShowDamage) { return; }
        _ShowDamage = Enabled;
        AddFullDamage();
    }

    void Compositor::Render(SwapChain& Target, GraphicsDevice& Device, const Color& Background) {
        _Backend->BeginFrame(Target, Device);
        const float DpiScale       = _Backend->GetDpiScale();
        const size_t HistoryLength = _Backend->GetBufferCount() - 1;

        _LiveActive = false;
        if (_Root) { PrepareLiveLayers(_Root->GetImpl(), Matrix3x2::Identity()); }

        DamageRegion Changed = _Pending;
        UpdateFlashes(Changed);

        DamageRegion Repaint = Changed;
        for (const DamageRegion& Previous : _History) {
            Repaint.Merge(Previous);
        }
        if (_History.size() < HistoryLength) { Repaint.AddAll(); }

        const std::vector<PixelRect> RepaintPixels = ToPixelRects(Repaint);
        const std::vector<PixelRect> ChangedPixels =
          Changed.IsFull() ? std::vector<PixelRect> {} : ToPixelRects(Changed);

        using Clock        = std::chrono::steady_clock;
        const auto Elapsed = [](Clock::time_point From, Clock::time_point To) {
            return std::chrono::duration<float, std::milli>(To - From).count();
        };
        const auto WaitStart = Clock::now();
        _Backend->WaitForFrame();
        const auto DrawStart = Clock::now();

        if (_Root) { UpdateCaches(_Root->GetImpl(), Matrix3x2::Identity()); }

        _Backend->BeginWindowDraw();
        for (const PixelRect& Pixels : RepaintPixels) {
            const Rect Area = Rect::FromEdges(static_cast<float>(Pixels.Left) / DpiScale,
                                              static_cast<float>(Pixels.Top) / DpiScale,
                                              static_cast<float>(Pixels.Right) / DpiScale,
                                              static_cast<float>(Pixels.Bottom) / DpiScale);

            _Backend->BeginDamage(Area, Background);
            if (_Root) { DrawLayer(_Root->GetImpl(), Matrix3x2::Identity(), Area); }
            DrawFlashes(Area);
            if (_FrameBorder) {
                const Size Viewport = _Backend->GetViewport();
                _Backend->DrawFlash({0.0f, 0.0f, Viewport.Width, Viewport.Height},
                                    _FrameBorder->WithAlpha(0.0f),
                                    *_FrameBorder);
            }
            _Backend->EndDamage();
        }
        _Backend->EndWindowDraw();

        const auto PresentStart      = Clock::now();
        const bool PresentedNormally = _Backend->Present(ChangedPixels);
        const auto PresentEnd        = Clock::now();

        float Area = 0.0f;
        for (const PixelRect& Pixels : ChangedPixels) {
            Area += static_cast<float>(Pixels.Area());
        }
        const float WindowArea =
          static_cast<float>(_Backend->GetPixelWidth()) * static_cast<float>(_Backend->GetPixelHeight());
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

        if ((!_Flashes.empty() || _LiveActive) && _RequestFrame) { _RequestFrame(); }
    }

    void Compositor::PrepareLiveLayers(LayerImpl& Node, const Matrix3x2& ParentWorld) {
        if (!Node.IsVisible()) { return; }

        const Matrix3x2 World = Node.GetLocalTransform() * ParentWorld;
        if (LiveState* Live = Node.GetLive()) {
            const LiveSurface& Surface = Live->Surface;
            const bool Stale           = !Surface.IsAllocated() || Surface.Generation != _Backend->GetGeneration() ||
                               Surface.Scale != World.GetMaxScale() * _Backend->GetDpiScale();
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

        const float Scale      = World.GetMaxScale() * _Backend->GetDpiScale();
        const auto PixelWidth  = static_cast<uint32_t>(std::ceil(Live.Extent.Width * Scale));
        const auto PixelHeight = static_cast<uint32_t>(std::ceil(Live.Extent.Height * Scale));
        LiveSurface& Surface   = Live.Surface;
        if (PixelWidth == 0 || PixelHeight == 0 || PixelWidth > kMaxCacheDimension ||
            PixelHeight > kMaxCacheDimension) {
            _Backend->ReleaseLiveSurface(Surface);
            return;
        }

        const uint64_t Generation = _Backend->GetGeneration();
        if (!Surface.IsAllocated() || Surface.PixelWidth != PixelWidth || Surface.PixelHeight != PixelHeight ||
            Surface.Generation != Generation || Surface.Scale != Scale) {
            _Backend->AllocateLiveSurface(Surface, PixelWidth, PixelHeight, Scale);
        }

        Live.Recording.Clear();
        if (Live.Drawer) {
            LiveCanvas Canvas(Live.Recording, Live.Extent);
            Live.Drawer(Canvas);
        }
        _Backend->RenderLive(Surface, Live.Extent, Scale, Live.Recording);
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
            const Size Viewport = _Backend->GetViewport();
            AddFlash({0.0f, 0.0f, Viewport.Width, Viewport.Height});
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
        const float Scale         = World.GetMaxScale() * _Backend->GetDpiScale();
        const uint64_t Generation = _Backend->GetGeneration();
        const bool UpToDate = Cache.IsAllocated() && !Cache.Dirty && Cache.Generation == Generation &&
                              Cache.Scale == Scale && Cache.Bounds == Bounds;
        if (UpToDate) { return; }

        const auto PixelWidth  = static_cast<uint32_t>(std::ceil(Bounds.Width * Scale));
        const auto PixelHeight = static_cast<uint32_t>(std::ceil(Bounds.Height * Scale));
        if (Bounds.IsEmpty() || PixelWidth == 0 || PixelHeight == 0 || PixelWidth > kMaxCacheDimension ||
            PixelHeight > kMaxCacheDimension) {
            _Backend->ReleaseCache(Cache);
            return;
        }

        const bool NeedsBitmap = !Cache.IsAllocated() || Cache.PixelWidth != PixelWidth ||
                                 Cache.PixelHeight != PixelHeight || Cache.Generation != Generation ||
                                 Cache.Scale != Scale;
        if (NeedsBitmap) { _Backend->AllocateCache(Cache, PixelWidth, PixelHeight, Scale); }

        _Backend->BeginCacheDraw(Cache);
        DrawLayerContents(Node,
                          Matrix3x2::Translation(-Bounds.X, -Bounds.Y),
                          {0.0f, 0.0f, Bounds.Width, Bounds.Height});
        _Backend->EndCacheDraw();

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
        if (Node.IsCached() && Cache.IsAllocated() && !Cache.Dirty && Cache.Generation == _Backend->GetGeneration()) {
            _Backend->DrawCache(Cache, World, Node.GetOpacity());
            return;
        }

        const bool Translucent = Node.GetOpacity() < 1.0f;
        if (Translucent) { _Backend->PushOpacity(Node.GetOpacity()); }
        DrawLayerContents(Node, World, Damage);
        if (Translucent) { _Backend->PopOpacity(); }
    }

    void Compositor::DrawLayerContents(LayerImpl& Node, const Matrix3x2& World, const Rect& Damage) {
        const std::optional<Rect>& Clip = Node.GetClip();
        if (Clip) { _Backend->PushClip(*Clip, World); }

        if (const LiveState* Live = Node.GetLive(); Live != nullptr && Live->Surface.IsAllocated()) {
            _Backend->DrawLiveSurface(Live->Surface, Live->Extent, World);
        }

        const DisplayList& Content = Node.GetContent();
        if (!Content.IsEmpty() && World.TransformRect(Content.GetBounds()).Intersects(Damage)) {
            _Backend->DrawDisplayList(Content, World);
        }

        for (const std::shared_ptr<Layer>& Child : Node.GetChildren()) {
            DrawLayer(Child->GetImpl(), World, Damage);
        }

        if (Clip) { _Backend->PopClip(); }
    }

    void Compositor::DrawFlashes(const Rect& Area) {
        if (_Flashes.empty()) { return; }

        for (const Flash& Entry : _Flashes) {
            if (!Entry.Area.Intersects(Area)) { continue; }
            const float Fade = 1.0f - static_cast<float>(Entry.Age) / static_cast<float>(kFlashFrames);
            _Backend->DrawFlash(Entry.Area, Entry.Tint.WithAlpha(kFlashOpacity * Fade), Entry.Tint.WithAlpha(Fade));
        }
    }

    std::vector<PixelRect> Compositor::ToPixelRects(const DamageRegion& Region) const {
        const auto MaxX      = static_cast<int32_t>(_Backend->GetPixelWidth());
        const auto MaxY      = static_cast<int32_t>(_Backend->GetPixelHeight());
        const float DpiScale = _Backend->GetDpiScale();
        if (Region.IsFull()) { return {PixelRect {0, 0, MaxX, MaxY}}; }

        std::vector<PixelRect> Result;
        Result.reserve(Region.GetRects().size());
        for (const Rect& Area : Region.GetRects()) {
            const PixelRect Pixels {
              std::clamp(static_cast<int32_t>(std::floor(Area.Left() * DpiScale)), 0, MaxX),
              std::clamp(static_cast<int32_t>(std::floor(Area.Top() * DpiScale)), 0, MaxY),
              std::clamp(static_cast<int32_t>(std::ceil(Area.Right() * DpiScale)), 0, MaxX),
              std::clamp(static_cast<int32_t>(std::ceil(Area.Bottom() * DpiScale)), 0, MaxY),
            };
            if (Pixels.Right > Pixels.Left && Pixels.Bottom > Pixels.Top) { Result.push_back(Pixels); }
        }
        return Result;
    }
}  // namespace XenUI
