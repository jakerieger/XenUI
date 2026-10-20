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

#include "XenUI/Api.hpp"
#include "XenUI/Core/Math.hpp"
#include "XenUI/Core/Matrix.hpp"
#include "XenUI/Graphics/DisplayList.hpp"
#include "XenUI/Graphics/LiveCanvas.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace XenUI {
    class LayerImpl;

    /// @brief Retained node of a window's compositing tree: a DisplayList plus transform, clip and opacity.
    /// Every change invalidates only the screen area it affects. Must be used on the UI thread.
    /// @brief Where a native child window should sit, in window coordinates (device-independent pixels).
    struct NativePlacement {
        Rect Bounds;
        /// @brief The part of Bounds left after the clips of enclosing layers.
        Rect Visible;

        bool operator==(const NativePlacement&) const = default;
    };

    /// @brief Receives a native anchor's placement after each composited frame, or nothing when it is hidden.
    using NativePlacementCallback = std::function<void(const std::optional<NativePlacement>& Placement)>;

    class XENUI_API Layer {
    public:
        [[nodiscard]] static std::shared_ptr<Layer> Create();
        ~Layer();

        Layer(const Layer&)            = delete;
        Layer& operator=(const Layer&) = delete;

        void SetContent(DisplayList Content);
        [[nodiscard]] const DisplayList& GetContent() const;

        /// @brief Adds content drawn by Drawer with Direct3D into an offscreen surface of Extent, beneath the display
        /// list. Continuous content redraws every frame while visible; otherwise it redraws on InvalidateLive.
        void SetLiveContent(const Size& Extent, LiveDrawer Drawer, bool Continuous);
        void ClearLiveContent();

        /// @brief Marks this layer as the place of a native child window of Extent. After each frame the compositor
        /// reports where the window should be, accounting for transforms, clips and visibility of every ancestor.
        void SetNativeAnchor(const Size& Extent, NativePlacementCallback OnPlaced);
        void ClearNativeAnchor();
        void InvalidateLive();

        void SetOffset(const Vec2& Offset);
        [[nodiscard]] Vec2 GetOffset() const;

        /// @brief Applied in local space, before the offset.
        void SetTransform(const Matrix3x2& Transform);
        [[nodiscard]] const Matrix3x2& GetTransform() const;

        void SetOpacity(float Opacity);
        [[nodiscard]] float GetOpacity() const;

        /// @brief Clips content and children to a rectangle in local space. std::nullopt disables clipping.
        void SetClip(const std::optional<Rect>& Clip);
        [[nodiscard]] const std::optional<Rect>& GetClip() const;

        void SetVisible(bool Visible);
        [[nodiscard]] bool IsVisible() const;

        /// @brief Rasterizes this subtree into an offscreen bitmap that is reused until something inside it changes.
        /// Moving, fading or transforming the layer itself reuses the bitmap. Worthwhile for expensive content.
        void SetCached(bool Cached);
        [[nodiscard]] bool IsCached() const;

        /// @brief Appends Child, detaching it from any previous parent first.
        void AddChild(std::shared_ptr<Layer> Child);
        void RemoveChild(Layer& Child);

        /// @brief Replaces the child list. Does nothing when the list is unchanged.
        void SetChildren(const std::vector<std::shared_ptr<Layer>>& Children);
        void RemoveAllChildren();
        void RemoveFromParent();

        [[nodiscard]] const std::vector<std::shared_ptr<Layer>>& GetChildren() const;
        [[nodiscard]] Layer* GetParent() const;

        /// @internal
        [[nodiscard]] LayerImpl& GetImpl() const { return *_Impl; }

    private:
        Layer();

        std::unique_ptr<LayerImpl> _Impl;
    };
}  // namespace XenUI
