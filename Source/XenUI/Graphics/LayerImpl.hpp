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

#pragma once

#include "XenUI/Graphics/Layer.hpp"
#include "XenUI/Graphics/LiveRecording.hpp"

#include <d2d1_1.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace XenUI {
    class Compositor;

    struct LayerCache {
        Microsoft::WRL::ComPtr<ID2D1Bitmap1> Bitmap;
        Rect Bounds;
        float Scale          = 0.0f;
        uint32_t PixelWidth  = 0;
        uint32_t PixelHeight = 0;
        uint64_t Generation  = 0;
        bool Dirty           = true;
    };

    struct LiveSurface {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> Target;
        Microsoft::WRL::ComPtr<ID2D1Bitmap1> Bitmap;
        uint32_t PixelWidth  = 0;
        uint32_t PixelHeight = 0;
        float Scale          = 0.0f;
        uint64_t Generation  = 0;
    };

    struct LiveState {
        Size Extent;
        LiveDrawer Drawer;
        bool Continuous = false;
        bool Dirty      = true;
        LiveSurface Surface;
        LiveRecording Recording;
    };

    struct NativeAnchor {
        Size Extent;
        NativePlacementCallback OnPlaced;
        std::optional<NativePlacement> Last;
        Compositor* PlacedBy = nullptr;
        uint64_t Frame       = 0;
    };

    class LayerImpl {
    public:
        explicit LayerImpl(Layer& Owner);
        ~LayerImpl();

        LayerImpl(const LayerImpl&)            = delete;
        LayerImpl& operator=(const LayerImpl&) = delete;

        void SetContent(DisplayList Content);
        void SetLiveContent(const Size& Extent, LiveDrawer Drawer, bool Continuous);
        void ClearLiveContent();
        void InvalidateLive();
        void SetOffset(const Vec2& Offset);
        void SetTransform(const Matrix3x2& Transform);
        void SetOpacity(float Opacity);
        void SetClip(const std::optional<Rect>& Clip);
        void SetVisible(bool Visible);
        void SetCached(bool Cached);
        void AddChild(std::shared_ptr<Layer> Child);
        void RemoveChild(Layer& Child);
        void SetChildren(const std::vector<std::shared_ptr<Layer>>& Children);
        void RemoveAllChildren();
        void RemoveFromParent();

        void AttachHost(Compositor* Host);

        [[nodiscard]] Layer& GetOwner() const { return _Owner; }
        [[nodiscard]] LayerImpl* GetParent() const { return _Parent; }
        [[nodiscard]] const std::vector<std::shared_ptr<Layer>>& GetChildren() const { return _Children; }
        [[nodiscard]] const DisplayList& GetContent() const { return _Content; }
        [[nodiscard]] Vec2 GetOffset() const { return _Offset; }
        [[nodiscard]] const Matrix3x2& GetTransform() const { return _Transform; }
        [[nodiscard]] float GetOpacity() const { return _Opacity; }
        [[nodiscard]] const std::optional<Rect>& GetClip() const { return _Clip; }
        [[nodiscard]] bool IsVisible() const { return _Visible; }
        [[nodiscard]] bool IsCached() const { return _Cached; }

        /// @brief Maps local space into the parent's space.
        [[nodiscard]] Matrix3x2 GetLocalTransform() const;

        /// @brief Maps local space into window space.
        [[nodiscard]] Matrix3x2 GetWorldTransform() const;

        /// @brief Bounds of this layer's content and visible descendants in local space, after clipping.
        [[nodiscard]] const Rect& GetSubtreeBounds() const;

        [[nodiscard]] LayerCache& GetCache() { return _Cache; }
        [[nodiscard]] LiveState* GetLive() const { return _Live.get(); }

        void SetNativeAnchor(const Size& Extent, NativePlacementCallback OnPlaced);
        void ClearNativeAnchor();
        [[nodiscard]] NativeAnchor* GetNativeAnchor() const { return _Native.get(); }

        /// @brief Every layer with a native anchor, so compositors can hide anchors they no longer reach.
        [[nodiscard]] static const std::vector<LayerImpl*>& GetAnchoredLayers();

    private:
        void DamageSubtree();
        void DamageLocal(const Rect& LocalArea);
        void InvalidateBounds();
        void InvalidateCaches(bool IncludeSelf);
        void DetachChild(LayerImpl& Child);
        [[nodiscard]] Compositor* FindHost() const;

        Layer& _Owner;
        LayerImpl* _Parent = nullptr;
        Compositor* _Host  = nullptr;
        std::vector<std::shared_ptr<Layer>> _Children;

        DisplayList _Content;
        Vec2 _Offset;
        Matrix3x2 _Transform;
        float _Opacity = 1.0f;
        std::optional<Rect> _Clip;
        bool _Visible = true;
        bool _Cached  = false;

        mutable std::optional<Rect> _SubtreeBounds;
        LayerCache _Cache;
        std::unique_ptr<LiveState> _Live;
        std::unique_ptr<NativeAnchor> _Native;
    };
}  // namespace XenUI
